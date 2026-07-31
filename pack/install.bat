@echo off
set "MYDIR=%~dp0"
if "%MYDIR:~-1%"=="\" set "MYDIR=%MYDIR:~0,-1%"
echo Installing test_runner_ui...
echo   Source: %MYDIR%
echo.
if exist "D:\.SDKtest\config.json" (
  echo [Backup] config.json
  move /y "D:\.SDKtest\config.json" "D:\.SDKtest\config.json.bak"
)
if not exist "D:\.SDKtest" mkdir "D:\.SDKtest"
echo [Install] Fixing paths...
powershell -NoProfile -Command "$m='%MYDIR%';$mf=$m.Replace('\','/');$mb=$m.Replace('\','\\');$c=Get-Content '%MYDIR%\config.json' -Raw -Encoding UTF8;$c=$c -replace 'D:/test_runner_ui/sdk',($mf+'/sdk');$c=$c -replace 'D:\\\\test_runner_ui\\\\sdk',($mb+'\\sdk');$c | Set-Content 'D:\.SDKtest\config.json' -Encoding UTF8"
echo.
echo Starting...
start "" "%MYDIR%\app\test_runner_ui.exe"
echo Done!
pause