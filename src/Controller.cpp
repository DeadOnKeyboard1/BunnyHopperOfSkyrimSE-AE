#include "Controller.h"

#include <algorithm>
#include <chrono>

#include "Crouch.h"
#include "Events.h"
#include "Height.h"
#include "Ram.h"
#include "Settings.h"
#include "Speed.h"
#include "Strafe.h"
#include "Visual.h"


namespace
{
	[[nodiscard]] std::int64_t NowMs() noexcept
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch())
			.count();
	}
}


auto Controller::GetSingleton()
-> Controller*
{
	static Controller singleton;
	return std::addressof(singleton);
}


void Controller::RecordCompletedHop() noexcept
{
	if (streakTracker.CompleteHop()) {
		Settings::hopHighscore.Set(static_cast<std::int32_t>(streakTracker.GetHighscore()));
		highscoreDirty.store(true, std::memory_order_release);
	}
}


void Controller::SyncHighscoreFromSettings() noexcept
{
	const auto loaded = Settings::hopHighscore.Get();
	const auto clamped = loaded > 0 ? loaded : 0;
	if (clamped != loaded) {
		Settings::hopHighscore.Set(clamped);
	}
	streakTracker.SetHighscore(clamped);
	highscoreDirty.store(false, std::memory_order_release);
}


void Controller::FlushHighscore()
{
	if (highscoreDirty.exchange(false, std::memory_order_acq_rel)) {
		(void)Settings::SaveHighscore();
	}
}


void Controller::ResetHighscore()
{
	streakTracker.ResetHighscore();
	Settings::hopHighscore.Set(0);
	highscoreDirty.store(false, std::memory_order_release);
	(void)Settings::SaveHighscore();
}


void Controller::OnJumping()
{
	groundedSinceMs.store(0, std::memory_order_release);
	stationarySinceMs.store(0, std::memory_order_release);

	HeightController::GetSingleton()->EnsurePos();
	StrafeController::GetSingleton()->TryStrafe();

	streakTracker.OnJumping();
	ResetCounter();
}


void Controller::OnFalling()
{
	groundedSinceMs.store(0, std::memory_order_release);
	stationarySinceMs.store(0, std::memory_order_release);

	auto* height = HeightController::GetSingleton();
	auto* ram = RamController::GetSingleton();
	auto* speed = SpeedController::GetSingleton();
	auto* strafe = StrafeController::GetSingleton();

	height->GainHeightBonus();

	ram->TestRam();

	speed->SpeedUp();

	strafe->CalcDipChange();
	strafe->GainStrafeBonud();
}


void Controller::OnLanding()
{
	std::int64_t expectedZero = 0;
	groundedSinceMs.compare_exchange_strong(expectedZero, NowMs(), std::memory_order_acq_rel);

	auto* crouch = CrouchController::GetSingleton();
	auto* height = HeightController::GetSingleton();
	auto* ram = RamController::GetSingleton();
	auto* strafe = StrafeController::GetSingleton();
	auto* visual = VisualController::GetSingleton();

	RecordCompletedHop();
	crouch->GainCrouchBonus();

	height->CalcHeightDiff();
	height->InitState(height->GetLastState());

	ram->TestRam();

	strafe->CalcDipChange();
	strafe->GainStrafeBonud();

	visual->ApplyEffect();
}


void Controller::OnGround()
{
	const auto allowedMissteps = static_cast<unsigned>((std::max)(0, *Settings::misttepAllowed));
	if (++stopCounter >= allowedMissteps) {
		ResetCounter();
		HaltProcess();
		return;
	}

	std::int64_t expectedZero = 0;
	groundedSinceMs.compare_exchange_strong(expectedZero, NowMs(), std::memory_order_acq_rel);

	RecordCompletedHop();

	auto* height = HeightController::GetSingleton();
	auto* ram = RamController::GetSingleton();

	height->CalcHeightDiff();
	height->InitState(height->GetLastState());

	ram->TestRam();
}


void Controller::OnFrameTick()
{
	auto*      player = RE::PlayerCharacter::GetSingleton();
	auto*      speed = SpeedController::GetSingleton();
	auto*      height = HeightController::GetSingleton();
	const bool chainActive =
		speed->GetBaseSpeed() >= 0.0f ||
		speed->GetAppliedBonus() != 0.0f ||
		height->GetBaseJumpHeight() >= 0.0f ||
		streakTracker.GetCurrentStreak() > 0;

	auto*       ui = RE::UI::GetSingleton();
	const auto* actorState = player ? player->AsActorState() : nullptr;
	const auto  isBlockingMenuOpen = [ui]() {
		if (!ui) {
			return false;
		}
		static constexpr std::array kBlockingMenus{
			RE::LoadingMenu::MENU_NAME,
			RE::MapMenu::MENU_NAME,
			RE::JournalMenu::MENU_NAME,
			RE::DialogueMenu::MENU_NAME,
			RE::TweenMenu::MENU_NAME,
			RE::SleepWaitMenu::MENU_NAME,
			RE::MainMenu::MENU_NAME,
		};
		return std::any_of(kBlockingMenus.begin(), kBlockingMenus.end(), [ui](const std::string_view a_name) {
			return ui->IsMenuOpen(a_name);
		});
	};
	const bool unsafeOrMenu =
		!player ||
		player->IsDead() ||
		player->IsOnMount() ||
		player->IsInRagdollState() ||
		player->IsInKillMove() ||
		(actorState && (actorState->IsSwimming() ||
						actorState->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal)) ||
		isBlockingMenuOpen();

	const bool inMidair = player && !unsafeOrMenu && player->IsInMidair();
	if (inMidair) {
		groundedSinceMs.store(0, std::memory_order_release);
		stationarySinceMs.store(0, std::memory_order_release);
		if (chainActive) {
			RamController::GetSingleton()->TestRam();
		}
	}

	if (!chainActive) {
		groundedSinceMs.store(0, std::memory_order_release);
		stationarySinceMs.store(0, std::memory_order_release);
		return;
	}

	const auto nowMs = NowMs();
	bool       isMoving = false;
	if (actorState) {
		const auto& state1 = actorState->actorState1;
		isMoving = state1.movingForward || state1.movingBack || state1.movingLeft || state1.movingRight;
	}

	std::int64_t groundedElapsedMs = 0;
	std::int64_t stationaryElapsedMs = 0;
	if (!inMidair && !unsafeOrMenu) {
		std::int64_t expectedZero = 0;
		groundedSinceMs.compare_exchange_strong(expectedZero, nowMs, std::memory_order_acq_rel);
		const auto groundedStart = groundedSinceMs.load(std::memory_order_acquire);
		if (groundedStart > 0 && nowMs >= groundedStart) {
			groundedElapsedMs = nowMs - groundedStart;
		}

		if (!isMoving) {
			std::int64_t expectedStatZero = 0;
			stationarySinceMs.compare_exchange_strong(expectedStatZero, nowMs, std::memory_order_acq_rel);
			const auto statStart = stationarySinceMs.load(std::memory_order_acquire);
			if (statStart > 0 && nowMs >= statStart) {
				stationaryElapsedMs = nowMs - statStart;
			}
		} else {
			stationarySinceMs.store(0, std::memory_order_release);
		}
	}

	if (EvaluateGroundChainState(
			chainActive,
			unsafeOrMenu,
			inMidair,
			isMoving,
			groundedElapsedMs,
			stationaryElapsedMs) == FrameGuardAction::kReset) {
		HaltProcess();
	}
}


void Controller::OnDelete()
{
	HaltProcess();
	VisualController::GetSingleton()->Halt();
}


void Controller::OnPreLoad()
{
	groundedSinceMs.store(0, std::memory_order_release);
	stationarySinceMs.store(0, std::memory_order_release);

	FlushHighscore();
	streakTracker.Invalidate();

	Events::BHopHandler::GetSingleton()->ResetCountdown();
	CrouchController::GetSingleton()->Invalidate();
	HeightController::GetSingleton()->Invalidate();
	RamController::GetSingleton()->Invalidate();
	SpeedController::GetSingleton()->Invalidate();
	StrafeController::GetSingleton()->Invalidate();
	VisualController::GetSingleton()->Invalidate();

	ResetCounter();
}


void Controller::HaltProcess()
{
	groundedSinceMs.store(0, std::memory_order_release);
	stationarySinceMs.store(0, std::memory_order_release);

	streakTracker.EndChain(NowMs());
	FlushHighscore();

	CrouchController::GetSingleton()->Halt();
	HeightController::GetSingleton()->Halt();
	RamController::GetSingleton()->Halt();
	SpeedController::GetSingleton()->Halt();
	StrafeController::GetSingleton()->Halt();

	ResetCounter();
}
