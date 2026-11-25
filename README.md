# BunnyHopper of Skyrim (Universal SE/AE)

[![Platform: Windows](https://img.shields.io/badge/Platform-Windows-737373?style=for-the-badge)](https://learn.microsoft.com/en-us/windows/)
[![Skyrim: 1.7.104.0](https://img.shields.io/badge/Skyrim-1.7.104.0-007ec6?style=for-the-badge)](#target-runtimes--compatibility-status)
[![Edition: Universal SE / AE](https://img.shields.io/badge/Edition-Universal%20SE%20%2F%20AE-00599C?style=for-the-badge)](#target-runtimes--compatibility-status)
[![SKSE: 2.3.1](https://img.shields.io/badge/SKSE-2.3.1-e87039?style=for-the-badge)](https://skse.silverlock.org/)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-78b900?style=for-the-badge)](LICENSE)

Native SKSE64 C++23 plugin bringing momentum-based bunnyhopping, air strafing, slope/drop launches, crouch boosts, high-speed NPC/creature ramming knockback, optional first-person landing feedback, and optional in-game settings/HUD streak counters to Skyrim SE and AE in a single universal DLL (no `.esp`/`.esl` plugin required).

## Target Runtimes & Compatibility Status

* **Mandatory Primary Target:** **Skyrim AE 1.7.104.0** (SKSE64 2.3.1, Address Library Format 5)
* **Universal Baseline:** One multi-runtime Flat-Rim DLL (`ENABLE_SKYRIM_SE=1`, `ENABLE_SKYRIM_AE=1`, `ENABLE_SKYRIM_VR=0`; `StructsPost629` + `AddressLibraryV5`)

| Runtime | Edition | Address Library | Status |
| :--- | :--- | :--- | :--- |
| **1.7.104.0** | Skyrim AE (Primary) | Format 5 | Statically verified (Address Library + executable); **User confirmed** |
| **1.6.1170.0** | Skyrim AE (Steam) | Format 2 | Statically verified (Address Library); **Requires in-game test** |
| **1.6.1179.0** | Skyrim AE (GOG) | Format 2 | Statically verified (Address Library); **Requires in-game test** |
| **1.6.659.0** | Skyrim AE (GOG) | Format 2 | Statically verified (Address Library); **Requires in-game test** |
| **1.5.97.0** | Skyrim SE | Format 1 | Statically verified (Address Library); **Requires in-game test** |

*Skyrim VR is not targeted.*

## Requirements (End Users)

* **Required:**
  * Matching **SKSE64** for your Skyrim runtime (e.g. SKSE 2.3.1 for 1.7.104.0)
  * **Address Library for SKSE Plugins**
  * Microsoft Visual C++ Redistributable x64 (`MSVCP140` / `VCRUNTIME140`)
* **Optional:**
  * **SKSE Menu Framework** — detected softly at runtime (`GetModuleHandleW` / `GetProcAddress`, no hard DLL import). Enables the live in-game settings UI and optional **Bunnyhop Streak** & **Highscore** HUD overlay. Without SMF installed, the plugin runs normally via `Data\SKSE\Plugins\BunnyHopperOfSkyrim.json`.
* **Original Assets Required:** None (standalone native SKSE plugin + JSON config).

## Compatibility Architecture

* Ported to **CommonLibSSE-NG v16.11.1** (`https://github.com/alandtse/CommonLibSSE-NG.git`, commit `5298302831d03a01b3b406f4b7b8128ee3bd7a44`).
* Engine bindings validated across all five Address Library databases via `bunnyhopper-tests`, including:
  * `VTABLE_PlayerCharacter[0]` (`0x0AD` frame hook for HUD/visual ticks, deferred graph registration, and jump-only speed watchdog)
  * `VTABLE_FirstPersonState[0]` / `PlayerCamera`
  * `RELOCATION_ID(38858, 39895)` (`AIProcess::KnockExplosion` direct upward-forward ram knockback)
  * `RELOCATION_ID(36259, 37243)` (`Actor::IsInMidair`)
  * `RELOCATION_ID(514167, 400315)` (`RE::ProcessLists::GetSingleton`)
  * `RELOCATION_ID(517014, 403521)`, `RELOCATION_ID(514642, 400802)`, `RELOCATION_ID(516943, 403449)`, `RELOCATION_ID(14108, 14298)`

## Reproducible Build Instructions

### Build Requirements
* MSVC x64 toolset with C++23 support (Visual Studio 2022/2026)
* CMake >= 3.24 and Ninja
* **CommonLibSSE-NG v16.11.1** (`5298302831d03a01b3b406f4b7b8128ee3bd7a44`) built with `spdlog`, `fmt`, `rapidcsv`, and `DirectXMath`
* **nlohmann/json** (`3.11.3`)
* **SKSE Menu Framework 3 API** header (`SKSEMenuFramework.h`)

### Configure, Build & Test (CMake + Ninja)

```powershell
cmake -S . -B build/release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DBHOS_COMMONLIB_ROOT="path/to/CommonLibSSE-NG-v16.11.1" `
  -DBHOS_JSON_INCLUDE="path/to/nlohmann-json/include" `
  -DBHOS_SMF_INCLUDE="path/to/SKSE-Menu-Framework-3-API" `
  -DCMAKE_PREFIX_PATH="path/to/deps;path/to/vcpkg-installed/x64-windows-static-md"

cmake --build build/release --target BunnyHopperOfSkyrim bunnyhopper-tests --parallel
```

When building inside the workspace SKSE standard, `Build-Phase1.bat` / `skse-build.json` invoke `_toolchain\SKSEBuildStandard` with deterministic path mapping (`/experimental:deterministic`, `/pathmap`, `/Brepro`, `/DEBUG:FULL`, `/PDBALTPATH:BunnyHopperOfSkyrim.pdb`).

## Attribution & License

* Original **BunnyHopperOfSkyrim** mod: **Copyright (c) 2020 DK** (originally MIT License; preserved in `NOTICE`).
* This Universal SE/AE port is licensed under **GNU GPL v3.0 or later (`GPL-3.0-or-later`)** (see [LICENSE](LICENSE)) together with the **Modding Exception** and **GPL-3.0 Linking Exception** from CommonLibSSE-NG v16.11.1 (see [EXCEPTIONS.md](EXCEPTIONS.md)).
