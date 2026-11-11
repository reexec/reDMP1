@echo off
setlocal

set CONFIG=Debug
if not "%~1"=="" set CONFIG=%~1

if /I "%CONFIG%"=="Release" (
    set PRESET=win64-release
) else (
    set PRESET=win64-debug
    set CONFIG=Debug
)

echo === Configuring %PRESET% ===
cmake --preset %PRESET%
if errorlevel 1 exit /b %errorlevel%

echo === Building %PRESET% (%CONFIG%) ===
cmake --build "build/%PRESET%" --config %CONFIG%
if errorlevel 1 exit /b %errorlevel%

echo === Testing %PRESET% (%CONFIG%) ===
ctest --test-dir "build/%PRESET%" -C %CONFIG% --output-on-failure
if errorlevel 1 exit /b %errorlevel%

exit /b 0
