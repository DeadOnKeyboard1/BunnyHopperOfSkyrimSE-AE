#include "Controller.h"
#include "Events.h"
#include "Menu.h"
#include "Settings.h"
#include "Visual.h"
#include "version.h"

#include <windows.h>


namespace
{
	DWORD g_mainThreadId = 0;

	struct PlayerUpdateHook
	{
		using UpdateFn = void (*)(RE::PlayerCharacter*, float);

		static void Thunk(RE::PlayerCharacter* a_this, const float a_delta)
		{
			if (a_this && a_this == RE::PlayerCharacter::GetSingleton() &&
				::GetCurrentThreadId() == g_mainThreadId) {
				Events::ProcessPendingAnimationGraphRegistration();
				Controller::GetSingleton()->OnFrameTick();
				VisualController::GetSingleton()->Tick();
			}
			func(a_this, a_delta);
		}

		static inline UpdateFn func = nullptr;
	};

	void InstallPlayerUpdateHook()
	{
		static bool installed = false;
		if (installed) {
			return;
		}

		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[0] };
		PlayerUpdateHook::func = reinterpret_cast<PlayerUpdateHook::UpdateFn>(
			vtbl.write_vfunc(0x0AD, PlayerUpdateHook::Thunk));
		installed = true;
		SKSE::log::info("Installed PlayerCharacter::Update frame hook");
	}

	class TESObjectLoadedHandler final : public RE::BSTEventSink<RE::TESObjectLoadedEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;

		static TESObjectLoadedHandler* GetSingleton()
		{
			static TESObjectLoadedHandler singleton;
			return std::addressof(singleton);
		}

		EventResult ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override
		{
			if (!a_event || !a_event->loaded) {
				return EventResult::kContinue;
			}

			if (const auto player = RE::PlayerCharacter::GetSingleton(); player && a_event->formID == player->GetFormID()) {
				Events::ScheduleAnimationGraphRegistration();
			}

			return EventResult::kContinue;
		}

		TESObjectLoadedHandler(const TESObjectLoadedHandler&) = delete;
		TESObjectLoadedHandler(TESObjectLoadedHandler&&) = delete;
		TESObjectLoadedHandler& operator=(const TESObjectLoadedHandler&) = delete;
		TESObjectLoadedHandler& operator=(TESObjectLoadedHandler&&) = delete;

	private:
		TESObjectLoadedHandler() = default;
		~TESObjectLoadedHandler() override = default;
	};


	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) {
			return;
		}

		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			InstallPlayerUpdateHook();
			Menu::Register();
			if (const auto sourceHolder = RE::ScriptEventSourceHolder::GetSingleton()) {
				sourceHolder->AddEventSink<RE::TESObjectLoadedEvent>(TESObjectLoadedHandler::GetSingleton());
				sourceHolder->AddEventSink<RE::TESLoadGameEvent>(Events::LoadGameHandler::GetSingleton());
			}
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Events::CancelPendingGraphRegistration();
			Events::BHopHandler::GetSingleton()->ResetCountdown();
			Controller::GetSingleton()->OnPreLoad();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			Events::ScheduleAnimationGraphRegistration();
			break;
		case SKSE::MessagingInterface::kSaveGame:
			Controller::GetSingleton()->FlushHighscore();
			break;
		default:
			break;
		}
	}
}


SKSEPluginInfo(
	.Version = REL::Version{ BHOS_VERSION_MAJOR, BHOS_VERSION_MINOR, BHOS_VERSION_PATCH, BHOS_VERSION_BETA },
	.Name = "BunnyHopperOfSkyrim"sv,
	.StructCompatibility = SKSE::StructCompatibility::Dependent,
	.RuntimeCompatibility = { SKSE::VersionIndependence::AddressLibrary, true },
	.MinimumSKSEVersion = REL::Version{}
)


SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
	g_mainThreadId = ::GetCurrentThreadId();
	SKSE::Init(a_skse);
	SKSE::log::info("BunnyHopperOfSkyrim v{} loaded", BHOS_VERSION_VERSTRING);

	const auto messaging = SKSE::GetMessagingInterface();
	if (messaging && messaging->RegisterListener("SKSE", MessageHandler)) {
		SKSE::log::info("Messaging interface registration successful");
	} else {
		SKSE::log::critical("Messaging interface registration failed!");
		return false;
	}

	if (Settings::LoadSettings()) {
		Controller::GetSingleton()->SyncHighscoreFromSettings();
		SKSE::log::info("Settings successfully loaded");
	} else {
		SKSE::log::critical("Settings failed to load!");
		return false;
	}

	return true;
}
