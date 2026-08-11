@echo off
REM build_release.bat - Compila el motor Hy3 1.7 en modo Release (raiz del proyecto)
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if exist "Hy3 1.7.exe" del /Q "Hy3 1.7.exe"
del /Q src\*.obj 2>nul
cl /EHsc /O2 /std:c++17 /I src src\uci.cpp src\board.cpp src\movegen.cpp src\eval.cpp src\search.cpp /Fe:"Hy3 1.7.exe"
if %errorlevel%==0 ( echo BUILD RELEASE OK -^> "Hy3 1.7.exe" ) else ( echo BUILD FAILED )
