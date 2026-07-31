@echo off
echo Uninstalling test_runner_ui...
echo.
:: Remove everything we created
if exist "D:\.SDKtest" (
  echo [Remove] D:\.SDKtest
  rmdir /s /q "D:\.SDKtest"
)
:: Restore original if any
if exist "D:\.SDKtest.bak" (
  echo [Restore] D:\.SDKtest.bak
  move "D:\.SDKtest.bak" "D:\.SDKtest"
)
echo Done!
pause