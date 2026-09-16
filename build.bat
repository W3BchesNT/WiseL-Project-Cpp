@echo off
set INCLUDE=kernel\FASM\FASM-WINDOWS\INCLUDE

wiselc.exe
if %errorlevel% neq 0 (
    echo [ERROR] WiseL compiler failed
    pause
    exit /b 1
)

kernel\FASM\FASM-WINDOWS\fasm.exe out.asm main.exe
if %errorlevel% neq 0 (
    echo [ERROR] FASM failed
    pause
    exit /b 1
)

echo ==========================================
main.exe
echo ==========================================
pause