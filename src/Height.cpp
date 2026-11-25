#include "Height.h"

#include <cmath>

#include "Controller.h"
#include "Settings.h"
#include "Speed.h"


void HeightController::Invalidate() noexcept
{
	baseJumpHeight = -1.0f;
	currJumpHeight = -1.0f;
	pos = 0.0f;
	hasPos = false;
	state = State::kNone;
}


void HeightController::Reset() noexcept
{
	if (baseJumpHeight >= 0.0f) {
		if (auto* bhk = CharController()) {
			bhk->jumpHeight = baseJumpHeight;
		}
	}

	baseJumpHeight = -1.0f;
	currJumpHeight = -1.0f;
	pos = 0.0f;
	hasPos = false;
	state = State::kNone;
}


void HeightController::Update() noexcept
{
	auto* bhk = CharController();
	if (!bhk) {
		return;
	}

	if (baseJumpHeight < 0.0f) {
		baseJumpHeight = bhk->jumpHeight;
		currJumpHeight = baseJumpHeight;
	}

	bhk->jumpHeight = currJumpHeight;
}


void HeightController::CalcHeightDiff()
{
	const auto* player = Player();
	if (!player) {
		state = State::kNone;
		return;
	}

	const float currentZ = player->GetPositionZ();
	if (!hasPos) {
		pos = currentZ;
		hasPos = true;
		state = State::kNone;
		return;
	}

	const float diff = currentZ - pos;
	if (std::fabs(diff) > 800.0f) {
		state = State::kNone;
		return;
	}

	// average height for stairs & hills
	if (diff >= 68.6f) {
		state = State::kStair;
		return;
	}

	if (diff < 0.0f && std::fabs(diff) >= *Settings::minHeightLaunch) {
		state = State::kLaunch;
		return;
	}

	state = State::kNone;
}


// distinguish : This HeightBonus is kJumpHeight not HeightLaunch
void HeightController::GainHeightBonus()
{
	const auto* speed = SpeedController::GetSingleton();
	const float currSpeed = speed->GetCurrSpeed();
	const float bonusHeight = currSpeed > 0.0f ? currSpeed / 1000.0f : 0.0f;

	if (baseJumpHeight >= 0.0f && currJumpHeight < baseJumpHeight + bonusHeight) {
		currJumpHeight = baseJumpHeight + bonusHeight;
	}

	Update();
}


void HeightController::InitState(const State a_state)
{
	auto* controller = Controller::GetSingleton();
	switch (a_state) {
	case State::kNone:
		break;
	case State::kStair:
		controller->ResetCounter();
		break;
	case State::kLaunch:
		TryHeightLaunch();
		controller->ResetCounter();
		break;
	default:
		break;
	}
	ResetPos();
}


void HeightController::TryHeightLaunch() const
{
	const auto* player = Player();
	if (!player || !hasPos) {
		return;
	}

	const float diff = player->GetPositionZ() - pos;
	const float heightBonus = *Settings::globalSpeedMult * *Settings::heightLaunchMult * std::fabs(diff);

	SpeedController::GetSingleton()->SpeedUp(heightBonus);
}


void HeightController::ResetPos() noexcept
{
	if (const auto* player = Player()) {
		pos = player->GetPositionZ();
		hasPos = true;
	}
}


void HeightController::EnsurePos() noexcept
{
	if (!hasPos) {
		ResetPos();
	}
}
