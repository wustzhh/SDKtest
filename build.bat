@echo off
title test_runner_ui Build

set "VSWHERE=%ProgramFiles%/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvarsall.bat"
if not exist "%VSWHERE%" (
    echo [ERROR] vcvarsall.bat not found at "%VSWHERE%"
    pause & exit /b 1
)
call "%VSWHERE%" x64 >nul 2>&1
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] vcvarsall.bat failed & pause & exit /b 1 )

set "SCRIPT_DIR=%~dp0"
set "PATH=C:\Program Files\CMake\bin;C:\Qt\Tools\Ninja;%PATH%"

echo ============================================
echo   test_runner_ui Build
echo ============================================
echo.

echo [1/4] Configure...
cmake -B "%SCRIPT_DIR%build" -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE="D:/vcpkg/scripts/buildsystems/vcpkg.cmake" ^
    -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/Ninja/ninja.exe" ^
    -DQt6_DIR="C:/Qt/6.11.1/msvc2022_64/lib/cmake/Qt6" ^
    -S "%SCRIPT_DIR%"
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] Config failed & pause & exit /b 1 )
echo [OK]

echo [2/4] Build...
cmake --build "%SCRIPT_DIR%build"
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] Build failed & pause & exit /b 1 )
echo [OK]

echo [3/4] Deploy to dist...
taskkill /f /im test_runner_ui.exe >nul 2>&1
if not exist "%SCRIPT_DIR%dist" mkdir "%SCRIPT_DIR%dist"

copy /Y "%SCRIPT_DIR%build\test_runner_ui.exe" "%SCRIPT_DIR%dist\" >nul 2>&1
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] exe not found & pause & exit /b 1 )
copy "%SCRIPT_DIR%build\*.dll" "%SCRIPT_DIR%dist\" >nul 2>&1
copy /Y "%SCRIPT_DIR%src\template_report.html" "%SCRIPT_DIR%dist\" >nul 2>&1

if exist "%SCRIPT_DIR%build\config\test_config.json" (
    xcopy /E /I /Y "%SCRIPT_DIR%build\config\*" "%SCRIPT_DIR%dist\config\" >nul 2>&1
) else if exist "%SCRIPT_DIR%config\test_config.json" (
    xcopy /E /I /Y "%SCRIPT_DIR%config\*" "%SCRIPT_DIR%dist\config\" >nul 2>&1
)

:: Qt: 只复制必需的5个DLL + platforms插件
copy /Y "C:\Qt\6.11.1\msvc2022_64\bin\Qt6Core.dll" "%SCRIPT_DIR%dist\" >nul 2>&1
copy /Y "C:\Qt\6.11.1\msvc2022_64\bin\Qt6Gui.dll" "%SCRIPT_DIR%dist\" >nul 2>&1
copy /Y "C:\Qt\6.11.1\msvc2022_64\bin\Qt6Widgets.dll" "%SCRIPT_DIR%dist\" >nul 2>&1
copy /Y "C:\Qt\6.11.1\msvc2022_64\bin\Qt6OpenGL.dll" "%SCRIPT_DIR%dist\" >nul 2>&1
copy /Y "C:\Qt\6.11.1\msvc2022_64\bin\Qt6OpenGLWidgets.dll" "%SCRIPT_DIR%dist\" >nul 2>&1
if not exist "%SCRIPT_DIR%dist\platforms" mkdir "%SCRIPT_DIR%dist\platforms"
copy /Y "C:\Qt\6.11.1\msvc2022_64\plugins\platforms\qwindows.dll" "%SCRIPT_DIR%dist\platforms\" >nul 2>&1

:: 复制VC运行时
if exist "%VCToolsRedistDir%\x64\Microsoft.VC143.CRT\*.dll" (
    copy "%VCToolsRedistDir%\x64\Microsoft.VC143.CRT\*.dll" "%SCRIPT_DIR%dist\" >nul 2>&1
)
echo [OK]

echo.
echo [4/4] Pack to test_runner_ui.zip...
if exist "%SCRIPT_DIR%test_runner_ui.zip" del "%SCRIPT_DIR%test_runner_ui.zip"
powershell -NoProfile -Command "Compress-Archive -Path '%SCRIPT_DIR%dist\*' -DestinationPath '%SCRIPT_DIR%test_runner_ui.zip' -Force"
if %ERRORLEVEL% EQU 0 ( echo [OK] Zipped ) else ( echo [WARN] Zip failed )
echo.

echo ============================================
echo   Done!
echo   exe: dist\test_runner_ui.exe
echo   zip: test_runner_ui.zip
echo ============================================
pause
