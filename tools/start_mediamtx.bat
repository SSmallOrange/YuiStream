@echo off
REM ============================================
REM  Start MediaMTX Streaming Server
REM  RTSP: rtsp://127.0.0.1:8554
REM  RTMP: rtmp://127.0.0.1:1935
REM  API:  http://127.0.0.1:9997
REM ============================================

set MEDIAMTX_DIR=%~dp0mediamtx
set MEDIAMTX_EXE=%MEDIAMTX_DIR%\mediamtx.exe
set MEDIAMTX_CFG=%MEDIAMTX_DIR%\mediamtx.yml

if not exist "%MEDIAMTX_EXE%" (
    echo [Error] mediamtx.exe not found: %MEDIAMTX_EXE%
    echo.
    echo Please download MediaMTX from:
    echo   https://github.com/bluenviron/mediamtx/releases
    echo.
    echo Download: mediamtx_vX.X.X_windows_amd64.zip
    echo Extract mediamtx.exe to: %MEDIAMTX_DIR%\
    echo.
    pause
    exit /b 1
)

echo [MediaMTX] Starting server...
echo [MediaMTX] RTSP: rtsp://127.0.0.1:8554
echo [MediaMTX] RTMP: rtmp://127.0.0.1:1935
echo [MediaMTX] API:  http://127.0.0.1:9997
echo.

"%MEDIAMTX_EXE%" "%MEDIAMTX_CFG%"
