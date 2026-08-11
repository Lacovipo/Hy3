@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
del /Q *.obj perft_test.exe 2>nul
cl /EHsc /O2 /std:c++17 /I src tests\perft_test.cpp src\board.cpp src\movegen.cpp src\perft.cpp /Fe: perft_test.exe
if %errorlevel%==0 (
  echo BUILD OK
  perft_test.exe
) else (
  echo BUILD FAILED
)