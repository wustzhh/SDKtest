@echo off
echo Installing test_runner_ui...
echo.
:: Backup existing config if any
if exist "D:\.SDKtest\config.json" (
  echo [Backup] config.json
  move /y "D:\.SDKtest\config.json" "D:\.SDKtest\config.json.bak"
)
echo [1/2] Copying files...
robocopy "%~dp0app" "D:\test_runner_ui\app" /e /njh /njs /ndl /np
robocopy "%~dp0sdk" "D:\test_runner_ui\sdk" /e /njh /njs /ndl /np
if not exist "D:\.SDKtest" mkdir "D:\.SDKtest"
copy /y "%~dp0config.json" "D:\.SDKtest\config.json" >nul
echo.
echo [2/2] Starting...
start "" "D:\test_runner_ui\app\test_runner_ui.exe"
echo Done!
pause