@echo off
setlocal enabledelayedexpansion

set APP_NAME=DropAndDrag
set BUILD_DIR=build
set EXE=%BUILD_DIR%\Release\%APP_NAME%.exe

echo ==^> %APP_NAME% Windows build

:: ---- dependencies ----
echo ==^> Checking dependencies
where cmake >nul 2>&1 || (echo cmake not found. Install: winget install cmake && exit /b 1)
where ninja >nul 2>&1 || (echo ninja not found. Install: winget install Ninja-build.Ninja && exit /b 1)

:: ---- skia check ----
if "%SKIA_DIR%"=="" (
    if exist "%USERPROFILE%\skia" set SKIA_DIR=%USERPROFILE%\skia
)
if "%SKIA_DIR%"=="" (
    if exist "C:\skia" set SKIA_DIR=C:\skia
)

if "%SKIA_DIR%"=="" (
    echo SKIA_DIR not set and Skia not found.
    echo Build Skia first or set SKIA_DIR:
    echo   set SKIA_DIR=C:\path\to\skia
    echo See README.md for Skia build instructions.
    exit /b 1
)

echo   Skia: %SKIA_DIR%

:: ---- vcvars ----
if not defined VCINSTALLDIR (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) else (
        echo Visual Studio 2022 not found. Install from https://visualstudio.microsoft.com/
        exit /b 1
    )
)

:: ---- configure ----
echo ==^> Configuring
cmake -B "%BUILD_DIR%" -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DSKIA_DIR="%SKIA_DIR%" ^
    -DDROPANDDRAG_BUILD_TESTS=OFF ^
    -DCMAKE_C_COMPILER=clang-cl ^
    -DCMAKE_CXX_COMPILER=clang-cl

if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

:: ---- build ----
echo ==^> Building
cmake --build "%BUILD_DIR%"

if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo ==^> Build successful: %EXE%
echo.
echo To create installer:
echo   packaging\windows\build.bat
