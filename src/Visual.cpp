#include "Visual.h"

#include <algorithm>
#include <chrono>

#include "Settings.h"


void VisualController::Invalidate() noexcept
{
	RestorePendingFov();
	bhkOffsetCountdown = 4;
	hasSavedDirectionZ = false;
}


void VisualController::RestorePendingFov() noexcept
{
	zoomActive = false;
	zoomAppliedSteps = 0;
	if (zoomAccumulatedDelta != 0.0f) {
		if (auto* cam = Camera()) {
			cam->GetRuntimeData2().worldFOV -= zoomAccumulatedDelta;
		}
		zoomAccumulatedDelta = 0.0f;
	}
}


void VisualController::Reset() noexcept
{
	RestorePendingFov();
	bhkOffsetCountdown = 4;
	if (hasSavedDirectionZ) {
		if (auto* bhk = CharController()) {
			bhk->direction.quad.m128_f32[2] = savedDirectionZ;
		}
		hasSavedDirectionZ = false;
	}
}


void VisualController::Update() noexcept
{
}


void VisualController::Tick() noexcept
{
	if (!zoomActive) {
		return;
	}

	const auto* main = RE::Main::GetSingleton();
	auto*       ui = RE::UI::GetSingleton();
	if (!main || main->GetRuntimeData().quitGame || main->GetRuntimeData().freezeTime ||
		(ui && ui->GameIsPaused()) || !*Settings::enableJumpFeedback || !*Settings::enableFovZoom) {
		RestorePendingFov();
		return;
	}

	auto* cam = Camera();
	if (!cam || cam->IsInThirdPerson()) {
		RestorePendingFov();
		return;
	}

	const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - zoomStartTime).count();
	const int targetSteps = static_cast<int>(std::clamp<long long>(elapsedMs / 4, 1, 32));

	float frameDelta = 0.0f;
	while (zoomAppliedSteps < targetSteps) {
		frameDelta += (zoomAppliedSteps >= 16) ? 0.1f : -0.1f;
		++zoomAppliedSteps;
	}

	if (frameDelta != 0.0f) {
		cam->GetRuntimeData2().worldFOV += frameDelta;
		zoomAccumulatedDelta += frameDelta;
	}

	if (zoomAppliedSteps >= 32) {
		RestorePendingFov();
	}
}


void VisualController::ApplyEffect()
{
	if (!*Settings::enableJumpFeedback) {
		Invalidate();
		Reset();
		return;
	}

	const auto* cam = Camera();
	if (!cam || cam->IsInThirdPerson()) {
		Invalidate();
		Reset();
		return;
	}

	if (*Settings::enableFovZoom) {
		RestorePendingFov();
		zoomStartTime = std::chrono::steady_clock::now();
		zoomAppliedSteps = 0;
		zoomActive = true;
	} else {
		RestorePendingFov();
	}

	if (*Settings::enableTremble) {
		if (auto* bhk = CharController()) {
			if (!hasSavedDirectionZ) {
				savedDirectionZ = bhk->direction.quad.m128_f32[2];
				hasSavedDirectionZ = true;
			}
			// may cause serious visual artifact
			bhk->direction.quad.m128_f32[2] += 0.5f;
		}

		if (--bhkOffsetCountdown < 0) {
			Reset();
		}
	} else if (hasSavedDirectionZ) {
		Reset();
	}
}
