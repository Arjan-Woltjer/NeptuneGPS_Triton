@echo off
title Ploegbesturing logboek
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\logboek.ps1"
echo.
pause
