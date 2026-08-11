@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cl /EHsc /O2 /std:c++17 /I src tests\debug_moves.cpp src\board.cpp src\movegen.cpp /Fe: debug_moves.exe
if %errorlevel%==0 ( debug_moves.exe ) else ( echo BUILD FAILED )