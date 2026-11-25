$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath($env:SKSE_PROJECT_ROOT)
$toolchainRoot = [IO.Path]::GetFullPath($env:SKSE_TOOLCHAIN_ROOT)

Write-Host "BunnyHopperOfSkyrim prebuild: toolchain=$toolchainRoot"

foreach ($required in @(
    'CommonLibSSE-NG-v16.11.1\include\SKSE\SKSE.h',
    'nlohmann-json-3.11.3\include\nlohmann\json.hpp',
    'SKSE-Menu-Framework-3-API\SKSEMenuFramework.h',
    'skyrim_1_7_104_compat\deps-msvc1951\lib\spdlog.lib',
    'skyrim_1_7_104_compat\deps-msvc1951\lib\fmt.lib',
    'skyrim_1_7_104_compat\deps-msvc1951\include\rapidcsv.h',
    'vcpkg-installed\x64-windows-static-md\include\directxmath\DirectXMath.h'
)) {
    $full = Join-Path $toolchainRoot $required
    if (!(Test-Path -LiteralPath $full -PathType Leaf)) {
        throw "Required shared dependency missing: $full"
    }
}
Write-Host "BunnyHopperOfSkyrim prebuild PASS"
