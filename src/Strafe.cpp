#include "Strafe.h"

#include <algorithm>
#include <cmath>

#include "Settings.h"
#include "Speed.h"


void StrafeController::Invalidate() noexcept
{
	diff = 0.0f;
	takeoffYaw = 0.0f;
	startX = 0.0f;
	startY = 0.0f;
	hasOrigin = false;
}


void StrafeController::Reset() noexcept
{
	Invalidate();
}


void StrafeController::Update() noexcept
{
	const auto* player = Player();
	if (!player) {
		diff = 0.0f;
		return;
	}

	const float currX = player->GetPositionX();
	const float currY = player->GetPositionY();
	if (!hasOrigin) {
		startX = currX;
		startY = currY;
		takeoffYaw = player->GetAngleZ();
		hasOrigin = true;
		diff = 0.0f;
		return;
	}

	const Vector currPos = _mm_setr_ps(currX, currY, 0.0f, 0.0f);
	const Vector startPos = _mm_setr_ps(startX, startY, 0.0f, 0.0f);
	diff = CalcRelativeStrafeAngle(currPos, startPos, takeoffYaw);
}


void StrafeController::TryStrafe()
{
	if (const auto* player = Player()) {
		startX = player->GetPositionX();
		startY = player->GetPositionY();
		takeoffYaw = player->GetAngleZ();
		diff = 0.0f;
		hasOrigin = true;
	} else {
		Invalidate();
	}
}


void StrafeController::CalcDipChange()
{
	Update();
}


void StrafeController::GainStrafeBonud()
{
	if (!std::isfinite(diff)) {
		return;
	}

	constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
	diff *= kRadToDeg;

	if (diff >= *Settings::minStrafeAngle) {
		const float clamped = (std::min)(diff, *Settings::maxStrafeAngle);
		const float angle = clamped - *Settings::strafeDeadzone;
		if (angle <= 0.0f) {
			return;
		}
		const float strafeBonus = *Settings::globalSpeedMult * *Settings::strafeSpeedMult * angle / 3.0f;
		if (strafeBonus != 0.0f) {
			SpeedController::GetSingleton()->SpeedUp(strafeBonus);
		}
	}
}
