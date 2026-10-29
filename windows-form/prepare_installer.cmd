@echo off
cd /d "%~dp0"

echo Preparing installer files...
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\smart-install-maker\prepare_package.ps1"

echo.
if errorlevel 1 (
    echo ERROR: Package was not prepared.
) else (
    echo DONE.
    echo Open: installer\smart-install-maker\package
)

echo.
pause
