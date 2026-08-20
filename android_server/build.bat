@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo   LINX_MCP_LECTOR - BUILD ALL-IN-ONE DAEMON (ARM64)
echo   Powered by: uam.lol/j
echo =======================================================

SET NDK=
IF DEFINED ANDROID_NDK_HOME (
    IF EXIST "%ANDROID_NDK_HOME%\ndk-build.cmd" SET "NDK=%ANDROID_NDK_HOME%"
)
IF "%NDK%"=="" IF DEFINED NDK_ROOT (
    IF EXIST "%NDK_ROOT%\ndk-build.cmd" SET "NDK=%NDK_ROOT%"
)
IF "%NDK%"=="" IF EXIST "C:\NDK\ndk-build.cmd" SET "NDK=C:\NDK"
IF "%NDK%"=="" IF EXIST "C:\Users\Usuario\Documents\C\WINNDK~1\android-ndk-r27d\ndk-build.cmd" SET "NDK=C:\Users\Usuario\Documents\C\WINNDK~1\android-ndk-r27d"
IF "%NDK%"=="" IF EXIST "%LOCALAPPDATA%\Android\Sdk\ndk-bundle\ndk-build.cmd" SET "NDK=%LOCALAPPDATA%\Android\Sdk\ndk-bundle"

IF "%NDK%"=="" (
    echo [!] NDK not found. Please set ANDROID_NDK_HOME or install Android NDK.
    exit /b 1
)

echo [*] Using Android NDK: %NDK%
SET SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

IF EXIST "obj" rmdir /s /q "obj"
IF EXIST "libs" rmdir /s /q "libs"

echo [*] Compiling single binary mem_server.sh with ndk-build...
call "%NDK%\ndk-build.cmd" NDK_PROJECT_PATH=. NDK_APPLICATION_MK=jni/Application.mk APP_BUILD_SCRIPT=jni/Android.mk -j4

IF ERRORLEVEL 1 (
    echo [!] Build Failed! Check compilation logs above.
    exit /b 1
)

IF EXIST "libs\arm64-v8a\mem_server.sh" (
    echo.
    echo =======================================================
    echo [+] EXITO! Archivo unico listo:
    echo     %SCRIPT_DIR%libs\arm64-v8a\mem_server.sh
    echo.
    echo Pasos para tu celular Android Root:
    echo 1. Copia 'mem_server.sh' a /data/local/tmp/
    echo 2. En MT Manager dale permisos 777 (rwxrwxrwx)
    echo 3. Toca sobre el archivo y dale a 'Ejecutar' (Root)
    echo Info & Soporte: uam.lol/j
    echo =======================================================
)

