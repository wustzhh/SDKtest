@echo off
set "MYDIR=%~dp0"
if "%MYDIR:~-1%"=="\" set "MYDIR=%MYDIR:~0,-1%"
echo Installing test_runner_ui...
echo   Source: %MYDIR%
echo.
:: Backup entire .SDKtest if exists
if exist "D:\.SDKtest" (
  echo [Backup] D:\.SDKtest -^> D:\.SDKtest.bak
  if exist "D:\.SDKtest.bak" (
    echo [WARN] D:\.SDKtest.bak already exists, skipping backup
  ) else (
    move "D:\.SDKtest" "D:\.SDKtest.bak"
  )
)
mkdir "D:\.SDKtest"
echo [Install] Fixing paths...
powershell -NoProfile -Command "$m='%MYDIR%';$mf=$m.Replace('\','/');$mb=$m.Replace('\','\\');$c=Get-Content '%MYDIR%\config.json' -Raw -Encoding UTF8;$c=$c -replace 'D:/test_runner_ui/sdk',($mf+'/sdk');$c=$c -replace 'D:\\\\test_runner_ui\\\\sdk',($mb+'\\sdk');$c | Set-Content 'D:\.SDKtest\config.json' -Encoding UTF8"
echo.
echo Starting...
start "" "%MYDIR%\app\test_runner_ui.exe"
echo Done!
pause