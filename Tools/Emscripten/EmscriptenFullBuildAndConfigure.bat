REM *** Command Line Arguments *** 
REM [1] - Configuration - release || debug
echo:
echo ====================================
echo Git: Install githooks
echo ====================================
echo:
git config core.hooksPath .githooks
git config commit.template .githooks/commit-template.txt
echo githooks installed

IF NOT EXIST "../../modules/premake/README.md" (
echo:
echo ====================================
echo Git: submodule
echo ====================================
echo:
	git submodule update --recursive
)

IF NOT EXIST "../../modules/premake/bin/release/premake5.exe" (
echo:
echo ====================================
echo Bootstrap
echo ====================================
echo:
	cd ..\..\modules\premake
	call Bootstrap.bat
	cd ..\..\Tools\Emscripten
)

for /f "delims=" %%x in (emsdk_version.txt) do (
    set emsdk_version=%%x
    goto :Build
)

:Build
echo:
echo ====================================
echo Build: Verify args
echo ====================================
echo:
cd ../..
del Makefile

if "%~1"=="" (goto ArgError) else (goto ArgOk)

:ArgError
echo No configuration name provided
exit /b 1

:ArgOk
echo:
echo ====================================
echo Build: Call premake generate wasm
echo ====================================
echo:
"modules/premake/bin/release/premake5" gmake2 --generate_wasm
goto End

:End
echo:
echo ====================================
echo ReplaceComSpec
echo ====================================
echo:
python Tools/Emscripten/ReplaceComSpec.py

echo:
echo ====================================
echo Docker clean & run 
echo ====================================
echo:
docker run -w /src -v %cd%:/src --rm emscripten/emsdk:%emsdk_version% emmake make config=%~1_wasm clean
docker run -w /src -v %cd%:/src --rm emscripten/emsdk:%emsdk_version% emmake make -j 8 config=%~1_wasm
cd Tools/Emscripten
