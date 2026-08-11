@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
del /Q *.obj hy3.exe 2>nul
cl /EHsc /O2 /std:c++17 /I src src\uci.cpp src\board.cpp src\movegen.cpp src\eval.cpp src\search.cpp /Fe: hy3.exe
if %errorlevel%==0 ( echo BUILD OK ) else ( echo BUILD FAILED )
