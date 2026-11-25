#include "Events.h"

#include <atomic>

#include <SKSE/SKSE.h>

#include "Animations.h"
#include "Controller.h"
#include "Height.h"
#include "Speed.h"


namespace Events
{
	namespace
	{
		enum class GraphRegisterStatus
		{
			kRegistered,
			kAlreadyRegistered,
			kNotReady
		};

		std::atomic_int g_pendingGraphRetries{ 0 };
		std::atomic_int g_deferGraphFrames{ 0 };

		GraphRegisterStatus TryRegisterAnimationGraph(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink)
		{
			if (!a_sink) {
				return GraphRegisterStatus::kNotReady;
			}

			const auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return GraphRegisterStatus::kNotReady;
			}

			RE::BSAnimationGraphManagerPtr graphManager;
			if (!player->GetAnimationGraphManager(graphManager) || !graphManager || graphManager->graphs.empty()) {
				return GraphRegisterStatus::kNotReady;
			}

			const auto& frontGraph = graphManager->graphs.front();
			if (!frontGraph) {
				return GraphRegisterStatus::kNotReady;
			}

			const auto eventSource = frontGraph->GetEventSource<RE::BSAnimationGraphEvent>();
			if (!eventSource) {
				return GraphRegisterStatus::kNotReady;
			}

			for (const auto& sink : eventSource->sinks) {
				if (sink == a_sink) {
					return GraphRegisterStatus::kAlreadyRegistered;
				}
			}

			eventSource->AddEventSink(a_sink);
			SKSE::log::info("Registered animation event handler");
			return GraphRegisterStatus::kRegistered;
		}
	}


	bool AnimationGraphEventHandler(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink)
	{
		return TryRegisterAnimationGraph(a_sink) == GraphRegisterStatus::kRegistered;
	}


	void CancelPendingGraphRegistration() noexcept
	{
		g_deferGraphFrames.store(0, std::memory_order_release);
		g_pendingGraphRetries.store(0, std::memory_order_release);
	}


	void ScheduleAnimationGraphRegistration(const int a_retriesLeft, const int a_deferFrames)
	{
		if (a_deferFrames <= 0) {
			const auto status = TryRegisterAnimationGraph(BHopHandler::GetSingleton());
			if (status != GraphRegisterStatus::kNotReady) {
				CancelPendingGraphRegistration();
				return;
			}
			g_deferGraphFrames.store(0, std::memory_order_release);
			g_pendingGraphRetries.store(a_retriesLeft, std::memory_order_release);
			return;
		}

		g_deferGraphFrames.store(a_deferFrames, std::memory_order_release);
		g_pendingGraphRetries.store(a_retriesLeft, std::memory_order_release);
	}


	void ProcessPendingAnimationGraphRegistration()
	{
		if (g_deferGraphFrames.load(std::memory_order_acquire) > 0) {
			g_deferGraphFrames.fetch_sub(1, std::memory_order_acq_rel);
			return;
		}

		const int retries = g_pendingGraphRetries.load(std::memory_order_acquire);
		if (retries <= 0) {
			return;
		}

		if (TryRegisterAnimationGraph(BHopHandler::GetSingleton()) != GraphRegisterStatus::kNotReady) {
			g_pendingGraphRetries.store(0, std::memory_order_release);
			return;
		}

		if (g_pendingGraphRetries.fetch_sub(1, std::memory_order_acq_rel) == 1) {
			SKSE::log::warn("Player animation graph did not become ready before retry limit");
		}
	}


	auto BHopHandler::ProcessEvent(
		const RE::BSAnimationGraphEvent*                                a_event,
		[[maybe_unused]] RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource)
		-> EventResult
	{
		if (!a_event || !a_event->holder || !a_event->holder->IsPlayerRef()) {
			return EventResult::kContinue;
		}

		auto* controller = Controller::GetSingleton();
		if (safeStartCountdown > 0) {
			--safeStartCountdown;
			controller->HaltProcess();
			return EventResult::kContinue;
		}

		if (const auto* player = RE::PlayerCharacter::GetSingleton(); player && player->IsOnMount()) {
			if (SpeedController::GetSingleton()->GetBaseSpeed() >= 0.0f ||
				HeightController::GetSingleton()->GetBaseJumpHeight() >= 0.0f) {
				controller->HaltProcess();
			}
			return EventResult::kContinue;
		}

		switch (HashAnimation(a_event->tag)) {
		case Anim::kUp:
			controller->OnJumping();
			break;
		case Anim::kFall:
		case Anim::kFallDirectional:
			controller->OnFalling();
			break;
		case Anim::kDown:
			controller->OnLanding();
			break;
		case Anim::kLandEnd:
		case Anim::kFootLeft:
		case Anim::kFootRight:
			controller->OnGround();
			break;
		case Anim::kGraphDeleting:
			controller->OnDelete();
			ScheduleAnimationGraphRegistration(180, 1);
			break;
		default:
			break;
		}

		return EventResult::kContinue;
	}


	auto LoadGameHandler::ProcessEvent(
		const RE::TESLoadGameEvent*                                a_event,
		[[maybe_unused]] RE::BSTEventSource<RE::TESLoadGameEvent>* a_eventSource)
		-> EventResult
	{
		if (!a_event) {
			return EventResult::kContinue;
		}

		BHopHandler::GetSingleton()->ResetCountdown();
		Controller::GetSingleton()->HaltProcess();
		return EventResult::kContinue;
	}
}
