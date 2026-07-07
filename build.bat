@echo off
title test_runner_ui Build
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

set SCRIPT_DIR=%~dp0
set PATH=C:\Program Files\CMake\bin;C:\Qt\Tools\Ninja;%PATH%

echo ============================================
echo   test_runner_ui Build
echo ============================================
echo.

echo [1/3] Configure...
cmake -B "%SCRIPT_DIR%build" -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE="D:/vcpkg/scripts/buildsystems/vcpkg.cmake" ^
    -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/Ninja/ninja.exe" ^
    -DQt6_DIR="C:/Qt/6.11.1/msvc2022_64/lib/cmake/Qt6" ^
    -S "%SCRIPT_DIR%"
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] Config failed & pause & exit /b 1 )
echo [OK]

echo [2/3] Build...
cmake --build "%SCRIPT_DIR%build"
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] Build failed & pause & exit /b 1 )
echo [OK]

echo [3/3] Deploy to dist...
taskkill /f /im test_runner_ui.exe >nul 2>&1
copy /Y "%SCRIPT_DIR%build\test_runner_ui.exe" "%SCRIPT_DIR%dist\" >nul
copy "%SCRIPT_DIR%build\*.dll" "%SCRIPT_DIR%dist\" >nul 2>&1
copy /Y "C:\Qt\6.11.1\msvc2022_64\bin\Qt6Core.dll" "%SCRIPT_DIR%dist\" >nul
copy /Y "C:\Qt\6.11.1\msvc2022_64\bin\Qt6Gui.dll" "%SCRIPT_DIR%dist\" >nul
copy /Y "C:\Qt\6.11.1\msvc2022_64\bin\Qt6Widgets.dll" "%SCRIPT_DIR%dist\" >nul
copy /Y "C:\Qt\6.11.1\msvc2022_64\bin\Qt6OpenGL.dll" "%SCRIPT_DIR%dist\" >nul
copy /Y "C:\Qt\6.11.1\msvc2022_64\bin\Qt6OpenGLWidgets.dll" "%SCRIPT_DIR%dist\" >nul
if not exist "%SCRIPT_DIR%dist\platforms" mkdir "%SCRIPT_DIR%dist\platforms"
copy /Y "C:\Qt\6.11.1\msvc2022_64\plugins\platforms\qwindows.dll" "%SCRIPT_DIR%dist\platforms\" >nul 2>&1
copy "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Redist\MSVC\14.50.35710\x64\Microsoft.VC145.CRT\*.dll" "%SCRIPT_DIR%dist\" >nul 2>&1
copy /Y "%SCRIPT_DIR%src\template_report.html" "%SCRIPT_DIR%dist\" >nul
echo [OK]

echo.
echo [Pack] Creating test_runner_ui.zip...
if exist "%SCRIPT_DIR%test_runner_ui.zip" del "%SCRIPT_DIR%test_runner_ui.zip"
powershell -NoProfile -Command "Compress-Archive -Path '%SCRIPT_DIR%dist\*' -DestinationPath '%SCRIPT_DIR%test_runner_ui.zip' -Force" >nul 2>&1
if %ERRORLEVEL% EQU 0 ( echo [OK] Zipped ) else ( echo [WARN] Zip failed )
echo.

echo ============================================
echo   Done!
echo   exe: dist\test_runner_ui.exe
echo   zip: test_runner_ui.zip
echo ============================================
