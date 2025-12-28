@echo off
setlocal

:: Configuration
set QT_PATH=C:\Qt\6.10.1\msvc2022_64
set VERSION=0.1.0

echo.
echo ========================================
echo   Building Slate Browser v%VERSION%
echo ========================================
echo.

:: Clean and create build directory
if exist build rmdir /s /q build
mkdir build
cd build

:: Configure
echo [1/4] Configuring...
cmake -DCMAKE_PREFIX_PATH="%QT_PATH%" -DCMAKE_BUILD_TYPE=Release .. >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

:: Build
echo [2/4] Building...
cmake --build . --config Release >nul 2>&1
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

:: Deploy Qt dependencies
echo [3/4] Deploying Qt dependencies...
"%QT_PATH%\bin\windeployqt.exe" --release --no-translations Release\Slate.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: windeployqt failed
    exit /b 1
)

:: Create zip package
echo [4/4] Creating release package...
cd Release
powershell -Command "Compress-Archive -Path * -DestinationPath '..\..\Slate-v%VERSION%-windows.zip' -Force"
cd ..

echo.
echo ========================================
echo   Build complete!
echo   Package: Slate-v%VERSION%-windows.zip
echo ========================================
echo.

cd ..
