@echo off
setlocal enabledelayedexpansion
pushd "%~dp0.."
set "ROOT=%CD%\"
popd
set "DEPLOY_DIR=%~dp0"
set "PACK_DIR=%ROOT%pack"
set "DIST_DIR=%ROOT%dist"
set "SDK_DIR=D:\pyProj\HUAWEISDK"

echo ========================================
echo   test_runner_ui - Pack
echo ========================================
echo.
if not exist "%ROOT%config.json" (echo [ERROR] config.json not found & pause & exit /b 1)

:: Clean
if exist "%PACK_DIR%" rmdir /s /q "%PACK_DIR%"

:: Step 1: App
echo [1/4] Copy app...
mkdir "%PACK_DIR%\app" 2>nul
copy /y "%DIST_DIR%\test_runner_ui.exe" "%PACK_DIR%\app\" >nul
copy /y "%DIST_DIR%\*.dll" "%PACK_DIR%\app\" >nul 2>nul
echo   Done

:: Step 2: SDK
echo [2/4] Copy SDK...
if exist "%SDK_DIR%" (
    robocopy "%SDK_DIR%" "%PACK_DIR%\sdk" /e /njh /njs /ndl /np /mt:4 >nul
    echo   Done
) else (
    echo   [WARN] SDK not found: %SDK_DIR%
    mkdir "%PACK_DIR%\sdk" 2>nul
)

:: Step 3: Config fix + scripts
echo [3/4] Fix config...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0fix_conf.ps1" -ConfigPath "%ROOT%config.json" -PackDir "%PACK_DIR%" 2>nul
if not exist "%PACK_DIR%\config.json" copy "%ROOT%config.json" "%PACK_DIR%\config.json" >nul
(
echo @echo off
echo robocopy "%%~dp0app" "D:\test_runner_ui\app" /e /njh /njs /ndl /np ^>nul
echo robocopy "%%~dp0sdk" "D:\test_runner_ui\sdk" /e /njh /njs /ndl /np ^>nul
echo if not exist "D:\.SDKtest" mkdir "D:\.SDKtest"
echo copy /y "%%~dp0config.json" "D:\.SDKtest\config.json" ^>nul
echo start "" "D:\test_runner_ui\app\test_runner_ui.exe"
echo echo Done^^! ^& pause
) > "%PACK_DIR%\install.bat"
(
echo @echo off
echo if exist "D:\test_runner_ui" rmdir /s /q "D:\test_runner_ui"
echo if exist "D:\.SDKtest\config.json" del /q "D:\.SDKtest\config.json"
echo dir /b "D:\.SDKtest" 2^>nul ^| findstr "^" ^>nul ^|^| rmdir /q "D:\.SDKtest"
echo echo Done^^! ^& pause
) > "%PACK_DIR%\uninstall.bat"
echo   Done

:: Step 4: Compress
echo [4/4] Compressing...
set "OUTPUT=%DEPLOY_DIR%test_runner_ui_portable.zip"
if exist "%OUTPUT%" del /f "%OUTPUT%" 2>nul
where 7z.exe >nul 2>nul
if %errorlevel% equ 0 (
    echo   Using 7z...
    7z a -tzip -mx5 "%OUTPUT%" "%PACK_DIR%\*" >nul
) else (
    echo   Using PowerShell Compress-Archive...
    powershell -NoProfile -Command "Compress-Archive -Path '%PACK_DIR%\*' -DestinationPath '%OUTPUT%' -Force" 2>nul
)

if exist "%OUTPUT%" (for %%F in ("%OUTPUT%") do echo   Done: %%~zF bytes) else (echo   [ERROR] Failed to create zip & pause & exit /b 1)
echo ========================================
echo   Output: %OUTPUT%
echo ========================================
pause