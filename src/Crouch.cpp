#include "Crouch.h"

#include <algorithm>

#include "Settings.h"
#include "Speed.h"


void CrouchController::Invalidate() noexcept
{
	cooldown = 0;
}


void CrouchController::Reset() noexcept
{
	cooldown = (std::max)(0, *Settings::crouchBoostCooldown);
}


void CrouchController::Update() noexcept
{
	cooldown = ClampCrouchCooldown(cooldown, *Settings::crouchBoostCooldown);
	if (cooldown > 0) {
		--cooldown;
	}
}


void CrouchController::GainCrouchBonus()
{
	Update();

	const auto* player = Player();
	if (!player) {
		return;
	}

	if (cooldown <= 0 && player->IsSneaking()) {
		const float crouchBonus = *Settings::globalSpeedMult * *Settings::crouchBoostMult * *Settings::crouchSpeedBoost;

		SpeedController::GetSingleton()->SpeedUp(crouchBonus);

		Reset();
	}
}
