@echo off
REM ============================================
REM  FFmpeg RTMP Push Stream
REM  Usage: push_rtmp.bat [video_file]
REM  Target: rtmp://127.0.0.1:1935/live/stream
REM ============================================

set FFMPEG=%~dp0..\vendor\ffmpeg\bin\ffmpeg.exe
set RTMP_URL=rtmp://127.0.0.1:1935/live/stream

if "%~1"=="" (
    echo [Usage] push_rtmp.bat ^<video_file^>
    echo [Example] push_rtmp.bat D:\Videos\test.mp4
    echo.
    pause
    exit /b 1
)

set INPUT=%~1

if not exist "%INPUT%" (
    echo [Error] File not found: %INPUT%
    pause
    exit /b 1
)

echo [RTMP Push] Source : %INPUT%
echo [RTMP Push] Target : %RTMP_URL%
echo [RTMP Push] Press Ctrl+C to stop
echo.

"%FFMPEG%" -re -stream_loop -1 -i "%INPUT%" -c:v libx264 -preset ultrafast -tune zerolatency -g 30 -c:a aac -ar 44100 -ac 2 -f flv "%RTMP_URL%"
