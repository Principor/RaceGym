@echo off
setlocal

REM Build the RaceGym simulation DLL using CMake + vcpkg

if "%VCPKG_ROOT%"=="" (
  set "VCPKG_ROOT=C:\vcpkg"
)

if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
  echo [ERROR] vcpkg not found. Set VCPKG_ROOT or install vcpkg in C:\vcpkg
  exit /b 1
)

set "PROJ_DIR=%~dp0"
set "BUILD_DIR=%PROJ_DIR%build"

if not exist "%BUILD_DIR%" (
  mkdir "%BUILD_DIR%"
)

pushd "%BUILD_DIR%"
cmake -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows "%PROJ_DIR%"
if errorlevel 1 (
  popd
  exit /b 1
)

cmake --build . --config Release
set "ERR=%ERRORLEVEL%"
popd
if not "%ERR%"=="0" exit /b %ERR%

echo.
echo Built DLL: "%BUILD_DIR%\Release\racegym_sim.dll"
endlocal
