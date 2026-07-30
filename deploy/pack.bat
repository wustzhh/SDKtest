@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   test_runner_ui - Pack
echo ========================================
echo.

set "ROOT=%~dp0..\"
set "DEPLOY_DIR=%~dp0"
set "PACK_DIR=%ROOT%pack"
set "DIST_DIR=%ROOT%dist"
set "SDK_DIR=D:\pyProj\HUAWEISDK"

if not exist "%ROOT%config.json" (
    echo [ERROR] config.json not found
    pause & exit /b 1
)

if exist "%PACK_DIR%" rmdir /s /q "%PACK_DIR%"
mkdir "%PACK_DIR%\app"

echo [1/5] Copy main app...
xcopy /y /q "%DIST_DIR%\test_runner_ui.exe" "%PACK_DIR%\app\" >nul
xcopy /y /q "%DIST_DIR%\*.dll" "%PACK_DIR%\app\" >nul 2>nul
echo   Done

echo [2/5] Copy SDK (%SDK_DIR%)...
if exist "%SDK_DIR%" (
    xcopy /e /q /y "%SDK_DIR%\*" "%PACK_DIR%\sdk\" >nul
    echo   Done
) else (
    echo   [WARN] SDK not found: %SDK_DIR%
    mkdir "%PACK_DIR%\sdk"
)

echo [3/5] Fix config paths...
powershell -NoProfile -Command "$c = Get-Content '%ROOT%config.json' -Raw -Encoding UTF8 | ConvertFrom-Json; foreach ($p in $c.profiles) { $p.test_binary = $p.test_binary -replace 'D:/pyProj/HUAWEISDK','D:/test_runner_ui/sdk'; $nd = @(); foreach ($d in $p.dependencies) { $nd += $d -replace 'D:/pyProj/HUAWEISDK','D:/test_runner_ui/sdk' }; $p.dependencies = $nd; if ($p.env_vars.MODEL_DIR) { $p.env_vars.MODEL_DIR = $p.env_vars.MODEL_DIR -replace 'D:/pyProj/HUAWEISDK','D:/test_runner_ui/sdk' -replace 'D:\\\\pyProj\\\\HUAWEISDK','D:\\\\test_runner_ui\\\\sdk' } }; $c.config_path = 'D:/.SDKtest/config.json'; $c | ConvertTo-Json -Depth 10 | Set-Content '%PACK_DIR%\config.json' -Encoding UTF8" 2>nul
if not exist "%PACK_DIR%\config.json" copy "%ROOT%config.json" "%PACK_DIR%\config.json" >nul
echo   Done

echo [4/5] Generate scripts...
(
echo @echo off
echo echo === test_runner_ui Installer ===
echo xcopy /y /e /q "%%~dp0app\*" "D:\test_runner_ui\app\" ^>nul
echo xcopy /y /e /q "%%~dp0sdk\*" "D:\test_runner_ui\sdk\" ^>nul
echo if not exist "D:\.SDKtest" mkdir "D:\.SDKtest"
echo copy /y "%%~dp0config.json" "D:\.SDKtest\config.json" ^>nul
echo echo Done!
echo start "" "D:\test_runner_ui\app\test_runner_ui.exe"
echo pause
) > "%PACK_DIR%\install.bat"

(
echo @echo off
echo echo === test_runner_ui Uninstaller ===
echo if exist "D:\test_runner_ui" rmdir /s /q "D:\test_runner_ui"
echo if exist "D:\.SDKtest\config.json" del /q "D:\.SDKtest\config.json"
echo dir /b "D:\.SDKtest" 2^>nul ^| findstr "^" ^>nul ^|^| rmdir /q "D:\.SDKtest"
echo echo Done!
echo pause
) > "%PACK_DIR%\uninstall.bat"
echo   Done

echo [5/5] Create zip (this may take a while for large SDK)...
set "ZIP_NAME=%DEPLOY_DIR%test_runner_ui_portable.zip"
if exist "%ZIP_NAME%" del "%ZIP_NAME%"
powershell -NoProfile -Command ^
  "Add-Type -A 'System.IO.Compression.FileSystem'; ^
   [System.IO.Compression.ZipFile]::CreateFromDirectory('%PACK_DIR%','%ZIP_NAME%','Optimal',$false); ^
   Write-Host '  Done'"
echo.
echo ========================================
echo   Output: %ZIP_NAME%
echo ========================================
pause