rem Generate dummy lib exports

python scripts/GenerateExports.py


rem Generate unit tests

python scripts/GenerateTests.py


rem Generate projects

"../modules/premake/bin/release/premake5.exe" vs2019

del Makefile
del UnitTestingBinary.make
del mimalloc.make
"../modules/premake/bin/release/premake5.exe" gmake2 --generate_wasm