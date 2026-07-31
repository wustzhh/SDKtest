@echo off
setlocal enabledelayedexpansion
echo Uninstalling test_runner_ui...
echo.
set "ANY=0"
if exist "D:\test_runner_ui" (
  echo [Remove] D:\test_runner_ui
  rmdir /s /q "D:\test_runner_ui"
  if exist "D:\test_runner_ui" (
    echo [ERROR] D:\test_runner_ui could not be removed, files may be in use.
  ) else set "ANY=1"
)
if exist "D:\.SDKtest\reports" (
  echo [Remove] reports
  rmdir /s /q "D:\.SDKtest\reports" && set "ANY=1"
)
if exist "D:\.SDKtest\restore" (
  echo [Remove] restore
  rmdir /s /q "D:\.SDKtest\restore" && set "ANY=1"
)
if exist "D:\.SDKtest\xml" (
  echo [Remove] xml
  rmdir /s /q "D:\.SDKtest\xml" && set "ANY=1"
)
if exist "D:\.SDKtest\config.json" (
  echo [Remove] config.json
  del /q "D:\.SDKtest\config.json" && set "ANY=1"
)
if exist "D:\.SDKtest\config.json.bak" (
  echo [Restore] config.json.bak
  move /y "D:\.SDKtest\config.json.bak" "D:\.SDKtest\config.json" && set "ANY=1"
)
if exist "D:\.SDKtest\.installed" del /q "D:\.SDKtest\.installed"
dir /b "D:\.SDKtest" 2>nul | findstr "^" >nul || rmdir /q "D:\.SDKtest"
if %ANY%==0 echo Nothing to uninstall.
echo Done!
pause