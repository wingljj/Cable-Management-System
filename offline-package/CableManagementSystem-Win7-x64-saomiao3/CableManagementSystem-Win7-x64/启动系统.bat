@echo off
setlocal
cd /d "%~dp0"
set "PATH=%~dp0;%PATH%"
start "" "%~dp0DapmQt.exe"
