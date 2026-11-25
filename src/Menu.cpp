#include "Menu.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <string>

#include "Controller.h"
#include "SKSEMenuFramework.h"
#include "Settings.h"
#include "Speed.h"
#include "Visual.h"

namespace Menu
{
namespace
{
	std::atomic_bool     g_registered{ false };
	std::atomic_bool     g_hudRegistered{ false };
	std::atomic_int64_t  g_lastSettingsRenderNs{ 0 };
	std::string          g_statusMessage;
	auto                 g_statusTimestamp = std::chrono::steady_clock::time_point{};

	[[nodiscard]] bool HasExports(const HMODULE a_module, const char* const* a_names, const std::size_t a_count, const char* a_context) noexcept
	{
		if (!a_module) {
			return false;
		}

		for (std::size_t i = 0; i < a_count; ++i) {
			if (!GetProcAddress(a_module, a_names[i])) {
				SKSE::log::warn("SKSEMenuFramework is loaded but missing export '{}'; skipping {}", a_names[i], a_context);
				return false;
			}
		}

		return true;
	}

	[[nodiscard]] bool HasRequiredExports(const HMODULE a_module) noexcept
	{
		constexpr std::array<const char*, 13> kRequiredExports = {
			"AddSectionItem",
			"igButton",
			"igCheckbox",
			"igSliderFloat",
			"igSliderInt",
			"igCollapsingHeader_TreeNodeFlags",
			"igSeparator",
			"igSameLine",
			"igTextDisabledV",
			"igIsItemHovered",
			"igSetTooltipV",
			"igBeginDisabled",
			"igEndDisabled"
		};

		return HasExports(a_module, kRequiredExports.data(), kRequiredExports.size(), "menu registration");
	}

	[[nodiscard]] bool HasHudExports(const HMODULE a_module) noexcept
	{
		constexpr std::array<const char*, 11> kRequiredHudExports = {
			"RegisterHudElement",
			"IsAnyBlockingWindowOpened",
			"igGetMainViewport",
			"igSetNextWindowPos",
			"igSetNextWindowBgAlpha",
			"igBegin",
			"igEnd",
			"igSetWindowFontScale",
			"igTextV",
			"igPushStyleColor_Vec4",
			"igPopStyleColor"
		};

		return HasExports(a_module, kRequiredHudExports.data(), kRequiredHudExports.size(), "HUD overlay registration");
	}

	void SetStatus(std::string a_message)
	{
		g_statusMessage = std::move(a_message);
		g_statusTimestamp = std::chrono::steady_clock::now();
	}

	void ShowTooltip(const char* a_tooltip)
	{
		if (a_tooltip && ImGuiMCP::IsItemHovered()) {
			ImGuiMCP::SetTooltip("%s", a_tooltip);
		}
	}

	bool DrawFloatSetting(
		const char*    a_label,
		J2S::fSetting& a_setting,
		const float    a_min,
		const float    a_max,
		const char*    a_format,
		const char*    a_tooltip,
		const float    a_hardMin = 0.0f,
		const float    a_hardMax = 100000.0f)
	{
		float      value = a_setting.Get();
		const bool changed = ImGuiMCP::SliderFloat(a_label, &value, a_min, a_max, a_format);
		if (changed) {
			value = std::clamp(value, a_hardMin, a_hardMax);
			a_setting.Set(value);
		}
		ShowTooltip(a_tooltip);
		return changed;
	}

	bool DrawIntSetting(
		const char*    a_label,
		J2S::iSetting& a_setting,
		const int      a_min,
		const int      a_max,
		const char*    a_format,
		const char*    a_tooltip,
		const int      a_hardMin = 0,
		const int      a_hardMax = 1000)
	{
		int        value = static_cast<int>(a_setting.Get());
		const bool changed = ImGuiMCP::SliderInt(a_label, &value, a_min, a_max, a_format);
		if (changed) {
			value = std::clamp(value, a_hardMin, a_hardMax);
			a_setting.Set(static_cast<std::int32_t>(value));
		}
		ShowTooltip(a_tooltip);
		return changed;
	}

	bool DrawBoolSetting(
		const char*    a_label,
		J2S::bSetting& a_setting,
		const char*    a_tooltip)
	{
		bool       value = a_setting.Get();
		const bool changed = ImGuiMCP::Checkbox(a_label, &value);
		if (changed) {
			a_setting.Set(value);
		}
		ShowTooltip(a_tooltip);
		return changed;
	}

	void QueueRuntimeSync(const bool a_syncSpeed, const bool a_resetVisual)
	{
		if (!a_syncSpeed && !a_resetVisual) {
			return;
		}

		if (const auto* task = SKSE::GetTaskInterface()) {
			task->AddTask([a_syncSpeed, a_resetVisual]() {
				if (const auto* main = RE::Main::GetSingleton(); main && !main->GetRuntimeData().quitGame) {
					if (a_syncSpeed) {
						SpeedController::GetSingleton()->SyncCap();
					}
					if (a_resetVisual) {
						VisualController::GetSingleton()->Halt();
					}
				}
			});
		}
	}

	void DrawCounterWindow(
		const char* a_windowId,
		const float a_posXPercent,
		const float a_posYPercent,
		const float a_scale,
		const char* a_text,
		const bool  a_highlightGold)
	{
		const auto* viewport = ImGuiMCP::GetMainViewport();
		if (!viewport || viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f) {
			return;
		}

		const float clampedX = std::clamp(a_posXPercent, 0.0f, 100.0f) * 0.01f;
		const float clampedY = std::clamp(a_posYPercent, 0.0f, 100.0f) * 0.01f;
		const float clampedScale = std::clamp(a_scale, 0.5f, 3.0f);

		const ImGuiMCP::ImVec2 pos{
			viewport->Pos.x + viewport->Size.x * clampedX,
			viewport->Pos.y + viewport->Size.y * clampedY
		};
		const ImGuiMCP::ImVec2 pivot{ clampedX, clampedY };

		ImGuiMCP::SetNextWindowPos(pos, ImGuiMCP::ImGuiCond_Always, pivot);
		ImGuiMCP::SetNextWindowBgAlpha(0.45f);
		constexpr ImGuiMCP::ImGuiWindowFlags kHudFlags =
			ImGuiMCP::ImGuiWindowFlags_NoDecoration |
			ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiMCP::ImGuiWindowFlags_NoSavedSettings |
			ImGuiMCP::ImGuiWindowFlags_NoInputs |
			ImGuiMCP::ImGuiWindowFlags_NoNav |
			ImGuiMCP::ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiMCP::ImGuiWindowFlags_NoBringToFrontOnFocus;

		if (ImGuiMCP::Begin(a_windowId, nullptr, kHudFlags)) {
			ImGuiMCP::SetWindowFontScale(clampedScale);
			if (a_highlightGold) {
				ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{ 1.0f, 0.84f, 0.25f, 1.0f });
				ImGuiMCP::Text("%s", a_text);
				ImGuiMCP::PopStyleColor();
			} else {
				ImGuiMCP::Text("%s", a_text);
			}
		}
		ImGuiMCP::End();
	}

	void __stdcall RenderHud()
	{
		const bool showStreak = Settings::enableHopCounter.Get();
		const bool showHighscore = Settings::enableHighscoreCounter.Get();
		if (!showStreak && !showHighscore) {
			return;
		}

		const auto* main = RE::Main::GetSingleton();
		auto*       ui = RE::UI::GetSingleton();
		const auto* player = RE::PlayerCharacter::GetSingleton();
		if (!main || main->GetRuntimeData().quitGame || !ui) {
			return;
		}

		if (ui->IsMenuOpen("Loading Menu") ||
			ui->IsMenuOpen("Fader Menu") ||
			ui->IsMenuOpen("Credits Menu") ||
			ui->IsMenuOpen("RaceSex Menu")) {
			return;
		}

		const auto now = std::chrono::steady_clock::now();
		const auto nowNs = now.time_since_epoch().count();
		const auto lastMenuNs = g_lastSettingsRenderNs.load(std::memory_order_acquire);
		const bool previewInMenu = (nowNs - lastMenuNs) < 250'000'000LL;

		if (!previewInMenu) {
			if (!player ||
				!ui->IsMenuOpen(RE::HUDMenu::MENU_NAME) ||
				ui->GameIsPaused() ||
				ui->IsApplicationMenuOpen() ||
				ui->IsItemMenuOpen() ||
				ui->IsModalMenuOpen() ||
				ui->IsMenuOpen("Main Menu") ||
				ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) ||
				ui->IsMenuOpen(RE::Console::MENU_NAME) ||
				SKSEMenuFramework::IsAnyBlockingWindowOpened()) {
				return;
			}
		}

		const auto* controller = Controller::GetSingleton();
		const auto  streak = controller->GetCurrentStreak();
		const auto  highscore = controller->GetHighscore();
		const auto  lastEnded = controller->GetLastEndedStreak();
		const auto  endedAtMs = controller->GetStreakEndTimeMs();
		const auto  nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
		const bool  inLinger = (streak == 0 && lastEnded > 0 && endedAtMs > 0 && (nowMs - endedAtMs) < 2000);
		const bool  hideWhenIdle = Settings::hopCounterHideWhenIdle.Get();

		if (!previewInMenu && hideWhenIdle && streak == 0 && !inLinger) {
			return;
		}

		const bool newRecord = controller->IsNewRecordActive();
		const bool highlightNewRecord = newRecord && (streak > 0 || inLinger);

		if (showStreak) {
			std::string label;
			if (streak > 0) {
				label = std::format("Bunnyhop Streak: {}", streak);
			} else if (inLinger) {
				label = newRecord
					? std::format("Streak ended: {} (NEW BEST!)", lastEnded)
					: std::format("Streak ended: {}", lastEnded);
			} else if (previewInMenu) {
				label = "Bunnyhop Streak: 5";
			} else {
				label = "Bunnyhop Streak: 0";
			}

			DrawCounterWindow(
				"##BHOS_HopCounter",
				Settings::hopCounterPosX.Get(),
				Settings::hopCounterPosY.Get(),
				Settings::hopCounterScale.Get(),
				label.c_str(),
				highlightNewRecord);
		}

		if (showHighscore) {
			const auto shownBest = (previewInMenu && highscore == 0) ? 12u : highscore;
			const auto label = highlightNewRecord
				? std::format("Best Streak: {} *", shownBest)
				: std::format("Best Streak: {}", shownBest);

			DrawCounterWindow(
				"##BHOS_HighscoreCounter",
				Settings::highscoreCounterPosX.Get(),
				Settings::highscoreCounterPosY.Get(),
				Settings::highscoreCounterScale.Get(),
				label.c_str(),
				highlightNewRecord);
		}
	}

	void __stdcall Render()
	{
		g_lastSettingsRenderNs.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_release);

		if (ImGuiMCP::Button("Save to JSON")) {
			if (Settings::SaveSettings()) {
				SetStatus("Saved settings to BunnyHopperOfSkyrim.json");
			} else {
				SetStatus("Failed to save BunnyHopperOfSkyrim.json (check log)");
			}
		}
		ImGuiMCP::SameLine();
		if (ImGuiMCP::Button("Reload JSON")) {
			if (Settings::LoadSettings()) {
				Controller::GetSingleton()->SyncHighscoreFromSettings();
				QueueRuntimeSync(true, true);
				SetStatus("Reloaded settings from BunnyHopperOfSkyrim.json");
			} else {
				SetStatus("Failed to reload BunnyHopperOfSkyrim.json (check log)");
			}
		}
		ImGuiMCP::SameLine();
		if (ImGuiMCP::Button("Reset Defaults")) {
			Settings::ResetDefaults();
			QueueRuntimeSync(true, true);
			SetStatus("Reset all settings to defaults (not yet saved)");
		}

		if (!g_statusMessage.empty()) {
			const auto age = std::chrono::steady_clock::now() - g_statusTimestamp;
			if (age < std::chrono::seconds(4)) {
				ImGuiMCP::SameLine();
				ImGuiMCP::TextDisabled("%s", g_statusMessage.c_str());
			} else {
				g_statusMessage.clear();
			}
		}

		ImGuiMCP::TextDisabled("Changes apply immediately in-game. Ctrl+Click any slider to type an exact value.");
		ImGuiMCP::Separator();

		if (ImGuiMCP::CollapsingHeader("Global", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
			DrawFloatSetting(
				"Global Speed Multiplier",
				Settings::globalSpeedMult,
				0.0f,
				5.0f,
				"%.2f",
				"Master multiplier applied to base speed boost, strafe bonus, crouch boost, and height launch.");
			if (DrawFloatSetting(
					"Max Speed Mult (%)",
					Settings::maxSpeed,
					100.0f,
					1500.0f,
					"%.0f",
					"Upper SpeedMult (%) cap reachable via bunny hopping.",
					100.0f)) {
				QueueRuntimeSync(true, false);
			}
			DrawIntSetting(
				"Missteps Allowed",
				Settings::misttepAllowed,
				0,
				20,
				"%d",
				"Ground/landing events tolerated before momentum resets.");
			if (Settings::misttepAllowed.Get() == 0) {
				ImGuiMCP::TextDisabled("Note: Missteps Allowed = 0 resets momentum on the first ground step.");
			}
		}

		if (ImGuiMCP::CollapsingHeader("Base Speed", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
			DrawFloatSetting(
				"Base Speed Boost",
				Settings::baseSpeedBoost,
				0.0f,
				20.0f,
				"%.2f",
				"Flat SpeedMult added on every jump/fall event.");
			DrawFloatSetting(
				"Base Speed Multiplier",
				Settings::baseSpeedMult,
				0.0f,
				5.0f,
				"%.2f",
				"Scale factor for Base Speed Boost.");
		}

		if (ImGuiMCP::CollapsingHeader("Air Strafe", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
			if (DrawFloatSetting(
					"Min Strafe Angle",
					Settings::minStrafeAngle,
					0.0f,
					180.0f,
					"%.1f deg",
					"Minimum turning/strafing angle from takeoff heading required to award a strafe bonus.",
					0.0f,
					180.0f)) {
				if (Settings::minStrafeAngle.Get() > Settings::maxStrafeAngle.Get()) {
					Settings::maxStrafeAngle.Set(Settings::minStrafeAngle.Get());
				}
			}
			if (DrawFloatSetting(
					"Max Strafe Angle",
					Settings::maxStrafeAngle,
					0.0f,
					180.0f,
					"%.1f deg",
					"Upper strafe angle clamp before calculating bonus speed.",
					0.0f,
					180.0f)) {
				if (Settings::maxStrafeAngle.Get() < Settings::minStrafeAngle.Get()) {
					Settings::minStrafeAngle.Set(Settings::maxStrafeAngle.Get());
				}
			}
			DrawFloatSetting(
				"Strafe Deadzone",
				Settings::strafeDeadzone,
				0.0f,
				180.0f,
				"%.1f deg",
				"Degrees subtracted from the measured strafe angle before scaling.",
				0.0f,
				180.0f);
			DrawFloatSetting(
				"Strafe Speed Multiplier",
				Settings::strafeSpeedMult,
				0.0f,
				5.0f,
				"%.2f",
				"Multiplier for the strafe speed bonus.");
			if (Settings::strafeDeadzone.Get() >= Settings::maxStrafeAngle.Get()) {
				ImGuiMCP::TextDisabled("Note: Strafe Deadzone >= Max Strafe Angle disables the strafe speed bonus.");
			}
		}

		if (ImGuiMCP::CollapsingHeader("Height & Launch", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
			DrawFloatSetting(
				"Min Height Launch",
				Settings::minHeightLaunch,
				20.0f,
				800.0f,
				"%.0f units",
				"Minimum downward Z drop required to trigger a slope/ledge launch boost.",
				0.0f,
				800.0f);
			DrawFloatSetting(
				"Height Launch Multiplier",
				Settings::heightLaunchMult,
				0.0f,
				5.0f,
				"%.2f",
				"Speed boost gained per unit of downward height drop.");
		}

		if (ImGuiMCP::CollapsingHeader("Crouch Boost", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
			DrawFloatSetting(
				"Crouch Speed Boost",
				Settings::crouchSpeedBoost,
				0.0f,
				200.0f,
				"%.1f",
				"Extra SpeedMult awarded when landing while sneaking.");
			DrawFloatSetting(
				"Crouch Boost Multiplier",
				Settings::crouchBoostMult,
				0.0f,
				5.0f,
				"%.2f",
				"Multiplier for Crouch Speed Boost.");
			DrawIntSetting(
				"Crouch Boost Cooldown",
				Settings::crouchBoostCooldown,
				0,
				30,
				"%d landings",
				"Landings required before another crouch boost can trigger.");
		}

		if (ImGuiMCP::CollapsingHeader("Ramming", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
			DrawFloatSetting(
				"Ram Speed Threshold (%)",
				Settings::ramSpeedThreshold,
				100.0f,
				1000.0f,
				"%.0f",
				"Minimum SpeedMult (%) required to ram and knock back non-teammate NPCs.",
				0.0f);
			DrawFloatSetting(
				"Ram Damage",
				Settings::ramDamage,
				0.0f,
				100.0f,
				"%.1f",
				"Base health damage factor scaled by excess speed above threshold.");
			DrawFloatSetting(
				"Ram Damage Multiplier",
				Settings::ramDamageMult,
				0.0f,
				5.0f,
				"%.2f",
				"Multiplier applied to Ram Damage.");
			DrawFloatSetting(
				"Ram Speed Reduction",
				Settings::ramSpeedReduction,
				0.0f,
				2.0f,
				"%.2f",
				"Fraction of excess speed lost when ramming an NPC.");
		}

		if (ImGuiMCP::CollapsingHeader("Visual Feedback", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
			const bool jumpFeedbackChanged = DrawBoolSetting(
				"Enable Jump Feedback",
				Settings::enableJumpFeedback,
				"Master toggle for all first-person landing visual effects.");

			const bool feedbackEnabled = Settings::enableJumpFeedback.Get();
			if (jumpFeedbackChanged && !feedbackEnabled) {
				QueueRuntimeSync(false, true);
			}

			ImGuiMCP::BeginDisabled(!feedbackEnabled);
			DrawBoolSetting(
				"Enable FOV Zoom",
				Settings::enableFovZoom,
				"Subtle first-person FOV pulse on jump landings.");
			if (DrawBoolSetting(
					"Enable Tremble (Experimental)",
					Settings::enableTremble,
					"Camera/character controller direction nudge on landing; may cause visual artifacts.")) {
				if (!Settings::enableTremble.Get()) {
					QueueRuntimeSync(false, true);
				}
			}
			ImGuiMCP::EndDisabled();
		}

		if (ImGuiMCP::CollapsingHeader("HUD Counter", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
			DrawBoolSetting(
				"Enable Hop Counter",
				Settings::enableHopCounter,
				"Shows your consecutive bunnyhop streak as an on-screen HUD counter.");

			const bool hopEnabled = Settings::enableHopCounter.Get();
			ImGuiMCP::BeginDisabled(!hopEnabled);
			DrawFloatSetting(
				"Hop Counter Position X (%)",
				Settings::hopCounterPosX,
				0.0f,
				100.0f,
				"%.1f%%",
				"Horizontal screen position (0% = left, 100% = right).",
				0.0f,
				100.0f);
			DrawFloatSetting(
				"Hop Counter Position Y (%)",
				Settings::hopCounterPosY,
				0.0f,
				100.0f,
				"%.1f%%",
				"Vertical screen position (0% = top, 100% = bottom).",
				0.0f,
				100.0f);
			DrawFloatSetting(
				"Hop Counter Scale",
				Settings::hopCounterScale,
				0.5f,
				3.0f,
				"%.2fx",
				"Font size multiplier for the Bunnyhop Streak counter.",
				0.5f,
				3.0f);
			ImGuiMCP::EndDisabled();

			DrawBoolSetting(
				"Enable Highscore Counter",
				Settings::enableHighscoreCounter,
				"Shows your all-time maximum consecutive bunnyhop count (highscore) as an optional HUD counter.");

			const bool highscoreEnabled = Settings::enableHighscoreCounter.Get();
			ImGuiMCP::BeginDisabled(!highscoreEnabled);
			DrawFloatSetting(
				"Highscore Position X (%)",
				Settings::highscoreCounterPosX,
				0.0f,
				100.0f,
				"%.1f%%",
				"Horizontal screen position for the Highscore counter (0% = left, 100% = right).",
				0.0f,
				100.0f);
			DrawFloatSetting(
				"Highscore Position Y (%)",
				Settings::highscoreCounterPosY,
				0.0f,
				100.0f,
				"%.1f%%",
				"Vertical screen position for the Highscore counter (0% = top, 100% = bottom).",
				0.0f,
				100.0f);
			DrawFloatSetting(
				"Highscore Scale",
				Settings::highscoreCounterScale,
				0.5f,
				3.0f,
				"%.2fx",
				"Font size multiplier for the Highscore HUD counter.",
				0.5f,
				3.0f);
			ImGuiMCP::EndDisabled();

			ImGuiMCP::BeginDisabled(!hopEnabled && !highscoreEnabled);
			DrawBoolSetting(
				"Hide Counters When Idle",
				Settings::hopCounterHideWhenIdle,
				"Hides the HUD counters ~2 seconds after a hop chain ends; disable to keep them permanently visible.");
			ImGuiMCP::EndDisabled();

			ImGuiMCP::TextDisabled("Recorded Highscore: %u hops in a row", Controller::GetSingleton()->GetHighscore());
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button("Reset Highscore")) {
				Controller::GetSingleton()->ResetHighscore();
				SetStatus("Reset recorded highscore to 0");
			}
			ShowTooltip("Resets the saved maximum bunnyhop streak to 0.");

			if (hopEnabled || highscoreEnabled) {
				ImGuiMCP::TextDisabled("Live HUD positioning preview is visible while this page is open.");
			}
		}
	}
}  // namespace

void Register()
{
	if (g_registered.load(std::memory_order_acquire)) {
		return;
	}

	const HMODULE smfModule = GetMenuFrameworkModule();
	if (!smfModule) {
		SKSE::log::info("SKSEMenuFramework module not loaded; skipping optional menu registration");
		return;
	}

	if (!HasRequiredExports(smfModule)) {
		return;
	}

	SKSEMenuFramework::SetSection("BunnyHopper of Skyrim");
	SKSEMenuFramework::AddSectionItem("Settings", Render);
	g_registered.store(true, std::memory_order_release);
	SKSE::log::info("Registered optional SKSEMenuFramework settings page");

	if (!g_hudRegistered.load(std::memory_order_acquire) && HasHudExports(smfModule)) {
		static auto* s_hudElement = SKSEMenuFramework::AddHudElement(RenderHud);
		if (s_hudElement) {
			g_hudRegistered.store(true, std::memory_order_release);
			SKSE::log::info("Registered optional SKSEMenuFramework HUD counter overlay");
		}
	}
}
}  // namespace Menu
