#pragma once

#include <immintrin.h>

#include <algorithm>
#include <cmath>

#include "RE/Skyrim.h"

using Vector = __m128;

// 2D / 3D distance
[[nodiscard]] inline float CalcDistance(const Vector& a_v1, const Vector& a_v2, const bool a_calcZ = false) noexcept
{
	const auto diff = _mm_sub_ps(a_v1, a_v2);
	const auto power = _mm_mul_ps(diff, diff);
	alignas(16) float lanes[4]{};
	_mm_store_ps(lanes, power);
	const float sum = lanes[0] + lanes[1] + (a_calcZ ? lanes[2] : 0.0f);
	return sum > 0.0f ? std::sqrt(sum) : 0.0f;
}

// radian acos ( dY / dist )
[[nodiscard]] inline float CalcAngle2D(const Vector& a_v1, const Vector& a_v2) noexcept
{
	const float dist = CalcDistance(a_v1, a_v2);
	if (dist <= 1e-6f) {
		return 0.0f;
	}
	alignas(16) float p1[4]{};
	alignas(16) float p2[4]{};
	_mm_store_ps(p1, a_v1);
	_mm_store_ps(p2, a_v2);
	return std::acos(std::clamp((p1[1] - p2[1]) / dist, -1.0f, 1.0f));
}

// radian angle between XY displacement (a_curr - a_start) and Skyrim yaw forward (sin(yaw), cos(yaw))
[[nodiscard]] inline float CalcRelativeStrafeAngle(
	const Vector& a_curr,
	const Vector& a_start,
	const float   a_takeoffYawRad) noexcept
{
	const float dist = CalcDistance(a_curr, a_start);
	if (dist <= 1e-3f) {
		return 0.0f;
	}
	alignas(16) float curr[4]{};
	alignas(16) float start[4]{};
	_mm_store_ps(curr, a_curr);
	_mm_store_ps(start, a_start);
	const float dx = curr[0] - start[0];
	const float dy = curr[1] - start[1];
	const float fx = std::sin(a_takeoffYawRad);
	const float fy = std::cos(a_takeoffYawRad);
	const float dot = (dx * fx + dy * fy) / dist;
	return std::acos(std::clamp(dot, -1.0f, 1.0f));
}

// radian acos ( d2D / d3D )
[[nodiscard]] inline float CalcAngle3D(const Vector& a_v1, const Vector& a_v2) noexcept
{
	const float dist3 = CalcDistance(a_v1, a_v2, true);
	if (dist3 <= 1e-6f) {
		return 0.0f;
	}
	return std::acos(std::clamp(CalcDistance(a_v1, a_v2) / dist3, -1.0f, 1.0f));
}


class IController
{
public:
	virtual void Halt() noexcept = 0;

	IController() = default;
	virtual ~IController() = default;

	IController(const IController&) = delete;
	IController(IController&&) = delete;
	IController& operator=(const IController&) = delete;
	IController& operator=(IController&&) = delete;

protected:
	virtual void Reset() noexcept = 0;
	virtual void Update() noexcept = 0;

	[[nodiscard]] static RE::PlayerCharacter* Player() noexcept
	{
		return RE::PlayerCharacter::GetSingleton();
	}

	[[nodiscard]] static RE::bhkCharacterController* CharController() noexcept
	{
		const auto* player = RE::PlayerCharacter::GetSingleton();
		return player ? player->GetCharController() : nullptr;
	}

	[[nodiscard]] static RE::PlayerCamera* Camera() noexcept
	{
		return RE::PlayerCamera::GetSingleton();
	}
};
