#include "Speed.h"

#include <algorithm>
#include <cmath>

#include "Settings.h"


namespace
{
	// Skyrim's ModActorValue updates SpeedMult modifiers without running a change callback for AV 30;
	// a zero-net CarryWeight nudge fires AV 32's UpdateMovementSpeeds handler immediately.
	void RefreshMovementSpeed(RE::ActorValueOwner* a_av) noexcept
	{
		if (!a_av) {
			return;
		}

		const float invWeight = a_av->GetActorValue(RE::ActorValue::kInventoryWeight);
		const float carryWeight = a_av->GetActorValue(RE::ActorValue::kCarryWeight);
		float       firstNudge = 0.0f;
		if (ComputeSafeCarryWeightNudge(invWeight, carryWeight, firstNudge)) {
			a_av->ModActorValue(
				RE::ACTOR_VALUE_MODIFIER::kTemporary,
				RE::ActorValue::kCarryWeight,
				firstNudge);
			a_av->ModActorValue(
				RE::ACTOR_VALUE_MODIFIER::kTemporary,
				RE::ActorValue::kCarryWeight,
				-firstNudge);
		}
	}
}


void SpeedController::Invalidate() noexcept
{
	baseSpeed = -1.0f;
	currSpeed = -1.0f;
	appliedBonus = 0.0f;
}


void SpeedController::Reset() noexcept
{
	if (appliedBonus != 0.0f) {
		auto* player = Player();
		if (auto* av = player ? player->AsActorValueOwner() : nullptr) {
			av->ModActorValue(
				RE::ACTOR_VALUE_MODIFIER::kTemporary,
				RE::ActorValue::kSpeedMult,
				-appliedBonus);
			RefreshMovementSpeed(av);
		}
	}

	baseSpeed = -1.0f;
	currSpeed = -1.0f;
	appliedBonus = 0.0f;
}


void SpeedController::Update() noexcept
{
	auto* player = Player();
	auto* av = player ? player->AsActorValueOwner() : nullptr;
	if (!av) {
		return;
	}

	if (baseSpeed < 0.0f) {
		baseSpeed = av->GetActorValue(RE::ActorValue::kSpeedMult);
		currSpeed = baseSpeed;
		appliedBonus = 0.0f;
	}

	const float maxAllowed = (std::max)(baseSpeed, *Settings::maxSpeed);
	currSpeed = std::clamp(currSpeed, 0.0f, maxAllowed);

	const float desiredBonus = currSpeed - baseSpeed;
	const float stepDelta = desiredBonus - appliedBonus;
	if (stepDelta != 0.0f) {
		av->ModActorValue(
			RE::ACTOR_VALUE_MODIFIER::kTemporary,
			RE::ActorValue::kSpeedMult,
			stepDelta);
		appliedBonus += stepDelta;
		RefreshMovementSpeed(av);
	}
}


void SpeedController::SyncCap() noexcept
{
	if (baseSpeed >= 0.0f) {
		Update();
	}
}


void SpeedController::SpeedUp(const float a_speedOffset)
{
	if (baseSpeed < 0.0f) {
		Update();
		if (baseSpeed < 0.0f) {
			return;
		}
	}

	const float speedBoost = *Settings::globalSpeedMult * *Settings::baseSpeedBoost * *Settings::baseSpeedMult;
	currSpeed += speedBoost + a_speedOffset;
	Update();
}


void SpeedController::ReduceSpeed(const float a_amount) noexcept
{
	if (baseSpeed < 0.0f || a_amount <= 0.0f) {
		return;
	}

	currSpeed = (std::max)(baseSpeed, currSpeed - a_amount);
	Update();
}
