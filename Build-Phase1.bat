@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0.") do set "MOD_DIR=%%~fI"
for %%I in ("%~dp0..") do set "WORKSPACE_ROOT=%%~fI"
for %%I in ("%MOD_DIR%") do set "MOD_NAME=%%~nxI"

set "BUILD_DRV="
for %%D in (X W V U T S R Q P N M) do (
  if not defined BUILD_DRV (
    if not exist "%%D:\" set "BUILD_DRV=%%D:"
  )
)

if not defined BUILD_DRV (
  echo ERROR: No free drive letter available for isolated privacy build mapping.
  exit /b 1
)

subst "%BUILD_DRV%" "%WORKSPACE_ROOT%" >nul
if errorlevel 1 (
  echo ERROR: Failed to map neutral build drive %BUILD_DRV%.
  exit /b 1
)

set "MAPPED_ROOT=%BUILD_DRV%\%MOD_NAME%"
set "SKSE_TOOLCHAIN_ROOT=%BUILD_DRV%\_toolchain"
if not exist "%MAPPED_ROOT%\build\tmp" mkdir "%MAPPED_ROOT%\build\tmp"
set "TEMP=%MAPPED_ROOT%\build\tmp"
set "TMP=%MAPPED_ROOT%\build\tmp"

pushd "%MAPPED_ROOT%"
call "%SKSE_TOOLCHAIN_ROOT%\Build-SKSE-Mod.cmd" "%MAPPED_ROOT%." %*
set "RC=%ERRORLEVEL%"
popd

subst "%BUILD_DRV%" /D >nul 2>&1
if exist "%MOD_DIR%\build\tmp" rd /s /q "%MOD_DIR%\build\tmp" >nul 2>&1
exit /b %RC%
