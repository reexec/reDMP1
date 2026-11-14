@echo off
setlocal

rem Usage: scripts\build_win64.bat [Debug|Release] [ffmpeg] [notest]
rem   ffmpeg  also builds the optional FFmpeg codec plugin (sound and video).
rem           Downloads the pinned LGPL shared build of FFmpeg on first use.
rem   notest  skips CTest.

cd /d "%~dp0.."

set CONFIG=Debug
set FFMPEG=0
set RUNTEST=1

:parse
if "%~1"=="" goto parsed
if /I "%~1"=="Release" set CONFIG=Release
if /I "%~1"=="Debug" set CONFIG=Debug
if /I "%~1"=="ffmpeg" set FFMPEG=1
if /I "%~1"=="notest" set RUNTEST=0
shift
goto parse
:parsed

if /I "%CONFIG%"=="Release" (
    set PRESET=win64-release
) else (
    set PRESET=win64-debug
)

if "%FFMPEG%"=="1" goto with_ffmpeg

set BUILD_DIR=build\%PRESET%
echo === Configuring %PRESET% ===
cmake --preset %PRESET%
if errorlevel 1 exit /b %errorlevel%
goto build

:with_ffmpeg
set BUILD_DIR=build\%PRESET%-ffmpeg
set FFMPEG_DIR=%CD%\third_party\repsp\third_party\_ffmpeg
echo === Fetching FFmpeg (LGPL shared build, skipped when already present) ===
python third_party\repsp\scripts\fetch_ffmpeg.py --dest "%FFMPEG_DIR%"
if errorlevel 1 exit /b %errorlevel%

echo === Configuring %PRESET% with the FFmpeg codec plugin ===
cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 -DREDMP1_CODEC_FFMPEG=ON -DFFMPEG_ROOT="%FFMPEG_DIR%"
if errorlevel 1 exit /b %errorlevel%

:build
echo === Building %BUILD_DIR% (%CONFIG%) ===
cmake --build "%BUILD_DIR%" --config %CONFIG%
if errorlevel 1 exit /b %errorlevel%

if "%RUNTEST%"=="0" goto done
echo === Testing %BUILD_DIR% (%CONFIG%) ===
ctest --test-dir "%BUILD_DIR%" -C %CONFIG% --output-on-failure
if errorlevel 1 exit /b %errorlevel%

:done
echo.
echo Built: %BUILD_DIR%\%CONFIG%\redmp1.exe
echo Run:   %BUILD_DIR%\%CONFIG%\redmp1.exe roms
exit /b 0
