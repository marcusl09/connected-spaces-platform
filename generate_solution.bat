@echo off
:: ---------------------------------------------------------------------
:: *** Command Line Arguments *** 
:: Project CSP
:: DLLOnly - Generates a solution with only DLL-related build configurations
:: ---------------------------------------------------------------------
echo:
echo =======================================================================
echo Install Python dependencies
echo =======================================================================
echo:

py -m pip install -r teamcity/requirements.txt

if "%~1"=="-ci" (goto :NoGitConfig) else goto :GitConfig

:GitConfig
echo:
echo =======================================================================
echo Configuring githooks
echo =======================================================================
echo:
git config core.hooksPath .githooks
git config commit.template .githooks/commit-template.txt

:NoGitConfig

IF NOT EXIST "modules/premake/README.md" (
echo:
echo =======================================================================
echo Git submodule
echo =======================================================================
echo:
	git submodule init
	git submodule update --recursive
	echo git submodule configured
)

IF NOT EXIST "modules/premake/bin/release/premake5.exe" (
echo:
echo =======================================================================
echo Bootstrap
echo =======================================================================
echo:
	cd modules/premake
	call Bootstrap.bat
	cd ../..
)

IF NOT EXIST "modules/googletest/build/ALL_BUILD.vcxproj" (
echo:
echo =======================================================================
echo Call cmake and msbuild
echo =======================================================================
echo:
	cd modules/googletest
	mkdir build
	cd build
	call cmake ..
	call msbuild ALL_BUILD.vcxproj /property:Configuration=Release
	cd ../../..
)

cd tools/wrappergenerator

echo:
echo =======================================================================
echo Npm install
echo =======================================================================
echo:
call npm install
if %errorlevel% GTR 0 (
  echo npm install failed!
  exit 1
)

cd ../..

echo:
echo =======================================================================
echo Build Solution vs2022 using premake5.exe
echo =======================================================================
echo:
"modules/premake/bin/release/premake5.exe" vs2022 %*
if %errorlevel% GTR 0 (
  echo build solution vs2022 using premake5 failed!
  exit 1
)

pause
