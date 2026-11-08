@echo off
title Ploegbesturing firmware-update
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\flash.ps1"
echo.
pause
