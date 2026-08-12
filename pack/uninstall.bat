@echo off
echo Uninstalling test_runner_ui...
echo.
if not exist "D:\.SDKtest\.installed" (
  echo [SKIP] Not installed. Run install.bat first.
  pause
  exit /b
)
echo [Remove] D:\test_runner_ui
if exist "D:\test_runner_ui" rmdir /s /q "D:\test_runner_ui"
echo [Remove] reports
if exist "D:\.SDKtest\reports" rmdir /s /q "D:\.SDKtest\reports"
echo [Remove] restore
if exist "D:\.SDKtest\restore" rmdir /s /q "D:\.SDKtest\restore"
echo [Remove] xml
if exist "D:\.SDKtest\xml" rmdir /s /q "D:\.SDKtest\xml"
echo [Remove] config.json
if exist "D:\.SDKtest\config.json" del /q "D:\.SDKtest\config.json"
if exist "D:\.SDKtest\config.json.bak" (
  echo [Restore] config.json.bak
  move /y "D:\.SDKtest\config.json.bak" "D:\.SDKtest\config.json"
)
del /q "D:\.SDKtest\.installed" 2>nul
dir /b "D:\.SDKtest" 2>nul | findstr "^" >nul || rmdir /q "D:\.SDKtest"
echo Done!
pause
