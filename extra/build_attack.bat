@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
del /Q *.obj debug_attack.exe 2>nul
cl /EHsc /O2 /std:c++17 /I src tests\debug_attack.cpp src\board.cpp src\movegen.cpp /Fe: debug_attack.exe
if %errorlevel%==0 ( debug_attack.exe ) else ( echo BUILD FAILED )