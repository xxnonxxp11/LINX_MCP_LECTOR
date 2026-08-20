@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ========================================================
echo   LINX_MCP_LECTOR - PC CLIENT & MCP CONTROLLER
echo   (MODO STEALTH: Unix Abstract Socket @memsvc + ADB)
echo   Powered by: uam.lol/j
echo ========================================================
echo.

set "PATH=C:\Users\Usuario\AppData\Local\Android\Sdk\platform-tools;%PATH%"

:: 1. Comprobar si ya hay un dispositivo ADB conectado (USB o Wi-Fi)
set DETECTED_DEV=
for /f "skip=1 tokens=1,2" %%a in ('adb devices') do (
    if "%%b"=="device" (
        set "DETECTED_DEV=%%a"
    )
)

if defined DETECTED_DEV (
    echo [++] Dispositivo ADB detectado y activo: !DETECTED_DEV!
    echo [*] Conectando directamente en modo automatico...
    goto :setup_forward
)

:: 2. Si no hay dispositivo conectado, intentar reconectar con la ultima direccion guardada
set "LAST_ADDR_FILE=%~dp0last_adb_addr.txt"
set SAVED_ADDR=
if exist "%LAST_ADDR_FILE%" (
    set /p SAVED_ADDR=<"%LAST_ADDR_FILE%"
)

if defined SAVED_ADDR (
    echo [*] Intentando reconexion rapida con direccion guardada: !SAVED_ADDR! ...
    adb connect !SAVED_ADDR! >nul 2>&1
    for /f "skip=1 tokens=1,2" %%a in ('adb devices') do (
        if "%%b"=="device" (
            set "DETECTED_DEV=%%a"
        )
    )
    if defined DETECTED_DEV (
        echo [++] Reconexion automatica exitosa con !DETECTED_DEV!
        goto :setup_forward
    )
)

:: 3. Solo si no hay dispositivo activo, preguntar al usuario
echo [1] CONECTAR O VINCULAR VIA ADB (Cable USB o Wi-Fi)
echo --------------------------------------------------------
echo Opciones:
echo   - Cable USB: escribe 'usb' o presiona ENTER si ya esta conectado por cable.
echo   - Conectar Wi-Fi directo (si ya vinculaste antes): escribe IP:PUERTO
echo   - Vincular Wi-Fi con codigo de 6 digitos: escribe 'pair'
echo.

set DEFAULT_ADDR=192.168.7.8:20903
set /p TARGET_ADDR="> Elige opcion o escribe IP:PUERTO [%DEFAULT_ADDR%]: "

if "%TARGET_ADDR%"=="" (
    set TARGET_ADDR=%DEFAULT_ADDR%
)

if /i "%TARGET_ADDR%"=="pair" (
    echo.
    echo [*] VINCULAR DISPOSITIVO ANDROID (Depuracion Inalambrica)
    echo     En tu movil entra en: Opciones de desarrollador ^> Depuracion inalambrica ^> "Vincular con codigo de vinculacion"
    set /p PAIR_ADDR="> Introduce IP:PUERTO que sale en la ventana emergente de vinculacion: "
    set /p PAIR_CODE="> Introduce el codigo de 6 digitos: "
    echo.
    echo [*] Ejecutando: adb pair !PAIR_ADDR! !PAIR_CODE!
    adb pair !PAIR_ADDR! !PAIR_CODE!
    echo.
    echo [*] Ahora introduce la IP:PUERTO principal que sale en la pantalla de Depuracion Inalambrica:
    set /p TARGET_ADDR="> Conectar a IP:PUERTO: "
)

if /i not "%TARGET_ADDR%"=="usb" (
    echo.
    echo [*] Conectando ADB a %TARGET_ADDR% ...
    adb connect %TARGET_ADDR%
    echo %TARGET_ADDR%> "%LAST_ADDR_FILE%"
)

:setup_forward
echo.
echo [*] Configurando tunel stealth ADB (tcp:8088 -^> localabstract:memsvc)...
if defined DETECTED_DEV (
    adb -s !DETECTED_DEV! forward tcp:8088 localabstract:memsvc
) else (
    adb forward tcp:8088 localabstract:memsvc
)

if not exist "node_modules" (
    echo [*] Installing dependencies: express, ws, cors...
    call npm install
    if errorlevel 1 (
        echo [!] npm install failed! Check Node.js installation.
        pause
        exit /b 1
    )
)

echo.
echo [*] Checking port 3000 availability...
for /f "tokens=5" %%a in ('netstat -aon ^| findstr /c:":3000 "') do (
    if not "%%a"=="" if not "%%a"=="0" (
        echo [*] Terminating previous process on port 3000 ^(PID: %%a^)...
        taskkill /f /pid %%a >nul 2>&1
    )
)

echo [*] Launching Web UI at http://localhost:3000 ...
start "" http://localhost:3000

echo [*] Starting Node.js Server...
node server/index.js

pause
