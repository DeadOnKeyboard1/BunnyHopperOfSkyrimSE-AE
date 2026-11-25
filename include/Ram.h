#pragma once

#include "IController.h"

#include <chrono>
#include <type_traits>
#include <unordered_map>


class RamController final : public IController
{
public:
	static RamController* GetSingleton()
	{
		static RamController singleton;
		return std::addressof(singleton);
	}

	void TestRam();
	void Invalidate() noexcept;

	void Halt() noexcept override { Reset(); }

	RamController(const RamController&) = delete;
	RamController(RamController&&) = delete;
	RamController() = default;
	~RamController() override = default;

	RamController& operator=(const RamController&) = delete;
	RamController& operator=(RamController&&) = delete;

private:
	void TestRange(RE::PlayerCharacter* a_player, RE::Actor* a_actor);
	void ApplyForce(RE::PlayerCharacter* a_player, RE::Actor* a_target, float a_mult);

	void Reset() noexcept override;
	void Update() noexcept override;

	std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> targetCooldowns;
};
static_assert(std::is_base_of_v<IController, RamController>);
