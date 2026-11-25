#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "Animations.h"
#include "Controller.h"
#include "Crouch.h"
#include "Height.h"
#include "IController.h"
#include "Json2Settings.h"
#include "Settings.h"
#include "Speed.h"
#include "Visual.h"

namespace
{
	int g_failures = 0;

	void Check(const bool a_cond, const char* a_expr, const int a_line)
	{
		if (!a_cond) {
			std::cerr << "[FAIL] line " << a_line << ": " << a_expr << '\n';
			++g_failures;
		}
	}

	template <class T>
	void CheckNear(const T a_actual, const T a_expected, const T a_eps, const char* a_label, const int a_line)
	{
		if (std::isnan(a_actual) || std::fabs(a_actual - a_expected) > a_eps) {
			std::cerr << "[FAIL] line " << a_line << ": " << a_label
					  << " actual=" << a_actual << " expected=" << a_expected << '\n';
			++g_failures;
		}
	}

#define EXPECT_TRUE(expr) Check(static_cast<bool>(expr), #expr, __LINE__)
#define EXPECT_NEAR(actual, expected, eps) CheckNear((actual), (expected), (eps), #actual, __LINE__)

	struct ScopedTempFile
	{
		std::filesystem::path path;
		~ScopedTempFile()
		{
			std::error_code ec;
			std::filesystem::remove(path, ec);
		}
	};

	static_assert(sizeof(HeightController::State) == 0x1);
	static_assert(std::is_same_v<decltype(std::declval<RE::PlayerCamera>().GetRuntimeData2().worldFOV), float>);
	static_assert(offsetof(RE::bhkCharacterController, direction) == 0xC0);
	static_assert(offsetof(RE::bhkCharacterController, jumpHeight) == 0x23C);

	void TestAnimations()
	{
		static_assert(static_cast<std::uint64_t>(Anim::kUp) == hash_64_fnv1a_const("jumpup"));
		static_assert(static_cast<std::uint64_t>(Anim::kFall) == hash_64_fnv1a_const("jumpfall"));
		static_assert(static_cast<std::uint64_t>(Anim::kFallDirectional) == hash_64_fnv1a_const("jumpfalldirectional"));
		static_assert(static_cast<std::uint64_t>(Anim::kDown) == hash_64_fnv1a_const("jumpdown"));
		static_assert(static_cast<std::uint64_t>(Anim::kLandEnd) == hash_64_fnv1a_const("jumplandend"));
		static_assert(static_cast<std::uint64_t>(Anim::kFootLeft) == hash_64_fnv1a_const("footleft"));
		static_assert(static_cast<std::uint64_t>(Anim::kFootRight) == hash_64_fnv1a_const("footright"));
		static_assert(static_cast<std::uint64_t>(Anim::kGraphDeleting) == hash_64_fnv1a_const("graphdeleting"));

		EXPECT_TRUE(HashAnimation("jumpup", 6) == Anim::kUp);
		EXPECT_TRUE(HashAnimation("jumpfall", 8) == Anim::kFall);
		EXPECT_TRUE(HashAnimation("jumpfalldirectional", 19) == Anim::kFallDirectional);
		EXPECT_TRUE(HashAnimation("jumpdown", 8) == Anim::kDown);
		EXPECT_TRUE(HashAnimation("jumplandend", 11) == Anim::kLandEnd);
		EXPECT_TRUE(HashAnimation("footleft", 8) == Anim::kFootLeft);
		EXPECT_TRUE(HashAnimation("footright", 9) == Anim::kFootRight);
		EXPECT_TRUE(HashAnimation("graphdeleting", 13) == Anim::kGraphDeleting);

		// Case-folding, null, and empty string safety (no crash / deterministic basis hash)
		const auto basis = HashAnimation("", 0);
		EXPECT_TRUE(HashAnimation(nullptr, 0) == basis);
		EXPECT_TRUE(HashAnimation(nullptr, 12) == basis);
		EXPECT_TRUE(HashAnimation(std::string_view{}) == basis);
		EXPECT_TRUE(HashAnimation(std::string_view("JumpUp")) == Anim::kUp);
		EXPECT_TRUE(HashAnimation(std::string_view("JUMPFALLDIRECTIONAL")) == Anim::kFallDirectional);
		EXPECT_TRUE(HashAnimation(std::string_view("JumpDown")) == Anim::kDown);
		EXPECT_TRUE(HashAnimation(std::string_view("JumpLandEnd")) == Anim::kLandEnd);
		EXPECT_TRUE(HashAnimation(std::string_view("FootLeft")) == Anim::kFootLeft);
		EXPECT_TRUE(HashAnimation(std::string_view("FootRight")) == Anim::kFootRight);
		EXPECT_TRUE(HashAnimation(std::string_view("GraphDeleting")) == Anim::kGraphDeleting);
	}

	void TestVectorMath()
	{
		constexpr float kEps = 1e-4f;
		constexpr float kHalfPi = 1.5707963f;
		constexpr float kPi = 3.14159265f;

		const Vector origin = _mm_setr_ps(0.0f, 0.0f, 0.0f, 0.0f);
		const Vector p345 = _mm_setr_ps(3.0f, 4.0f, 99.0f, 0.0f);
		EXPECT_NEAR(CalcDistance(p345, origin, false), 5.0f, kEps);

		const Vector p122 = _mm_setr_ps(1.0f, 2.0f, 2.0f, 0.0f);
		EXPECT_NEAR(CalcDistance(p122, origin, true), 3.0f, kEps);

		// Zero-distance checks must never yield NaN
		const float zero2D = CalcAngle2D(origin, origin);
		const float zero3D = CalcAngle3D(origin, origin);
		EXPECT_TRUE(std::isfinite(zero2D));
		EXPECT_TRUE(std::isfinite(zero3D));
		EXPECT_NEAR(zero2D, 0.0f, kEps);
		EXPECT_NEAR(zero3D, 0.0f, kEps);

		EXPECT_NEAR(CalcAngle2D(_mm_setr_ps(0.0f, 10.0f, 0.0f, 0.0f), origin), 0.0f, kEps);
		EXPECT_NEAR(CalcAngle2D(_mm_setr_ps(10.0f, 0.0f, 0.0f, 0.0f), origin), kHalfPi, kEps);
		EXPECT_NEAR(CalcAngle2D(_mm_setr_ps(0.0f, -10.0f, 0.0f, 0.0f), origin), kPi, kEps);

		EXPECT_NEAR(CalcAngle3D(_mm_setr_ps(10.0f, 0.0f, 0.0f, 0.0f), origin), 0.0f, kEps);
		EXPECT_NEAR(CalcAngle3D(_mm_setr_ps(0.0f, 0.0f, 10.0f, 0.0f), origin), kHalfPi, kEps);

		// Relative strafe angle: straight jumps in all 4 cardinal headings must yield ~0 rad
		EXPECT_NEAR(CalcRelativeStrafeAngle(_mm_setr_ps(0.0f, 100.0f, 0.0f, 0.0f), origin, 0.0f), 0.0f, kEps);
		EXPECT_NEAR(CalcRelativeStrafeAngle(_mm_setr_ps(100.0f, 0.0f, 0.0f, 0.0f), origin, kHalfPi), 0.0f, kEps);
		EXPECT_NEAR(CalcRelativeStrafeAngle(_mm_setr_ps(0.0f, -100.0f, 0.0f, 0.0f), origin, kPi), 0.0f, kEps);
		EXPECT_NEAR(CalcRelativeStrafeAngle(_mm_setr_ps(-100.0f, 0.0f, 0.0f, 0.0f), origin, 3.0f * kHalfPi), 0.0f, kEps);

		// Relative strafe angle: 45 deg and 90 deg strafes regardless of facing direction
		EXPECT_NEAR(CalcRelativeStrafeAngle(_mm_setr_ps(100.0f, -100.0f, 0.0f, 0.0f), origin, kPi), 0.78539816f, kEps);
		EXPECT_NEAR(CalcRelativeStrafeAngle(_mm_setr_ps(100.0f, 0.0f, 0.0f, 0.0f), origin, 0.0f), kHalfPi, kEps);
		EXPECT_NEAR(CalcRelativeStrafeAngle(_mm_setr_ps(0.0f, 100.0f, 0.0f, 0.0f), origin, kHalfPi), kHalfPi, kEps);
		EXPECT_NEAR(CalcRelativeStrafeAngle(origin, origin, kPi), 0.0f, kEps);
	}

	void TestSettings(const std::filesystem::path& a_projectRoot)
	{
		const auto tempDir = std::filesystem::temp_directory_path();

		{
			const auto [log, ok] = Json2Settings::load_settings((tempDir / "bhos_nonexistent_settings.json").string());
			EXPECT_TRUE(!ok);
			EXPECT_TRUE(!log.empty());
		}

		{
			ScopedTempFile badJson{ tempDir / "bhos_test_malformed.json" };
			{
				std::ofstream out(badJson.path);
				out << "{ not valid json";
			}
			const auto [log, ok] = Json2Settings::load_settings(badJson.path.string());
			EXPECT_TRUE(!ok);
			EXPECT_TRUE(!log.empty());
		}

		{
			ScopedTempFile mismatchJson{ tempDir / "bhos_test_mismatch.json" };
			{
				std::ofstream out(mismatchJson.path);
				out << R"({"maxSpeed": "wrong_type", "misstepAllowed": 12.5})";
			}
			const auto [log, ok] = Json2Settings::load_settings(mismatchJson.path.string());
			EXPECT_TRUE(!ok);
			EXPECT_TRUE(log.find("maxSpeed") != std::string::npos);
		}

		{
			ScopedTempFile bomJson{ tempDir / "bhos_test_bom.json" };
			{
				std::ofstream out(bomJson.path, std::ios::binary);
				out << "\xEF\xBB\xBF" R"({"maxSpeed": 512.0})";
			}
			const auto [log, ok] = Json2Settings::load_settings(bomJson.path.string());
			EXPECT_TRUE(ok);
			EXPECT_NEAR(*Settings::maxSpeed, 512.0f, 1e-5f);
		}

		const auto configPath = a_projectRoot / "BunnyHopperOfSkyrim.json";
		EXPECT_TRUE(std::filesystem::exists(configPath));
		if (std::filesystem::exists(configPath)) {
			const auto [dumpLog, ok] = Json2Settings::load_settings(configPath.string(), true);
			EXPECT_TRUE(ok);
			EXPECT_TRUE(!dumpLog.empty());
			EXPECT_NEAR(*Settings::globalSpeedMult, 1.0f, 1e-5f);
			EXPECT_NEAR(*Settings::maxSpeed, 450.0f, 1e-5f);
			EXPECT_TRUE(*Settings::misttepAllowed == 4);
			EXPECT_NEAR(*Settings::ramSpeedThreshold, 220.0f, 1e-5f);
			EXPECT_TRUE(*Settings::crouchBoostCooldown == 6);
			EXPECT_TRUE(*Settings::enableJumpFeedback == true);
			EXPECT_TRUE(*Settings::enableFovZoom == true);
			EXPECT_TRUE(*Settings::enableTremble == false);
			EXPECT_TRUE(*Settings::enableHopCounter == false);
			EXPECT_TRUE(*Settings::enableHighscoreCounter == false);
			EXPECT_TRUE(*Settings::hopCounterHideWhenIdle == true);
			EXPECT_NEAR(*Settings::hopCounterPosX, 50.0f, 1e-5f);
			EXPECT_NEAR(*Settings::hopCounterPosY, 14.0f, 1e-5f);
			EXPECT_NEAR(*Settings::hopCounterScale, 1.2f, 1e-5f);
			EXPECT_NEAR(*Settings::highscoreCounterPosX, 50.0f, 1e-5f);
			EXPECT_NEAR(*Settings::highscoreCounterPosY, 20.0f, 1e-5f);
			EXPECT_NEAR(*Settings::highscoreCounterScale, 1.0f, 1e-5f);
			EXPECT_TRUE(*Settings::hopHighscore == 0);
		}

		// Save/Reset roundtrip preserving unrelated keys and persistent highscore
		{
			ScopedTempFile saveJson{ tempDir / "bhos_test_save.json" };
			{
				std::ofstream out(saveJson.path, std::ios::binary);
				out << R"({"customUserNote": "keep-me", "maxSpeed": 300.0})";
			}
			const auto [loadErr, loadedOk] = Json2Settings::load_settings(saveJson.path.string());
			EXPECT_TRUE(loadedOk);
			EXPECT_TRUE(loadErr.empty());
			EXPECT_NEAR(*Settings::maxSpeed, 300.0f, 1e-5f);

			Settings::globalSpeedMult.Set(1.75f);
			Settings::enableTremble.Set(true);
			Settings::enableHopCounter.Set(true);
			Settings::hopCounterScale.Set(1.8f);
			Settings::hopHighscore.Set(27);
			const auto [saveErr, savedOk] = Json2Settings::save_settings(saveJson.path.string());
			EXPECT_TRUE(savedOk);
			EXPECT_TRUE(saveErr.empty());

			Json2Settings::reset_defaults();
			EXPECT_NEAR(*Settings::globalSpeedMult, 1.0f, 1e-5f);
			EXPECT_NEAR(*Settings::maxSpeed, 450.0f, 1e-5f);
			EXPECT_TRUE(*Settings::enableTremble == false);
			EXPECT_TRUE(*Settings::enableHopCounter == false);
			EXPECT_NEAR(*Settings::hopCounterScale, 1.2f, 1e-5f);
			EXPECT_TRUE(*Settings::hopHighscore == 27);

			Settings::hopHighscore.Set(42);
			const auto [singleErr, singleOk] = Json2Settings::save_single_setting(saveJson.path.string(), Settings::hopHighscore);
			EXPECT_TRUE(singleOk);
			EXPECT_TRUE(singleErr.empty());

			const auto [reloadErr, reloadOk] = Json2Settings::load_settings(saveJson.path.string());
			EXPECT_TRUE(reloadOk);
			EXPECT_TRUE(reloadErr.empty());
			EXPECT_NEAR(*Settings::globalSpeedMult, 1.75f, 1e-5f);
			EXPECT_NEAR(*Settings::maxSpeed, 300.0f, 1e-5f);
			EXPECT_TRUE(*Settings::enableTremble == true);
			EXPECT_TRUE(*Settings::enableHopCounter == true);
			EXPECT_NEAR(*Settings::hopCounterScale, 1.8f, 1e-5f);
			EXPECT_TRUE(*Settings::hopHighscore == 42);

			std::ifstream verifyRaw(saveJson.path, std::ios::binary);
			const std::string savedContent(
				(std::istreambuf_iterator<char>(verifyRaw)),
				std::istreambuf_iterator<char>());
			EXPECT_TRUE(savedContent.find("\"customUserNote\": \"keep-me\"") != std::string::npos);

			Settings::hopHighscore.ResetToDefault();
			Json2Settings::reset_defaults();
			EXPECT_TRUE(*Settings::hopHighscore == 0);
			EXPECT_TRUE(!Settings::globalSpeedMult.Key().empty());
			Settings::globalSpeedMult.Set(std::numeric_limits<float>::quiet_NaN());
			EXPECT_NEAR(*Settings::globalSpeedMult, 1.0f, 1e-5f);
			Settings::globalSpeedMult.Set(std::numeric_limits<float>::infinity());
			EXPECT_NEAR(*Settings::globalSpeedMult, 1.0f, 1e-5f);
		}
	}

	void TestHopStreakTracker()
	{
		HopStreakTracker tracker;
		tracker.SetHighscore(2);
		EXPECT_TRUE(tracker.GetCurrentStreak() == 0);
		EXPECT_TRUE(tracker.GetHighscore() == 2);

		// Landing without a pending jump must not count
		EXPECT_TRUE(!tracker.CompleteHop());
		EXPECT_TRUE(tracker.GetCurrentStreak() == 0);

		tracker.OnJumping();
		EXPECT_TRUE(!tracker.CompleteHop());
		EXPECT_TRUE(tracker.GetCurrentStreak() == 1);
		EXPECT_TRUE(tracker.GetHighscore() == 2);
		EXPECT_TRUE(!tracker.IsNewRecordActive());

		// Duplicate landing call before another jump must not double count
		EXPECT_TRUE(!tracker.CompleteHop());
		EXPECT_TRUE(tracker.GetCurrentStreak() == 1);

		tracker.OnJumping();
		EXPECT_TRUE(!tracker.CompleteHop());
		EXPECT_TRUE(tracker.GetCurrentStreak() == 2);
		EXPECT_TRUE(tracker.GetHighscore() == 2);
		EXPECT_TRUE(!tracker.IsNewRecordActive());

		tracker.OnJumping();
		EXPECT_TRUE(tracker.CompleteHop());
		EXPECT_TRUE(tracker.GetCurrentStreak() == 3);
		EXPECT_TRUE(tracker.GetHighscore() == 3);
		EXPECT_TRUE(tracker.IsNewRecordActive());

		tracker.EndChain(5000);
		EXPECT_TRUE(tracker.GetCurrentStreak() == 0);
		EXPECT_TRUE(tracker.GetLastEndedStreak() == 3);
		EXPECT_TRUE(tracker.GetStreakEndTimeMs() == 5000);
		EXPECT_TRUE(tracker.IsNewRecordActive());

		// Starting a new chain resets the "new record" badge until record is beaten again
		tracker.OnJumping();
		EXPECT_TRUE(!tracker.CompleteHop());
		EXPECT_TRUE(tracker.GetCurrentStreak() == 1);
		EXPECT_TRUE(tracker.GetHighscore() == 3);
		EXPECT_TRUE(!tracker.IsNewRecordActive());

		tracker.Invalidate();
		EXPECT_TRUE(tracker.GetCurrentStreak() == 0);
		EXPECT_TRUE(tracker.GetHighscore() == 3);

		tracker.ResetHighscore();
		EXPECT_TRUE(tracker.GetHighscore() == 0);
	}

	struct RuntimeTarget
	{
		const char*            label;
		REL::Version           version;
		REL::Module::Runtime   runtime;
		REL::IDDB::Format      format;
		std::filesystem::path  binPath;
		std::filesystem::path  exePath;
		std::uint64_t          compileAndRunId;
	};

	void TestAddressLibraryMatrix(const std::filesystem::path& a_toolchainRoot)
	{
		const auto addrDir = a_toolchainRoot / "address Librarys";
		const auto refRoot = a_toolchainRoot.parent_path() / "_references";

		const std::vector<RuntimeTarget> targets = {
			{
				"1.5.97.0 (SE)",
				REL::Version{ 1, 5, 97, 0 },
				REL::Module::Runtime::SE,
				REL::IDDB::Format::SSEv1,
				addrDir / "version-1-5-97-0.bin",
				{},
				21416
			},
			{
				"1.6.659.0 (GOG AE)",
				REL::Version{ 1, 6, 659, 0 },
				REL::Module::Runtime::AE,
				REL::IDDB::Format::SSEv2,
				addrDir / "versionlib-1-6-659-0.bin",
				{},
				21890
			},
			{
				"1.6.1170.0 (Steam AE)",
				REL::Version{ 1, 6, 1170, 0 },
				REL::Module::Runtime::AE,
				REL::IDDB::Format::SSEv2,
				addrDir / "versionlib-1-6-1170-0.bin",
				{},
				441582
			},
			{
				"1.6.1179.0 (GOG AE)",
				REL::Version{ 1, 6, 1179, 0 },
				REL::Module::Runtime::AE,
				REL::IDDB::Format::SSEv2,
				addrDir / "versionlib-1-6-1179-0.bin",
				{},
				441582
			},
			{
				"1.7.104.0 (Primary AE)",
				REL::Version{ 1, 7, 104, 0 },
				REL::Module::Runtime::AE,
				REL::IDDB::Format::SSEv5,
				refRoot / "adresslibrary" / "versionlib-1-7-104-0.bin",
				refRoot / "runtime_1_7_104_0" / "SkyrimSE.exe",
				441582
			}
		};

		for (const auto& t : targets) {
			EXPECT_TRUE(std::filesystem::exists(t.binPath));
			if (!std::filesystem::exists(t.binPath)) {
				std::cerr << "[FAIL] Missing Address Library binary for " << t.label << ": " << t.binPath << '\n';
				continue;
			}

			if (!t.exePath.empty()) {
				EXPECT_TRUE(std::filesystem::exists(t.exePath));
				if (const auto ver = REL::GetFileVersion(t.exePath.wstring())) {
					EXPECT_TRUE(*ver == t.version);
				} else {
					std::cerr << "[FAIL] Could not read PE version from " << t.exePath << '\n';
					++g_failures;
				}
			}

			const bool moduleReady = REL::Module::mock(t.version, t.runtime, L"SkyrimSE.exe", 0x1000);
			EXPECT_TRUE(moduleReady);
			if (!moduleReady) {
				continue;
			}

			const bool injected = REL::IDDB::inject(t.binPath.wstring(), t.format, t.version);
			EXPECT_TRUE(injected);
			if (!injected) {
				std::cerr << "[FAIL] Failed to inject Address Library for " << t.label << '\n';
				REL::Module::reset();
				continue;
			}

			const auto playerOff = REL::RelocationID(517014, 403521).offset();
			const auto playerVtableOff = RE::VTABLE_PlayerCharacter[0].offset();
			const auto cameraOff = REL::RelocationID(514642, 400802).offset();
			const auto firstPersonVtableOff = RE::VTABLE_FirstPersonState[0].offset();
			const auto mainOff = REL::RelocationID(516943, 403449).offset();
			const auto eventHolderOff = REL::RelocationID(14108, 14298).offset();
			const auto factoriesOff = REL::RelocationID(514355, 400508).offset();
			const auto initOff = REL::RelocationID(514349, 400503).offset();
			const auto ghostOff = REL::RelocationID(36286, 37275).offset();
			const auto ragdollOff = REL::RelocationID(36492, 37491).offset();
			const auto mountedByOff = REL::RelocationID(37758, 38703).offset();
			const auto knockExplosionOff = REL::RelocationID(38858, 39895).offset();
			const auto isInMidairOff = REL::RelocationID(36259, 37243).offset();
			const auto processListsOff = REL::RelocationID(514167, 400315).offset();
			const auto compileAndRunOff = REL::IDDB::get().id2offset(t.compileAndRunId);
			const auto compileAndRunExprOff = REL::RelocationID(21416, AE_CHECK(SKSE::RUNTIME_SSE_1_6_1130, 21890, 441582)).offset();

			EXPECT_TRUE(playerOff != 0);
			EXPECT_TRUE(playerVtableOff != 0);
			EXPECT_TRUE(cameraOff != 0);
			EXPECT_TRUE(firstPersonVtableOff != 0);
			EXPECT_TRUE(mainOff != 0);
			EXPECT_TRUE(eventHolderOff != 0);
			EXPECT_TRUE(factoriesOff != 0);
			EXPECT_TRUE(initOff != 0);
			EXPECT_TRUE(ghostOff != 0);
			EXPECT_TRUE(ragdollOff != 0);
			EXPECT_TRUE(mountedByOff != 0);
			EXPECT_TRUE(knockExplosionOff != 0);
			EXPECT_TRUE(isInMidairOff != 0);
			EXPECT_TRUE(processListsOff != 0);
			EXPECT_TRUE(compileAndRunOff != 0);
			EXPECT_TRUE(compileAndRunOff == compileAndRunExprOff);

			if (t.version == REL::Version{ 1, 7, 104, 0 }) {
				EXPECT_TRUE(playerOff == 0x3230778);
				EXPECT_TRUE(cameraOff == 0x31A5478);
				EXPECT_TRUE(eventHolderOff == 0x1DA8F0);
				EXPECT_TRUE(factoriesOff == 0x21A3810);
				EXPECT_TRUE(initOff == 0x21A3800);
				EXPECT_TRUE(ghostOff == 0x6756B0);
				EXPECT_TRUE(ragdollOff == 0x687C20);
				EXPECT_TRUE(mountedByOff == 0x6D33D0);
				EXPECT_TRUE(knockExplosionOff == 0x7237C0);
				EXPECT_TRUE(compileAndRunOff == 0x343D00);
			}

			std::cout << "[PASS] " << t.label
					  << " Player=" << std::hex << playerOff
					  << " Camera=" << cameraOff
					  << " Main=" << mainOff
					  << " KnockExp=" << knockExplosionOff
					  << " Midair=" << isInMidairOff
					  << " ProcLists=" << processListsOff
					  << std::dec << '\n';
			std::cout.flush();
			REL::Module::reset();
		}
	}

	void TestImmediateSyncHelpers()
	{
		EXPECT_TRUE(ClampCrouchCooldown(20, 3) == 3);
		EXPECT_TRUE(ClampCrouchCooldown(2, 10) == 2);
		EXPECT_TRUE(ClampCrouchCooldown(5, 0) == 0);
		EXPECT_TRUE(ClampCrouchCooldown(5, -4) == 0);
		EXPECT_TRUE(ClampCrouchCooldown(-2, 6) == 0);

		float firstNudge = 0.0f;
		EXPECT_TRUE(ComputeSafeCarryWeightNudge(150.0f, 300.0f, firstNudge));
		EXPECT_TRUE(firstNudge > 0.0f);

		EXPECT_TRUE(ComputeSafeCarryWeightNudge(320.0f, 300.0f, firstNudge));
		EXPECT_TRUE(firstNudge < 0.0f);

		firstNudge = 42.0f;
		EXPECT_TRUE(!ComputeSafeCarryWeightNudge(300.0f, 300.0f, firstNudge));
		EXPECT_TRUE(!ComputeSafeCarryWeightNudge(std::numeric_limits<float>::quiet_NaN(), 300.0f, firstNudge));
		EXPECT_TRUE(!ComputeSafeCarryWeightNudge(100.0f, std::numeric_limits<float>::infinity(), firstNudge));
		EXPECT_NEAR(firstNudge, 42.0f, 1e-6f);
	}

	void TestFrameGuardState()
	{
		EXPECT_TRUE(EvaluateGroundChainState(false, true, false, false, 1000, 1000) == FrameGuardAction::kKeepActive);
		EXPECT_TRUE(EvaluateGroundChainState(true, true, true, true, 0, 0) == FrameGuardAction::kReset);
		EXPECT_TRUE(EvaluateGroundChainState(true, false, true, false, 9999, 9999) == FrameGuardAction::kKeepActive);
		EXPECT_TRUE(EvaluateGroundChainState(true, false, false, true, kMaxGroundedChainMs - 1, 0) == FrameGuardAction::kKeepActive);
		EXPECT_TRUE(EvaluateGroundChainState(true, false, false, true, kMaxGroundedChainMs, 0) == FrameGuardAction::kReset);
		EXPECT_TRUE(EvaluateGroundChainState(true, false, false, false, kMaxStationaryChainMs - 1, kMaxStationaryChainMs - 1) == FrameGuardAction::kKeepActive);
		EXPECT_TRUE(EvaluateGroundChainState(true, false, false, false, kMaxStationaryChainMs, kMaxStationaryChainMs) == FrameGuardAction::kReset);
	}

	std::filesystem::path FindProjectRoot(const int a_argc, char* a_argv[])
	{
		if (a_argc >= 3) {
			std::filesystem::path candidate(a_argv[2]);
			if (std::filesystem::exists(candidate / "BunnyHopperOfSkyrim.json")) {
				return candidate;
			}
		}

		std::vector<std::filesystem::path> probes = {
			std::filesystem::current_path(),
			std::filesystem::weakly_canonical(std::filesystem::path(a_argv[0])).parent_path()
		};
		for (auto dir : probes) {
			for (int depth = 0; depth < 4 && !dir.empty(); ++depth) {
				if (std::filesystem::exists(dir / "BunnyHopperOfSkyrim.json")) {
					return dir;
				}
				dir = dir.parent_path();
			}
		}
		return std::filesystem::current_path();
	}
}

int main(int a_argc, char* a_argv[])
{
	std::filesystem::path toolchainRoot;
	if (a_argc >= 2) {
		toolchainRoot = std::filesystem::path(a_argv[1]);
	} else {
		const auto projectRoot = FindProjectRoot(a_argc, a_argv);
		toolchainRoot = projectRoot.parent_path() / "_toolchain";
	}
	const auto projectRoot = FindProjectRoot(a_argc, a_argv);

	TestAnimations();
	TestVectorMath();
	TestSettings(projectRoot);
	TestHopStreakTracker();
	TestImmediateSyncHelpers();
	TestFrameGuardState();
	TestAddressLibraryMatrix(toolchainRoot);

	if (g_failures != 0) {
		std::cerr << "bunnyhopper-tests FAILED with " << g_failures << " error(s)\n";
		return 1;
	}

	std::cout << "bunnyhopper-tests PASS\n";
	return 0;
}
