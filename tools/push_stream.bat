@echo off
REM ============================================
REM  FFmpeg RTSP Push Stream
REM  Usage: push_stream.bat [video_file]
REM  Target: rtsp://127.0.0.1:8554/live
REM ============================================

set FFMPEG=%~dp0..\vendor\ffmpeg\bin\ffmpeg.exe
set RTSP_URL=rtsp://127.0.0.1:8554/live

if "%~1"=="" (
    echo [Usage] push_stream.bat ^<video_file^>
    echo [Example] push_stream.bat D:\Videos\test.mp4
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

echo [RTSP Push] Source : %INPUT%
echo [RTSP Push] Target : %RTSP_URL%
echo [RTSP Push] Press Ctrl+C to stop
echo.

"%FFMPEG%" -re -stream_loop -1 -i "%INPUT%" -c:v libx264 -preset ultrafast -tune zerolatency -g 30 -c:a aac -ar 44100 -ac 2 -f rtsp -rtsp_transport tcp "%RTSP_URL%"
