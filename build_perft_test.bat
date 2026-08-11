@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cl /nologo /EHsc /O2 /std:c++17 /I src tests\perft_test.cpp src\board.cpp src\movegen.cpp /Fe:perft_test.exe
