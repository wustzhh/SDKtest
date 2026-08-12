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
echo [1/1] Copying config to D:\.SDKtest\...
if not exist "D:\.SDKtest" mkdir "D:\.SDKtest"
copy /y "%~dp0config.json" "D:\.SDKtest\config.json" >nul
echo installed > "D:\.SDKtest\.installed"
echo.
echo [2/2] Starting...
start "" "D:\test_runner_ui\app\test_runner_ui.exe"
echo Done!
pause
