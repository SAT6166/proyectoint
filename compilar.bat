@echo off
rem Solo compila (usa la misma logica estable que ejecutar.bat)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ejecutar.ps1" -SoloCompilar
