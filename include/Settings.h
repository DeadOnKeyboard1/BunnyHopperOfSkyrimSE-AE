#pragma once

#include "Json2Settings.h"

namespace J2S = Json2Settings;

class Settings
{
public:
	static bool LoadSettings(bool a_dumpParse = false);
	static bool SaveSettings();
	static bool SaveHighscore();
	static void ResetDefaults() noexcept;
	[[nodiscard]] static constexpr const char* GetFileName() noexcept { return FILE_NAME; }

	// global
	static J2S::fSetting globalSpeedMult;
	static J2S::fSetting maxSpeed;
	static J2S::iSetting misttepAllowed;
	
	// base
	static J2S::fSetting baseSpeedBoost;
	static J2S::fSetting baseSpeedMult;

	// strafe
	static J2S::fSetting minStrafeAngle;
	static J2S::fSetting maxStrafeAngle;
	static J2S::fSetting strafeDeadzone;
	static J2S::fSetting strafeSpeedMult;
	
	// height
	static J2S::fSetting minHeightLaunch;
	static J2S::fSetting heightLaunchMult;

	// crouch
	static J2S::fSetting crouchSpeedBoost;
	static J2S::fSetting crouchBoostMult;
	static J2S::iSetting crouchBoostCooldown;

	// ram
	static J2S::fSetting ramDamage;
	static J2S::fSetting ramDamageMult;
	static J2S::fSetting ramSpeedThreshold;
	static J2S::fSetting ramSpeedReduction;
	
	// visual
	static J2S::bSetting enableJumpFeedback;
	static J2S::bSetting enableFovZoom;
	static J2S::bSetting enableTremble;

	// counter
	static J2S::bSetting enableHopCounter;
	static J2S::bSetting enableHighscoreCounter;
	static J2S::bSetting hopCounterHideWhenIdle;
	static J2S::fSetting hopCounterPosX;
	static J2S::fSetting hopCounterPosY;
	static J2S::fSetting hopCounterScale;
	static J2S::fSetting highscoreCounterPosX;
	static J2S::fSetting highscoreCounterPosY;
	static J2S::fSetting highscoreCounterScale;
	static J2S::iSetting hopHighscore;
	
	Settings() = delete;

private:
	static constexpr const char* FILE_NAME = "Data\\SKSE\\Plugins\\BunnyHopperOfSkyrim.json";
};
