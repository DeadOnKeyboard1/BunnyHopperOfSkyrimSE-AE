#pragma once

#include <algorithm>

#include "IController.h"


[[nodiscard]] constexpr int ClampCrouchCooldown(const int a_currentCooldown, const int a_maxCooldown) noexcept
{
	const int sanitizedMax = (std::max)(0, a_maxCooldown);
	return a_currentCooldown > sanitizedMax ? sanitizedMax : (std::max)(0, a_currentCooldown);
}


class CrouchController final : public IController
{
public:
	static CrouchController* GetSingleton()
	{
		static CrouchController singleton;
		return std::addressof(singleton);
	}

	void GainCrouchBonus();
	void Invalidate() noexcept;

	void Halt() noexcept override { Reset(); }

	CrouchController(const CrouchController&) = delete;
	CrouchController(CrouchController&&) = delete;
	CrouchController() = default;
	~CrouchController() override = default;

	CrouchController& operator=(const CrouchController&) = delete;
	CrouchController& operator=(CrouchController&&) = delete;

private:
	void Reset() noexcept override;
	void Update() noexcept override;

	int cooldown = 0;
};
