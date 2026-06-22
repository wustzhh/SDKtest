@echo off
title test_runner_ui Build + Pack
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build
set DIST_DIR=%SCRIPT_DIR%dist

set QT_DIR=C:\Qt\6.11.1\mingw_64
set MINGW_DIR=C:\Qt\Tools\mingw1310_64
set NINJA_DIR=C:\Qt\Tools\Ninja
set PATH=%MINGW_DIR%\bin;%QT_DIR%\bin;%NINJA_DIR%;%PATH%

echo ============================================
echo   test_runner_ui Build + Pack
echo ============================================
echo.

g++ --version >nul 2>&1 || ( echo [ERROR] g++ not found & pause & exit /b 1 )
where cmake >nul 2>&1 || ( echo [ERROR] cmake not found & pause & exit /b 1 )
for /f "tokens=1-3" %%a in ('g++ -dumpversion') do set GXX_VER=%%a
echo [OK] MinGW g++ !GXX_VER!  +  cmake

echo.
echo [1/3] Configure...
cmake -B "%BUILD_DIR%" -G Ninja ^
    -DCMAKE_CXX_COMPILER="%MINGW_DIR%/bin/g++.exe" ^
    -DQt6_DIR="%QT_DIR%/lib/cmake/Qt6" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -S "%SCRIPT_DIR%"
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] Config failed & pause & exit /b 1 )
echo [OK]

echo.
echo [2/3] Build...
cmake --build "%BUILD_DIR%"
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] Build failed & pause & exit /b 1 )
echo [OK]

echo.
echo [3/3] Deploy + Pack...
"%QT_DIR%/bin/windeployqt.exe" "%BUILD_DIR%/test_runner_ui.exe" --no-opengl-sw --compiler-runtime >nul 2>&1
echo [OK] Deploy

if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
mkdir "%DIST_DIR%" "%DIST_DIR%\config" "%DIST_DIR%\platforms" "%DIST_DIR%\styles" "%DIST_DIR%\imageformats"

copy /Y "%BUILD_DIR%\test_runner_ui.exe" "%DIST_DIR%" >nul
copy /Y "%SCRIPT_DIR%config\test_config.json" "%DIST_DIR%\config\" >nul
for %%f in (Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll Qt6OpenGLWidgets.dll Qt6OpenGL.dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if exist "%BUILD_DIR%\%%f" copy /Y "%BUILD_DIR%\%%f" "%DIST_DIR%" >nul
)
copy /Y "%QT_DIR%\bin\Qt6OpenGLWidgets.dll" "%DIST_DIR%" >nul 2>&1
copy /Y "%QT_DIR%\bin\Qt6OpenGL.dll" "%DIST_DIR%" >nul 2>&1
copy /Y "%BUILD_DIR%\platforms\qwindows.dll" "%DIST_DIR%\platforms\" >nul
copy /Y "%BUILD_DIR%\styles\qmodernwindowsstyle.dll" "%DIST_DIR%\styles\" >nul
if exist "%BUILD_DIR%\imageformats\qjpeg.dll" copy /Y "%BUILD_DIR%\imageformats\qjpeg.dll" "%DIST_DIR%\imageformats\" >nul
if exist "%BUILD_DIR%\imageformats\qsvg.dll" copy /Y "%BUILD_DIR%\imageformats\qsvg.dll" "%DIST_DIR%\imageformats\" >nul
echo [OK] Packed

echo [OK] STEP reader is built-in (no external DLLs needed)

set ZIP_FILE=%~dp0test_runner_ui.zip
if exist "%ZIP_FILE%" del "%ZIP_FILE%"
powershell -NoProfile -Command "Compress-Archive -Path '%DIST_DIR%\\*' -DestinationPath '%ZIP_FILE%' -Force" >nul 2>&1
if %ERRORLEVEL% EQU 0 ( echo [OK] Zipped: test_runner_ui.zip ) else ( echo [WARN] Zip failed )

echo.
echo ============================================
echo   Done!
echo   Zip:   %ZIP_FILE%
echo   Folder: %DIST_DIR%
echo ============================================
echo.
echo  To use:
echo   1. Extract test_runner_ui.zip anywhere
echo   2. Edit config/test_config.json
echo   3. Run test_runner_ui.exe
echo.
pause
