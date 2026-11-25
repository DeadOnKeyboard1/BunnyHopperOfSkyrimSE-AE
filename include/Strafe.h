#pragma once

#include "IController.h"


class StrafeController final : public IController
{
public:
	static StrafeController* GetSingleton()
	{
		static StrafeController singleton;
		return std::addressof(singleton);
	}

	void TryStrafe();
	void CalcDipChange();
	void GainStrafeBonud();
	void Invalidate() noexcept;
	void Halt() noexcept override { Reset(); }

	StrafeController(const StrafeController&) = delete;
	StrafeController(StrafeController&&) = delete;
	StrafeController() = default;
	~StrafeController() override = default;

	StrafeController& operator=(const StrafeController&) = delete;
	StrafeController& operator=(StrafeController&&) = delete;

private:
	void Reset() noexcept override;
	void Update() noexcept override;

	float diff = 0.0f;
	float takeoffYaw = 0.0f;
	float startX = 0.0f;
	float startY = 0.0f;
	bool  hasOrigin = false;
};
