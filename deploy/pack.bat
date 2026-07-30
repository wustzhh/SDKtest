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
set "OUTPUT=%DEPLOY_DIR%test_runner_ui_portable.7z"
if exist "%OUTPUT%" del /f "%OUTPUT%" 2>nul

:: Find 7z: 1) deploy\7za.exe  2) system PATH
set "SZ="
if exist "%~dp07za.exe" set "SZ=%~dp07za.exe"
if "%SZ%"=="" where 7z.exe >nul 2>nul && set "SZ=7z.exe"
if "%SZ%"=="" where 7za.exe >nul 2>nul && set "SZ=7za.exe"
if "%SZ%"=="" (
    echo   [WARN] 7-Zip not found
    echo   Downloading 7za.exe (1.2MB)...
    powershell -NoProfile -Command "Invoke-WebRequest -Uri 'https://www.7-zip.org/a/7za920.zip' -OutFile '%TEMP%\7za.zip'" 2>nul
    powershell -NoProfile -Command "Expand-Archive -Path '%TEMP%\7za.zip' -DestinationPath '%TEMP%\7z_tmp' -Force" 2>nul
    if exist "%TEMP%\7z_tmp\7za.exe" (
        copy "%TEMP%\7z_tmp\7za.exe" "%~dp07za.exe" >nul
        set "SZ=%~dp07za.exe"
        del /q "%TEMP%\7za.zip" 2>nul & rmdir /s /q "%TEMP%\7z_tmp" 2>nul
    )
    if "%SZ%"=="" (
        echo   [ERROR] Cannot get 7z. Download from https://7-zip.org/ and put 7za.exe in deploy\
        pause & exit /b 1
    )
)
echo   Compressing with 7z -mx9...
"%SZ%" a -t7z -mx9 "%OUTPUT%" "%PACK_DIR%\*" >nul
if exist "%OUTPUT%" (for %%F in ("%OUTPUT%") do echo   Done: %%~zF bytes) else (echo   [ERROR] Failed & pause & exit /b 1)
echo ========================================
echo   Output: %OUTPUT%
echo ========================================
pause