@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   Pack script for test_runner_ui
echo ========================================
echo.

set "ROOT=%~dp0"
set "PACK_DIR=%ROOT%pack"
set "DIST_DIR=%ROOT%dist"

if not exist "%ROOT%config.json" (
    echo [ERROR] config.json not found in script dir
    pause & exit /b 1
)

:: cleanup old pack
if exist "%PACK_DIR%" rmdir /s /q "%PACK_DIR%"
mkdir "%PACK_DIR%\bin"
mkdir "%PACK_DIR%\models"
mkdir "%PACK_DIR%\app"

echo [1/5] Copy main app...
xcopy /y /q "%DIST_DIR%\test_runner_ui.exe" "%PACK_DIR%\app\" >nul
xcopy /y /q "%DIST_DIR%\*.dll" "%PACK_DIR%\app\" >nul 2>nul
echo   OK

:: call PowerShell script to collect deps and fix config
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0pack.ps1" -ConfigPath "%ROOT%config.json" -PackDir "%PACK_DIR%"

:: generate install.bat
(
echo @echo off
echo echo === test_runner_ui Installer ===
echo echo.
echo xcopy /y /e /q "%%~dp0app\*" "D:\test_runner_ui\app\" ^>nul
echo xcopy /y /e /q "%%~dp0bin\*" "D:\test_runner_ui\bin\" ^>nul
echo xcopy /y /e /q "%%~dp0models\*" "D:\test_runner_ui\models\" ^>nul
echo if not exist "D:\.SDKtest" mkdir "D:\.SDKtest"
echo copy /y "%%~dp0config.json" "D:\.SDKtest\config.json" ^>nul
echo echo.
echo echo Install done!
echo echo App: D:\test_runner_ui\app\test_runner_ui.exe
echo echo Config: D:\.SDKtest\config.json
echo echo.
echo start "" "D:\test_runner_ui\app\test_runner_ui.exe"
echo pause
) > "%PACK_DIR%\install.bat"

:: generate uninstall.bat
(
echo @echo off
echo echo === test_runner_ui Uninstaller ===
echo echo.
echo if exist "D:\test_runner_ui" rmdir /s /q "D:\test_runner_ui" ^&^& echo   Removed D:\test_runner_ui
echo if exist "D:\.SDKtest\config.json" del /q "D:\.SDKtest\config.json" ^>nul ^&^& echo   Removed config file
echo dir /b "D:\.SDKtest" 2^>nul ^| findstr "^" ^>nul ^|^| ^(rmdir /q "D:\.SDKtest" ^&^& echo   Removed empty D:\.SDKtest^)
echo echo.
echo echo Uninstall done!
echo pause
) > "%PACK_DIR%\uninstall.bat"

echo [5/5] Creating zip...
set "ZIP_NAME=%ROOT%test_runner_ui_portable.zip"
if exist "%ZIP_NAME%" del "%ZIP_NAME%"
powershell -NoProfile -Command "Compress-Archive -Path '%PACK_DIR%\*' -DestinationPath '%ZIP_NAME%' -Force" 2>nul
echo   OK

echo.
echo ========================================
echo   Pack complete!
echo   Output: %ZIP_NAME%
echo ========================================
echo.
echo Usage:
echo   1. Extract test_runner_ui_portable.zip
echo   2. Run install.bat to deploy
echo   3. Run uninstall.bat to clean up
echo.
pause