#pragma once

#include <cmath>

#include "IController.h"


[[nodiscard]] inline bool ComputeSafeCarryWeightNudge(
	const float a_invWeight,
	const float a_carryWeight,
	float&      a_outFirstNudge) noexcept
{
	if (!std::isfinite(a_invWeight) || !std::isfinite(a_carryWeight)) {
		return false;
	}

	constexpr float kMargin = 0.01f;
	constexpr float kNudge = 0.0625f;
	if (std::fabs(a_invWeight - a_carryWeight) <= kMargin) {
		return false;
	}

	// If already overencumbered, subtract first so restoring (+kNudge) cannot cross downward and trigger the HUD alert.
	a_outFirstNudge = a_invWeight > a_carryWeight ? -kNudge : kNudge;
	return true;
}


class SpeedController final : public IController
{
public:
	void SpeedUp(float a_speedOffset = 0.0f);
	void ReduceSpeed(float a_amount) noexcept;
	void SyncCap() noexcept;
	void Invalidate() noexcept;

	void Halt() noexcept override { Reset(); }

	static SpeedController* GetSingleton()
	{
		static SpeedController singleton;
		return std::addressof(singleton);
	}

	[[nodiscard]] constexpr float GetBaseSpeed() const noexcept { return baseSpeed; }
	[[nodiscard]] constexpr float GetCurrSpeed() const noexcept { return currSpeed; }
	[[nodiscard]] constexpr float GetAppliedBonus() const noexcept { return appliedBonus; }

	SpeedController(const SpeedController&) = delete;
	SpeedController(SpeedController&&) = delete;
	SpeedController() = default;
	~SpeedController() override = default;

	SpeedController& operator=(const SpeedController&) = delete;
	SpeedController& operator=(SpeedController&&) = delete;

private:
	void Reset() noexcept override;
	void Update() noexcept override;

	float baseSpeed = -1.0f;
	float currSpeed = -1.0f;
	float appliedBonus = 0.0f;
};
