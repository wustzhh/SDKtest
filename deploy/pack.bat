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
set "CFG=%ROOT%config.json"
set "OUT=%PACK_DIR%\config.json"
(
echo $j = Get-Content '%CFG%' -Raw -Encoding UTF8 ^| ConvertFrom-Json
echo $old = @('D:/pyProj/HUAWEISDK', 'D:\\pyProj\\HUAWEISDK'^)
echo $new = @('D:/test_runner_ui/sdk', 'D:\\test_runner_ui\\sdk'^)
echo foreach ($p in $j.profiles^) {
echo   for ($i=0; $i -lt 2; $i++^) {
echo     if ($p.test_binary^) { $p.test_binary = $p.test_binary.Replace($old[$i], $new[$i]^) }
echo     $nd = @(^); foreach ($d in $p.dependencies^) { $nd += $d.Replace($old[$i], $new[$i]^) }
echo     $p.dependencies = $nd
echo     if ($p.env_vars.MODEL_DIR^) { $p.env_vars.MODEL_DIR = $p.env_vars.MODEL_DIR.Replace($old[$i], $new[$i]^) }
echo   }
echo }
echo $j.config_path = 'D:/.SDKtest/config.json'
echo $j ^| ConvertTo-Json -Depth 10 ^| Set-Content '%OUT%' -Encoding UTF8
) > "%TEMP%\fix_conf.ps1"
powershell -NoProfile -ExecutionPolicy Bypass -File "%TEMP%\fix_conf.ps1" 2>nul
del "%TEMP%\fix_conf.ps1" 2>nul
if not exist "%OUT%" copy "%CFG%" "%OUT%" >nul
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

echo [5/5] Compress...
set "ZIP_NAME=%DEPLOY_DIR%test_runner_ui_portable"
if exist "%ZIP_NAME%.7z" del /f "%ZIP_NAME%.7z" 2>nul
if exist "%ZIP_NAME%.zip" del /f "%ZIP_NAME%.zip" 2>nul
:: 优先用7z（压缩率更高），其次zip
where 7z.exe >nul 2>nul
if %errorlevel% equ 0 (
    echo   Using 7z...
    7z a -mx9 "%ZIP_NAME%.7z" "%PACK_DIR%\*" >nul
    echo   Done: %ZIP_NAME%.7z
) else (
    echo   Using zip (install 7-Zip for smaller files^)...
    (echo Add-Type -A 'System.IO.Compression.FileSystem'
    echo [System.IO.Compression.ZipFile]::CreateFromDirectory('%PACK_DIR%','%ZIP_NAME%.zip','Optimal',$false^)
    ) > "%TEMP%\zip.ps1"
    powershell -NoProfile -ExecutionPolicy Bypass -File "%TEMP%\zip.ps1" 2>nul
    del "%TEMP%\zip.ps1" 2>nul
    echo   Done: %ZIP_NAME%.zip
)
echo.
echo ========================================
if exist "%ZIP_NAME%.7z" echo   Output: %ZIP_NAME%.7z
if exist "%ZIP_NAME%.zip" echo   Output: %ZIP_NAME%.zip
echo ========================================
pause