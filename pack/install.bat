@echo off
echo === test_runner_ui Installer ===
echo.
xcopy /y /e /q "%~dp0app\*" "D:\test_runner_ui\app\" >nul
xcopy /y /e /q "%~dp0bin\*" "D:\test_runner_ui\bin\" >nul
xcopy /y /e /q "%~dp0models\*" "D:\test_runner_ui\models\" >nul
if not exist "D:\.SDKtest" mkdir "D:\.SDKtest"
copy /y "%~dp0config.json" "D:\.SDKtest\config.json" >nul
echo.
echo Install done
echo App: D:\test_runner_ui\app\test_runner_ui.exe
echo Config: D:\.SDKtest\config.json
echo.
start "" "D:\test_runner_ui\app\test_runner_ui.exe"
pause
