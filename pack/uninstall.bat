@echo off
echo Uninstalling test_runner_ui...
echo.
:: Remove our generated data
if exist "D:\.SDKtest\config.json" (
  echo [Remove] config.json
  del /q "D:\.SDKtest\config.json"
)
if exist "D:\.SDKtest\reports" (
  echo [Remove] reports
  rmdir /s /q "D:\.SDKtest\reports"
)
if exist "D:\.SDKtest\restore" (
  echo [Remove] restore
  rmdir /s /q "D:\.SDKtest\restore"
)
if exist "D:\.SDKtest\xml" (
  echo [Remove] xml
  rmdir /s /q "D:\.SDKtest\xml"
)
:: Restore original config
if exist "D:\.SDKtest\config.json.bak" (
  echo [Restore] config.json.bak
  move /y "D:\.SDKtest\config.json.bak" "D:\.SDKtest\config.json"
)
:: Clean empty dir
dir /b "D:\.SDKtest" 2>nul | findstr "^" >nul || rmdir /q "D:\.SDKtest"
echo Done!
pause