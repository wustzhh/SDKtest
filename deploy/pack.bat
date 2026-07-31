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
if not exist "%~dp0config.json" (echo [ERROR] config.json not found & pause & exit /b 1)

:: Clean
if exist "%PACK_DIR%" rmdir /s /q "%PACK_DIR%"

:: Step 1: App
echo [1/4] Copy app...
mkdir "%PACK_DIR%\app" 2>nul
copy /y "%DIST_DIR%\test_runner_ui.exe" "%PACK_DIR%\app\" >nul
copy /y "%DIST_DIR%\*.dll" "%PACK_DIR%\app\" >nul 2>nul
if exist "%DIST_DIR%\platforms" robocopy "%DIST_DIR%\platforms" "%PACK_DIR%\app\platforms" /e /njh /njs /ndl /np >nul
if exist "%DIST_DIR%\template_report.html" copy /y "%DIST_DIR%\template_report.html" "%PACK_DIR%\app\" >nul
if exist "%DIST_DIR%\concrt140.dll" copy /y "%DIST_DIR%\concrt140*.dll" "%DIST_DIR%\msvcp140*.dll" "%DIST_DIR%\vcruntime140*.dll" "%DIST_DIR%\vccorlib140*.dll" "%PACK_DIR%\app\" >nul 2>nul
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
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0fix_conf.ps1" -ConfigPath "%~dp0config.json" -PackDir "%PACK_DIR%" 2>nul
if not exist "%PACK_DIR%\config.json" copy "%~dp0config.json" "%PACK_DIR%\config.json" >nul
(
echo @echo off
echo setlocal enabledelayedexpansion
echo set "MYDIR=%%~dp0"
echo if "%%MYDIR:~-1%%"=="\" set "MYDIR=%%MYDIR:~0,-1%%"
echo echo Installing test_runner_ui...
echo echo   Source: %%MYDIR%%
echo echo.
echo :: Backup existing config if any
echo if exist "D:\.SDKtest\config.json" ^(
echo   echo [Backup] D:\.SDKtest\config.json -^> config.json.bak
echo   move /y "D:\.SDKtest\config.json" "D:\.SDKtest\config.json.bak"
echo ^)
echo if not exist "D:\.SDKtest" mkdir "D:\.SDKtest"
echo :: Read template and replace paths
echo powershell -NoProfile -Command "$m='%%MYDIR%%';$mf=$m.Replace('\','/');$mb=$m.Replace('\','\\');$c=Get-Content '%%MYDIR%%\config.json' -Raw -Encoding UTF8;$c=$c -replace 'D:/test_runner_ui/sdk',($mf+'/sdk');$c=$c -replace 'D:\\\\test_runner_ui\\\\sdk',($mb+'\\\\sdk');$c | Set-Content 'D:\.SDKtest\config.json' -Encoding UTF8"
echo echo.
echo echo Starting...
echo start "" "%%MYDIR%%\app\test_runner_ui.exe"
echo echo Done^^!
echo pause
) > "%PACK_DIR%\install.bat"
(
echo @echo off
echo echo Uninstalling test_runner_ui...
echo echo.
echo :: Remove our config
echo if exist "D:\.SDKtest\config.json" ^(
echo   echo [Remove] D:\.SDKtest\config.json
echo   del /q "D:\.SDKtest\config.json"
echo ^)
echo :: Restore original config
echo if exist "D:\.SDKtest\config.json.bak" ^(
echo   echo [Restore] config.json.bak
echo   move /y "D:\.SDKtest\config.json.bak" "D:\.SDKtest\config.json"
echo ^)
echo :: Clean empty dir
echo dir /b "D:\.SDKtest" 2^>nul ^| findstr "^" ^>nul ^|^| rmdir /q "D:\.SDKtest"
echo echo Done^^!
echo pause
) > "%PACK_DIR%\uninstall.bat"
echo   Done

:: Step 4: Compress
echo [4/4] Compressing...
set "OUTPUT=%DEPLOY_DIR%test_runner_ui_portable.7z"
if exist "%OUTPUT%" del /f "%OUTPUT%" 2>nul

:: Use bundled 7za.exe (always present in deploy/)
set "SZ=%~dp07za.exe"
echo   Compressing with 7z -mx9...
"%SZ%" a -t7z -mx9 "%OUTPUT%" "%PACK_DIR%\*" >nul
if exist "%OUTPUT%" (for %%F in ("%OUTPUT%") do echo   Done: %%~zF bytes) else (echo   [ERROR] Failed & pause & exit /b 1)
echo ========================================
echo   Output: %OUTPUT%
echo ========================================
pause