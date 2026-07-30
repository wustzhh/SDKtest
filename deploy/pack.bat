@echo off
setlocal enabledelayedexpansion
pushd "%~dp0.."
set "ROOT=%CD%\"
popd

set "DEPLOY_DIR=%~dp0"
set "PACK_DIR=%ROOT%\pack"
set "DIST_DIR=%ROOT%\dist"
set "SDK_DIR=D:\pyProj\HUAWEISDK"

echo ========================================
echo   test_runner_ui - Pack
echo ========================================
echo.

if not exist "%ROOT%config.json" (
    echo [ERROR] config.json not found at %ROOT%
    pause & exit /b 1
)

:: Clean and create dirs
if exist "%PACK_DIR%" rmdir /s /q "%PACK_DIR%"
mkdir "%PACK_DIR%\app" 2>nul

echo [1/4] Copy main app...
robocopy "%DIST_DIR%" "%PACK_DIR%\app" test_runner_ui.exe *.dll /njh /njs /ndl /np >nul 2>nul
echo   Done

echo [2/4] Copy SDK (robocopy, this may take a while)...
if exist "%SDK_DIR%" (
    robocopy "%SDK_DIR%" "%PACK_DIR%\sdk" /e /njh /njs /ndl /np >nul 2>nul
    echo   Done
) else (
    echo   [WARN] SDK not found: %SDK_DIR%
    mkdir "%PACK_DIR%\sdk" 2>nul
)

echo [3/4] Fix config + generate scripts...
set "CFG=%ROOT%config.json"
set "OUT=%PACK_DIR%\config.json"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
"$j=Get-Content '%CFG%'-Raw-Encoding UTF8|ConvertFrom-Json;^
$o=@('D:/pyProj/HUAWEISDK','D:\\pyProj\\HUAWEISDK');^
$n=@('D:/test_runner_ui/sdk','D:\\test_runner_ui\\sdk');^
foreach($p in $j.profiles){for($i=0;$i-lt2;$i++){^
if($p.test_binary){$p.test_binary=$p.test_binary.Replace($o[$i],$n[$i])};^
$nd=@();foreach($d in $p.dependencies){$nd+=$d.Replace($o[$i],$n[$i])};$p.dependencies=$nd;^
if($p.env_vars.MODEL_DIR){$p.env_vars.MODEL_DIR=$p.env_vars.MODEL_DIR.Replace($o[$i],$n[$i])}^
}};$j.config_path='D:/.SDKtest/config.json';^
$j|ConvertTo-Json-Depth 10|Set-Content '%OUT%'-Encoding UTF8" 2>nul
if not exist "%OUT%" copy "%CFG%" "%OUT%" >nul

:: install.bat
(echo @echo off
 echo echo === test_runner_ui Installer ===
 echo robocopy "%%~dp0app" "D:\test_runner_ui\app" /e /njh /njs /ndl /np ^>nul
 echo robocopy "%%~dp0sdk" "D:\test_runner_ui\sdk" /e /njh /njs /ndl /np ^>nul
 echo if not exist "D:\.SDKtest" mkdir "D:\.SDKtest"
 echo copy /y "%%~dp0config.json" "D:\.SDKtest\config.json" ^>nul
 echo start "" "D:\test_runner_ui\app\test_runner_ui.exe"
) > "%PACK_DIR%\install.bat"

:: uninstall.bat
(echo @echo off
 echo echo === test_runner_ui Uninstaller ===
 echo if exist "D:\test_runner_ui" rmdir /s /q "D:\test_runner_ui"
 echo if exist "D:\.SDKtest\config.json" del /q "D:\.SDKtest\config.json"
 echo dir /b "D:\.SDKtest" 2^>nul ^| findstr "^" ^>nul ^|^| rmdir /q "D:\.SDKtest"
) > "%PACK_DIR%\uninstall.bat"
echo   Done

echo [4/4] Compress (tar.exe or 7z)...
set "OUTPUT=%DEPLOY_DIR%test_runner_ui_portable"
if exist "%OUTPUT%.zip" del /f "%OUTPUT%.zip" 2>nul
if exist "%OUTPUT%.7z" del /f "%OUTPUT%.7z" 2>nul

:: Try 7z first (fast + small), then tar.exe, then fallback
where 7z.exe >nul 2>nul && (
    echo   Using 7z...
    7z a -tzip -mx5 "%OUTPUT%.zip" "%PACK_DIR%\*" >nul
    echo   Done: %OUTPUT%.zip
) || (
    where tar.exe >nul 2>nul && (
        echo   Using tar.exe...
        tar -a -cf "%OUTPUT%.zip" -C "%PACK_DIR%" . >nul 2>nul
        echo   Done: %OUTPUT%.zip
    ) || (
        echo   Using PowerShell...
        powershell -NoProfile -Command "Add-Type -A 'System.IO.Compression.FileSystem';[System.IO.Compression.ZipFile]::CreateFromDirectory('%PACK_DIR%','%OUTPUT%.zip','Fastest',$false)" 2>nul
        echo   Done: %OUTPUT%.zip
    )
)

:: Verify
if exist "%OUTPUT%.zip" (
    for %%F in ("%OUTPUT%.zip") do echo   Size: %%~zF bytes
) else (
    echo   [ERROR] Zip not created!
    pause & exit /b 1
)

echo.
echo ========================================
echo   Output: %OUTPUT%.zip
echo ========================================
pause