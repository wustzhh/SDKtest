@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
set "PATH=C:\Program Files\CMake\bin;C:\Qt\Tools\Ninja;%PATH%"
cd /d D:\pyProj\SDKtest
"C:\Program Files\CMake\bin\cmake.exe" -B build -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE="D:/vcpkg/scripts/buildsystems/vcpkg.cmake" ^
    -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/Ninja/ninja.exe" ^
    -DQt6_DIR="C:/Qt/6.11.1/msvc2022_64/lib/cmake/Qt6" ^
    -S .
if errorlevel 1 ( echo [CFG-FAIL] & exit /b 1 )
"C:\Program Files\CMake\bin\cmake.exe" --build build --target shared_topo_detect
if errorlevel 1 ( echo [BUILD-FAIL] & exit /b 1 )
echo [BUILD-OK]
