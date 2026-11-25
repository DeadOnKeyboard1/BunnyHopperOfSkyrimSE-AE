$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath($env:SKSE_PROJECT_ROOT)
$toolchainRoot = [IO.Path]::GetFullPath($env:SKSE_TOOLCHAIN_ROOT)
$testExe = Join-Path $projectRoot 'build\release\bunnyhopper-tests.exe'

if (!(Test-Path -LiteralPath $testExe -PathType Leaf)) {
    throw "Missing unit test binary: $testExe"
}

Write-Host "Running unit tests..."
& $testExe $toolchainRoot $projectRoot
if ($LASTEXITCODE -ne 0) {
    throw "bunnyhopper-tests failed with exit code $LASTEXITCODE"
}

$privacyPy = Join-Path $toolchainRoot 'skyrim_1_7_104_compat\validation\check_private_paths.py'
if ($env:SKSE_BUILT_DLL -and (Test-Path -LiteralPath $privacyPy -PathType Leaf)) {
    python $privacyPy $env:SKSE_BUILT_DLL
    if ($LASTEXITCODE -ne 0) {
        throw "DLL private path scan failed"
    }
}
Write-Host "BunnyHopperOfSkyrim postbuild PASS"
