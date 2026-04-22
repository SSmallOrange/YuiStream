@echo off
REM ============================================
REM  ffplay Pull Stream Verification
REM  Usage: test_pull.bat [rtsp|rtmp]
REM  Default: rtsp
REM ============================================

set FFPLAY=%~dp0..\vendor\ffmpeg\bin\ffplay.exe
set RTSP_URL=rtsp://127.0.0.1:8554/live
set RTMP_URL=rtmp://127.0.0.1:1935/live/stream

set MODE=%~1
if "%MODE%"=="" set MODE=rtsp

if /i "%MODE%"=="rtsp" (
    echo [Pull Test] Protocol: RTSP
    echo [Pull Test] URL: %RTSP_URL%
    echo.
    "%FFPLAY%" -fflags nobuffer -flags low_delay -analyzeduration 500000 -probesize 32768 -rtsp_transport tcp "%RTSP_URL%"
) else if /i "%MODE%"=="rtmp" (
    echo [Pull Test] Protocol: RTMP
    echo [Pull Test] URL: %RTMP_URL%
    echo.
    "%FFPLAY%" -fflags nobuffer -flags low_delay -analyzeduration 500000 -probesize 32768 "%RTMP_URL%"
) else (
    echo [Usage] test_pull.bat [rtsp^|rtmp]
    echo.
    echo   rtsp  - Pull from %RTSP_URL%
    echo   rtmp  - Pull from %RTMP_URL%
)

pause
