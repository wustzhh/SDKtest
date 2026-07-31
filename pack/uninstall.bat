@echo off
echo Uninstalling test_runner_ui...
echo.
:: Only uninstall if we installed
if not exist "D:\.SDKtest\.installed" (
  echo [SKIP] Not installed by us, nothing to uninstall.
  pause
  exit /b
)
:: Remove our files
if exist "D:\test_runner_ui" (
  echo [Remove] D:\test_runner_ui
  rmdir /s /q "D:\test_runner_ui"
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
if exist "D:\.SDKtest\config.json" (
  echo [Remove] config.json
  del /q "D:\.SDKtest\config.json"
)
:: Restore original config if backed up
if exist "D:\.SDKtest\config.json.bak" (
  echo [Restore] config.json.bak
  move /y "D:\.SDKtest\config.json.bak" "D:\.SDKtest\config.json"
)
:: Clean installed marker
del /q "D:\.SDKtest\.installed" 2>nul
:: Clean empty dir
dir /b "D:\.SDKtest" 2>nul | findstr "^" >nul || rmdir /q "D:\.SDKtest"
echo Done!
pause