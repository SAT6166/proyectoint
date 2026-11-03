# ============================================================================
#  ejecutar.ps1 - Compila y ejecuta el Sistema de Gestion Financiera (Raylib)
# ============================================================================
#  Uso (desde la carpeta del proyecto):
#     .\ejecutar.bat                 compila y ejecuta
#     .\ejecutar.bat -Vigilar        recompila y reinicia la app cada vez que guardas main.c
#     .\ejecutar.bat -SoloCompilar   solo compila
#
#  Que hace para que la compilacion sea estable:
#   1. Trabaja siempre en la carpeta del script (datos.txt queda en el lugar correcto).
#   2. Busca un gcc de 64 bits (WinLibs, w64devkit, MSYS2) e ignora el MinGW de 32 bits.
#   3. Si falta Raylib en .\raylib, lo descarga (version oficial 5.5).
#   4. Compila a un archivo temporal: si hay errores, el financiero.exe anterior se conserva.
#   5. Cierra la app si estaba abierta (si no, Windows no deja reemplazar el .exe).
#   6. Reintenta el reemplazo si OneDrive o el antivirus bloquean el archivo un momento.
# ============================================================================

param(
    [switch]$Vigilar,
    [switch]$SoloCompilar
)

$ErrorActionPreference = 'Continue'
Set-Location -LiteralPath $PSScriptRoot

$Fuente     = 'main.c'
$Exe        = 'financiero.exe'
$ExeTemp    = 'financiero_nuevo.exe'
$RutaExe    = Join-Path $PSScriptRoot $Exe
$UrlRaylib  = 'https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip'
$OpcionesGcc = @('-std=c99', '-Wall', '-O2', '-Iraylib\include', '-Lraylib\lib',
                 '-lraylib', '-lopengl32', '-lgdi32', '-lwinmm')

function Escribir-Paso($texto)  { Write-Host "==> $texto" -ForegroundColor Cyan }
function Escribir-Ok($texto)    { Write-Host "    $texto" -ForegroundColor Green }
function Escribir-Error($texto) { Write-Host "    $texto" -ForegroundColor Red }

# --- 1. Buscar un gcc de 64 bits -------------------------------------------
function Es-Gcc64($ruta) {
    try {
        $maquina = & $ruta -dumpmachine 2>$null
        return ($LASTEXITCODE -eq 0 -and "$maquina" -match 'x86_64')
    } catch { return $false }
}

function Buscar-Gcc {
    $candidatos = @()
    if ($env:GCC_RAYLIB) { $candidatos += $env:GCC_RAYLIB }   # permite forzar uno a mano

    $winget = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'
    $candidatos += Get-ChildItem -LiteralPath $winget -Directory -Filter 'BrechtSanders.WinLibs*' -ErrorAction SilentlyContinue |
                   ForEach-Object { Join-Path $_.FullName 'mingw64\bin\gcc.exe' }

    $candidatos += 'C:\raylib\w64devkit\bin\gcc.exe',
                   'C:\msys64\ucrt64\bin\gcc.exe',
                   'C:\msys64\mingw64\bin\gcc.exe'

    $candidatos += Get-Command gcc -All -ErrorAction SilentlyContinue | ForEach-Object { $_.Source }

    foreach ($c in $candidatos) {
        if ($c -and (Test-Path -LiteralPath $c) -and (Es-Gcc64 $c)) { return $c }
    }
    return $null
}

# --- 2. Asegurar que Raylib este en .\raylib -------------------------------
function Asegurar-Raylib {
    if ((Test-Path 'raylib\include\raylib.h') -and (Test-Path 'raylib\lib\libraylib.a')) { return $true }

    Escribir-Paso 'No se encontro Raylib en .\raylib. Descargando Raylib 5.5 oficial...'
    $temporal = Join-Path $env:TEMP ("raylib_descarga_" + [guid]::NewGuid().ToString('N'))
    try {
        New-Item -ItemType Directory -Force -Path $temporal | Out-Null
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $ProgressPreference = 'SilentlyContinue'
        $zip = Join-Path $temporal 'raylib.zip'
        Invoke-WebRequest -Uri $UrlRaylib -OutFile $zip -UseBasicParsing -ErrorAction Stop
        Expand-Archive -LiteralPath $zip -DestinationPath $temporal -Force -ErrorAction Stop
        $origen = Join-Path $temporal 'raylib-5.5_win64_mingw-w64'
        New-Item -ItemType Directory -Force -Path 'raylib' | Out-Null
        Copy-Item -Path (Join-Path $origen 'include'), (Join-Path $origen 'lib'), (Join-Path $origen 'LICENSE') `
                  -Destination 'raylib' -Recurse -Force -ErrorAction Stop
        Escribir-Ok 'Raylib instalado en .\raylib'
        return $true
    } catch {
        Escribir-Error "No se pudo descargar Raylib: $($_.Exception.Message)"
        return $false
    } finally {
        Remove-Item -LiteralPath $temporal -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# --- 3. Compilar a un .exe temporal ----------------------------------------
function Compilar {
    Remove-Item -LiteralPath $ExeTemp -Force -ErrorAction SilentlyContinue
    Escribir-Paso "Compilando $Fuente ..."
    $reloj = [Diagnostics.Stopwatch]::StartNew()
    # Out-Host: los mensajes de gcc se muestran en pantalla y no se mezclan con el valor devuelto
    & $script:Gcc $Fuente -o $ExeTemp @OpcionesGcc | Out-Host
    $codigo = $LASTEXITCODE
    $reloj.Stop()

    if ($codigo -ne 0 -or -not (Test-Path -LiteralPath $ExeTemp)) {
        Remove-Item -LiteralPath $ExeTemp -Force -ErrorAction SilentlyContinue
        Escribir-Error 'ERROR de compilacion: revisa los mensajes de arriba.'
        if (Test-Path -LiteralPath $Exe) { Escribir-Error "Se conserva el $Exe anterior (no se modifico)." }
        return $false
    }
    Escribir-Ok ("Compilado sin errores en {0:N1} s" -f $reloj.Elapsed.TotalSeconds)
    return $true
}

# --- 4. Cerrar la app si esta abierta ---------------------------------------
function Detener-App {
    Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($Exe)) -ErrorAction SilentlyContinue |
        Where-Object { try { $_.Path -eq $RutaExe } catch { $false } } |
        ForEach-Object {
            Escribir-Paso "Cerrando la instancia abierta de $Exe (PID $($_.Id))..."
            Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
            $_.WaitForExit(5000) | Out-Null
        }
}

# --- 5. Reemplazar el .exe (con reintentos) ---------------------------------
function Reemplazar-Exe {
    for ($i = 1; $i -le 10; $i++) {
        try {
            Move-Item -LiteralPath $ExeTemp -Destination $Exe -Force -ErrorAction Stop
            return $true
        } catch {
            Start-Sleep -Milliseconds 400
        }
    }
    Escribir-Error "No se pudo reemplazar $Exe (esta bloqueado por otro programa)."
    return $false
}

function Compilar-E-Instalar {
    if (-not (Compilar)) { return $false }
    Detener-App
    return (Reemplazar-Exe)
}

function Iniciar-App-Fondo {
    Escribir-Paso "Ejecutando $Exe ..."
    return Start-Process -FilePath $RutaExe -WorkingDirectory $PSScriptRoot -NoNewWindow -PassThru
}

# ============================== PROGRAMA ====================================
if (-not (Test-Path -LiteralPath $Fuente)) {
    Escribir-Error "No se encontro $Fuente en $PSScriptRoot"
    exit 1
}

$script:Gcc = Buscar-Gcc
if (-not $script:Gcc) {
    Escribir-Error 'No se encontro un gcc de 64 bits.'
    Escribir-Error 'Instalalo con:  winget install BrechtSanders.WinLibs.POSIX.UCRT'
    Escribir-Error '(El MinGW de C:\MinGW es de 32 bits y no sirve para Raylib.)'
    exit 1
}
Escribir-Ok "Compilador: $script:Gcc"

if (-not (Asegurar-Raylib)) { exit 1 }

if (-not (Compilar-E-Instalar)) { exit 1 }
if ($SoloCompilar) { exit 0 }

if (-not $Vigilar) {
    # Modo normal: la app corre en esta terminal y el script espera a que la cierres
    Escribir-Paso "Ejecutando $Exe ..."
    & $RutaExe
    exit $LASTEXITCODE
}

# Modo vigilar: recompila y reinicia cada vez que main.c cambia
$proceso = Iniciar-App-Fondo
$ultimoCambio = (Get-Item -LiteralPath $Fuente).LastWriteTimeUtc
Write-Host ''
Write-Host "Vigilando $Fuente. Guarda el archivo para recompilar. Ctrl+C para salir." -ForegroundColor Yellow

try {
    while ($true) {
        Start-Sleep -Milliseconds 700
        try { $cambio = (Get-Item -LiteralPath $Fuente -ErrorAction Stop).LastWriteTimeUtc } catch { continue }
        if ($cambio -eq $ultimoCambio) { continue }

        Start-Sleep -Milliseconds 400   # espera a que el editor termine de guardar
        try { $ultimoCambio = (Get-Item -LiteralPath $Fuente -ErrorAction Stop).LastWriteTimeUtc } catch { continue }

        Write-Host ''
        Escribir-Paso "Cambio detectado en $Fuente ($(Get-Date -Format HH:mm:ss))"
        # Si la compilacion falla, la app que ya estaba abierta sigue funcionando
        if (Compilar-E-Instalar) { $proceso = Iniciar-App-Fondo }
    }
} finally {
    Remove-Item -LiteralPath $ExeTemp -Force -ErrorAction SilentlyContinue
}
