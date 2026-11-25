#pragma once

#include "IController.h"

#include <cstdint>


class HeightController final : public IController
{
public:
	enum class State : std::uint8_t
	{
		kNone = 1 << 0,
		kStair = 1 << 1,
		kLaunch = 1 << 2
	};

	static_assert(sizeof(State) == 0x1);

	static HeightController* GetSingleton()
	{
		static HeightController singleton;
		return std::addressof(singleton);
	}

	void CalcHeightDiff();
	void GainHeightBonus();
	void InitState(State a_state);
	void TryHeightLaunch() const;
	void ResetPos() noexcept;
	void EnsurePos() noexcept;
	void Invalidate() noexcept;
	void Halt() noexcept override { Reset(); }
	[[nodiscard]] constexpr State GetLastState() const noexcept { return state; }
	[[nodiscard]] constexpr float GetBaseJumpHeight() const noexcept { return baseJumpHeight; }
	[[nodiscard]] constexpr float GetCurrJumpHeight() const noexcept { return currJumpHeight; }

	HeightController(const HeightController&) = delete;
	HeightController(HeightController&&) = delete;
	HeightController() = default;
	~HeightController() override = default;

	HeightController& operator=(const HeightController&) = delete;
	HeightController& operator=(HeightController&&) = delete;

private:
	void Reset() noexcept override;
	void Update() noexcept override;

	float baseJumpHeight = -1.0f;
	float currJumpHeight = -1.0f;
	float pos = 0.0f;
	bool  hasPos = false;

	State state = State::kNone;
};
