@echo off
rem Lanza ejecutar.ps1 sin depender de la politica de ejecucion de scripts de PowerShell.
rem Uso:  ejecutar.bat  |  ejecutar.bat -Vigilar  |  ejecutar.bat -SoloCompilar
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ejecutar.ps1" %*
if errorlevel 1 (
    echo.
    echo Hubo un error. Lee los mensajes de arriba.
    pause
)
