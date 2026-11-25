#pragma once

#include <atomic>
#include <cstdint>


enum class FrameGuardAction
{
	kKeepActive,
	kReset
};

inline constexpr std::int64_t kMaxGroundedChainMs = 350;
inline constexpr std::int64_t kMaxStationaryChainMs = 180;

[[nodiscard]] constexpr FrameGuardAction EvaluateGroundChainState(
	const bool         a_chainActive,
	const bool         a_unsafeOrMenu,
	const bool         a_inMidair,
	const bool         a_isMoving,
	const std::int64_t a_groundedElapsedMs,
	const std::int64_t a_stationaryElapsedMs) noexcept
{
	if (!a_chainActive) {
		return FrameGuardAction::kKeepActive;
	}
	if (a_unsafeOrMenu) {
		return FrameGuardAction::kReset;
	}
	if (a_inMidair) {
		return FrameGuardAction::kKeepActive;
	}
	if (!a_isMoving && a_stationaryElapsedMs >= kMaxStationaryChainMs) {
		return FrameGuardAction::kReset;
	}
	if (a_groundedElapsedMs >= kMaxGroundedChainMs) {
		return FrameGuardAction::kReset;
	}
	return FrameGuardAction::kKeepActive;
}


class HopStreakTracker
{
public:
	void SetHighscore(const std::int32_t a_highscore) noexcept
	{
		const auto clamped = static_cast<std::uint32_t>(a_highscore > 0 ? a_highscore : 0);
		highscore.store(clamped, std::memory_order_release);
	}

	void OnJumping() noexcept
	{
		hasPendingHop = true;
	}

	// Returns true if the all-time highscore increased on this hop.
	bool CompleteHop() noexcept
	{
		if (!hasPendingHop) {
			return false;
		}
		hasPendingHop = false;

		const auto next = currentStreak.fetch_add(1, std::memory_order_acq_rel) + 1;
		if (next == 1) {
			newRecordThisChain.store(false, std::memory_order_release);
		}
		lastEndedStreak.store(0, std::memory_order_release);
		streakEndTimeMs.store(0, std::memory_order_release);

		auto best = highscore.load(std::memory_order_acquire);
		if (next > best) {
			while (next > best && !highscore.compare_exchange_weak(best, next, std::memory_order_acq_rel)) {
			}
			newRecordThisChain.store(true, std::memory_order_release);
			return true;
		}
		return false;
	}

	void EndChain(const std::int64_t a_nowMs) noexcept
	{
		hasPendingHop = false;
		const auto ended = currentStreak.exchange(0, std::memory_order_acq_rel);
		if (ended > 0) {
			lastEndedStreak.store(ended, std::memory_order_release);
			streakEndTimeMs.store(a_nowMs, std::memory_order_release);
		}
	}

	void Invalidate() noexcept
	{
		hasPendingHop = false;
		currentStreak.store(0, std::memory_order_release);
		lastEndedStreak.store(0, std::memory_order_release);
		streakEndTimeMs.store(0, std::memory_order_release);
		newRecordThisChain.store(false, std::memory_order_release);
	}

	void ResetHighscore() noexcept
	{
		highscore.store(0, std::memory_order_release);
		newRecordThisChain.store(false, std::memory_order_release);
	}

	[[nodiscard]] std::uint32_t GetCurrentStreak() const noexcept { return currentStreak.load(std::memory_order_acquire); }
	[[nodiscard]] std::uint32_t GetHighscore() const noexcept { return highscore.load(std::memory_order_acquire); }
	[[nodiscard]] std::uint32_t GetLastEndedStreak() const noexcept { return lastEndedStreak.load(std::memory_order_acquire); }
	[[nodiscard]] std::int64_t  GetStreakEndTimeMs() const noexcept { return streakEndTimeMs.load(std::memory_order_acquire); }
	[[nodiscard]] bool          IsNewRecordActive() const noexcept { return newRecordThisChain.load(std::memory_order_acquire); }

private:
	bool                 hasPendingHop = false;
	std::atomic_uint32_t currentStreak{ 0 };
	std::atomic_uint32_t highscore{ 0 };
	std::atomic_uint32_t lastEndedStreak{ 0 };
	std::atomic_int64_t  streakEndTimeMs{ 0 };
	std::atomic_bool     newRecordThisChain{ false };
};


class Controller
{
public:
	static Controller* GetSingleton();

	void OnJumping();
	void OnFalling();
	void OnLanding();
	void OnGround();
	void OnFrameTick();
	void OnDelete();
	void OnPreLoad();
	void HaltProcess();
	void FlushHighscore();
	void SyncHighscoreFromSettings() noexcept;
	void ResetHighscore();

	void ResetCounter() noexcept { stopCounter = 0; }

	[[nodiscard]] std::uint32_t GetCurrentStreak() const noexcept { return streakTracker.GetCurrentStreak(); }
	[[nodiscard]] std::uint32_t GetHighscore() const noexcept { return streakTracker.GetHighscore(); }
	[[nodiscard]] std::uint32_t GetLastEndedStreak() const noexcept { return streakTracker.GetLastEndedStreak(); }
	[[nodiscard]] std::int64_t  GetStreakEndTimeMs() const noexcept { return streakTracker.GetStreakEndTimeMs(); }
	[[nodiscard]] bool          IsNewRecordActive() const noexcept { return streakTracker.IsNewRecordActive(); }

	Controller(const Controller&) = delete;
	Controller(Controller&&) = delete;
	Controller() = default;
	~Controller() = default;

	Controller& operator=(const Controller&) = delete;
	Controller& operator=(Controller&&) = delete;

private:
	void RecordCompletedHop() noexcept;

	HopStreakTracker     streakTracker;
	std::atomic_bool     highscoreDirty{ false };
	std::atomic_int64_t  groundedSinceMs{ 0 };
	std::atomic_int64_t  stationarySinceMs{ 0 };
	unsigned             stopCounter = static_cast<unsigned>(-1);
};
