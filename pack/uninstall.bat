@echo off
echo === test_runner_ui Uninstaller ===
echo.
if exist "D:\test_runner_ui" rmdir /s /q "D:\test_runner_ui" && echo   Removed D:\test_runner_ui
if exist "D:\.SDKtest\config.json" del /q "D:\.SDKtest\config.json" >nul && echo   Removed config file
dir /b "D:\.SDKtest" 2>nul | findstr "^" >nul || (rmdir /q "D:\.SDKtest" && echo   Removed empty D:\.SDKtest)
echo.
echo Uninstall done
pause
