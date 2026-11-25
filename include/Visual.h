#pragma once

#include "IController.h"

#include <chrono>
#include <cstdint>


class VisualController final : public IController
{
public:
	static VisualController* GetSingleton()
	{
		static VisualController singleton;
		return std::addressof(singleton);
	}

	void ApplyEffect();
	void Tick() noexcept;
	void Invalidate() noexcept;

	void Halt() noexcept override { Reset(); }

	VisualController() = default;
	~VisualController() override = default;

	VisualController(const VisualController&) = delete;
	VisualController(VisualController&&) = delete;
	VisualController& operator=(const VisualController&) = delete;
	VisualController& operator=(VisualController&&) = delete;

private:
	void Update() noexcept override;
	void Reset() noexcept override;
	void RestorePendingFov() noexcept;

	int                                   bhkOffsetCountdown = 4;
	bool                                  hasSavedDirectionZ = false;
	float                                 savedDirectionZ = 1.0144f;
	bool                                  zoomActive = false;
	std::chrono::steady_clock::time_point zoomStartTime{};
	int                                   zoomAppliedSteps = 0;
	float                                 zoomAccumulatedDelta = 0.0f;
};
