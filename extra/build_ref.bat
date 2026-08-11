@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
del /Q *.obj refcheck.exe 2>nul
cl /EHsc /O2 /std:c++17 /I src tests\refcheck.cpp src\board.cpp src\movegen.cpp src\perft.cpp /Fe: refcheck.exe
if %errorlevel%==0 ( refcheck.exe ) else ( echo BUILD FAILED )