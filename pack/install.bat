@echo off
echo Installing test_runner_ui...
echo.
:: Already installed?
if exist "D:\.SDKtest\.installed" (
  echo [SKIP] Already installed. Run uninstall.bat first.
  pause
  exit /b
)
:: Backup existing config (only if not already backed up)
if exist "D:\.SDKtest\config.json" (
  if not exist "D:\.SDKtest\config.json.bak" (
    echo [Backup] config.json
    move /y "D:\.SDKtest\config.json" "D:\.SDKtest\config.json.bak"
  ) else (
    echo [Backup] config.json.bak exists, keeping original backup
    del /q "D:\.SDKtest\config.json"
  )
)
echo [1/3] Copying app to D:\test_runner_ui\app...
robocopy "%~dp0app" "D:\test_runner_ui\app" /e /njh /njs /ndl /np
echo [2/3] Copying sdk to D:\test_runner_ui\sdk...
robocopy "%~dp0sdk" "D:\test_runner_ui\sdk" /e /njh /njs /ndl /np
echo [3/3] Copying config to D:\.SDKtest\...
if not exist "D:\.SDKtest" mkdir "D:\.SDKtest"
copy /y "%~dp0config.json" "D:\.SDKtest\config.json" >nul
echo installed > "D:\.SDKtest\.installed"
echo.
echo Starting...
start "" "D:\test_runner_ui\app\test_runner_ui.exe"
echo Done!
pause
