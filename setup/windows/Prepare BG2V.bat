@echo off
title BG2V Setup
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0prepare-bg2v.ps1"
echo.
pause
