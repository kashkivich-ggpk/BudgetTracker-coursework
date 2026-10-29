@echo off
chcp 65001 > nul
title Supabase setup for Financial Assistant
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup_supabase.ps1"
if errorlevel 1 pause
