#include "Settings.h"

#include <SKSE/SKSE.h>


bool Settings::LoadSettings(const bool a_dumpParse)
{
	auto [log, success] = J2S::load_settings(FILE_NAME, a_dumpParse);

	if (!log.empty()) {
		if (success) {
			SKSE::log::info("{}", log);
		} else {
			SKSE::log::error("{}", log);
		}
	}

	return success;
}


bool Settings::SaveSettings()
{
	const auto [error, success] = J2S::save_settings(FILE_NAME);
	if (!success && !error.empty()) {
		SKSE::log::error("{}", error);
	} else if (success) {
		SKSE::log::info("Saved settings to {}", FILE_NAME);
	}
	return success;
}


bool Settings::SaveHighscore()
{
	const auto [error, success] = J2S::save_single_setting(FILE_NAME, hopHighscore);
	if (!success && !error.empty()) {
		SKSE::log::error("{}", error);
	}
	return success;
}


void Settings::ResetDefaults() noexcept
{
	J2S::reset_defaults();
}


decltype(Settings::globalSpeedMult) Settings::globalSpeedMult("globalSpeedMult", 1.0f);
decltype(Settings::maxSpeed) Settings::maxSpeed("maxSpeed", 450.0f);
decltype(Settings::misttepAllowed) Settings::misttepAllowed("misstepAllowed", 4);

decltype(Settings::baseSpeedBoost) Settings::baseSpeedBoost("baseSpeedBoost", 1.0f);
decltype(Settings::baseSpeedMult) Settings::baseSpeedMult("baseSpeedMult", 1.0f);

decltype(Settings::minStrafeAngle) Settings::minStrafeAngle("minStrafeAngle", 35.0f);
decltype(Settings::maxStrafeAngle) Settings::maxStrafeAngle("maxStrafeAngle", 95.0f);
decltype(Settings::strafeDeadzone) Settings::strafeDeadzone("strafeDeadzone", 35.0f);
decltype(Settings::strafeSpeedMult) Settings::strafeSpeedMult("strafeSpeedMult", 0.5f);

decltype(Settings::minHeightLaunch) Settings::minHeightLaunch("minHeightLaunch", 140.0f);
decltype(Settings::heightLaunchMult) Settings::heightLaunchMult("heightLaunchMult", 0.5f);

decltype(Settings::crouchSpeedBoost) Settings::crouchSpeedBoost("crouchSpeedBoost", 32.0f);
decltype(Settings::crouchBoostMult) Settings::crouchBoostMult("crouchBoostMult", 1.0f);
decltype(Settings::crouchBoostCooldown) Settings::crouchBoostCooldown("crouchBoostCooldown", 6);


decltype(Settings::ramDamage) Settings::ramDamage("ramDamage", 5.0f);
decltype(Settings::ramDamageMult) Settings::ramDamageMult("ramDamageMult", 0.3f);
decltype(Settings::ramSpeedThreshold) Settings::ramSpeedThreshold("ramSpeedThreshold", 220.0f);
decltype(Settings::ramSpeedReduction) Settings::ramSpeedReduction("ramSpeedReduction", 0.5f);

decltype(Settings::enableJumpFeedback) Settings::enableJumpFeedback("enableJumpFeedback", true);
decltype(Settings::enableFovZoom) Settings::enableFovZoom("enableFovZoom", true);
decltype(Settings::enableTremble) Settings::enableTremble("enableTremble", false);

decltype(Settings::enableHopCounter) Settings::enableHopCounter("enableHopCounter", false);
decltype(Settings::enableHighscoreCounter) Settings::enableHighscoreCounter("enableHighscoreCounter", false);
decltype(Settings::hopCounterHideWhenIdle) Settings::hopCounterHideWhenIdle("hopCounterHideWhenIdle", true);
decltype(Settings::hopCounterPosX) Settings::hopCounterPosX("hopCounterPosX", 50.0f);
decltype(Settings::hopCounterPosY) Settings::hopCounterPosY("hopCounterPosY", 14.0f);
decltype(Settings::hopCounterScale) Settings::hopCounterScale("hopCounterScale", 1.2f);
decltype(Settings::highscoreCounterPosX) Settings::highscoreCounterPosX("highscoreCounterPosX", 50.0f);
decltype(Settings::highscoreCounterPosY) Settings::highscoreCounterPosY("highscoreCounterPosY", 20.0f);
decltype(Settings::highscoreCounterScale) Settings::highscoreCounterScale("highscoreCounterScale", 1.0f);
decltype(Settings::hopHighscore) Settings::hopHighscore("hopHighscore", 0, false);
