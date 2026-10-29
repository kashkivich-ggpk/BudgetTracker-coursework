@echo off
chcp 65001 > nul
title Настройка PostgreSQL для Budget Tracker
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup_postgresql.ps1"
if errorlevel 1 (
    echo.
    echo Настройка завершилась с ошибкой.
    pause
)
