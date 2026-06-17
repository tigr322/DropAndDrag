@echo off
setlocal enabledelayedexpansion

set APP_NAME=DropAndDrag
set VERSION=1.0.0
set BUILD_DIR=build
set SRC_DIR=%~dp0..\..

echo === Building %APP_NAME% v%VERSION% (Windows Release) ===

cmake -S "%SRC_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 17 2022"
if %ERRORLEVEL% neq 0 goto :error

cmake --build "%BUILD_DIR%" --config Release --target DropAndDrag
if %ERRORLEVEL% neq 0 goto :error

echo === Build complete ===

set WIX_CANDLE="%WIX%\bin\candle.exe"
set WIX_LIGHT="%WIX%\bin\light.exe"
set WIX_EXT="%WIX%\bin\WixUIExtension.dll"

if not defined WIX (
    echo WIX environment variable not set. Skipping MSI creation.
    goto :skip_msi
)

echo === Creating MSI installer ===

%WIX_CANDLE% ^
    -dBuildDir="%BUILD_DIR%" ^
    -dVersion="%VERSION%" ^
    -arch x64 ^
    -ext WixUIExtension ^
    -out "%BUILD_DIR%\%APP_NAME%.wixobj" ^
    "%~dp0installer.wxs"
if %ERRORLEVEL% neq 0 goto :error

%WIX_LIGHT% ^
    -ext WixUIExtension ^
    -cultures:en-us ^
    -out "%SRC_DIR%\%APP_NAME%-%VERSION%-win64.msi" ^
    "%BUILD_DIR%\%APP_NAME%.wixobj"
if %ERRORLEVEL% neq 0 goto :error

echo === MSI created: %APP_NAME%-%VERSION%-win64.msi ===

:skip_msi

if not defined SIGNTOOL (
    echo SIGNTOOL not set. Skipping code signing.
    goto :done
)

if defined CODE_SIGN_CERT (
    echo === Signing binaries ===
    %SIGNTOOL% sign /fd SHA256 /f "%CODE_SIGN_CERT%" /p "%CODE_SIGN_PASS%" ^
        /t http://timestamp.digicert.com "%BUILD_DIR%\Release\%APP_NAME%.exe"
)

:done
echo === Done ===
exit /b 0

:error
echo Build failed with error %ERRORLEVEL%
exit /b %ERRORLEVEL%
