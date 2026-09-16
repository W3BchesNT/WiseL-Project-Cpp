@echo off
chcp 65001 > nul
cls

echo ==================================================
echo   CHOOSE TARGET PLATFORM FOR WISELC BUILD
echo ==================================================
echo 1. Windows (MSVC cl.exe - Micro PE)
echo 2. Linux (Clang ELF64 - Optimized Binary)
echo ==================================================
set /p target="Enter choice (1 or 2): "

if "%target%"=="1" goto build_msvc
if "%target%"=="2" goto build_clang_linux
echo Invalid choice! Exiting.
pause
exit

:build_msvc
cls
echo [1/2] Initializing MSVC environment...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul
echo [2/2] Compiling wiselc.exe via MSVC...
cl /nologo /EHsc /O1 /GL /MD kernel\main.cpp kernel\lexer.cpp kernel\parser.cpp kernel\codegen.cpp /Fewiselc.exe /link /INCREMENTAL:NO /OPT:REF /OPT:ICF > nul
del /q *.obj > nul 2>&1
echo --------------------------------------------------
echo Windows Build Done!
echo PE binary: %cd%\wiselc.exe
pause
exit

:build_clang_linux
cls
echo [1/1] Compiling wiselc (ELF64) via Clang...
clang++ kernel/main.cpp kernel/lexer.cpp kernel/parser.cpp kernel/codegen.cpp -o wiselc -std=c++17 -Oz -fno-rtti -fno-exceptions "-Wl,/NODEFAULTLIB:libcmt" "-Wl,/DEFAULTLIB:msvcrt" "-Wl,/OPT:REF"
echo --------------------------------------------------
echo Linux ELF64 Build Done!
echo ELF binary path: %cd%\wiselc
pause
exit
