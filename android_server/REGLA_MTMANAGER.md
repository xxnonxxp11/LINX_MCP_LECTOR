# REGLA CRITICA DE ARQUITECTURA Y MT MANAGER
# ==============================================================================

1. MT MANAGER Y BINARIOS ELF:
   - MT Manager en Android ejecuta archivos binarios ELF directamente con permisos Root cuando tienen permisos 777.
   - Para que MT Manager lo ejecute con 1 solo toque y no pida "seleccionar tipo de archivo" o "abrir como script", el binario nativo compilado en C++ (ARM64) DEBE llamarse DIRECTAMENTE:
     `mem_server.sh`
   - `mem_server.sh` NO ES UN SCRIPT DE SHELL, ES EL BINARIO NATIVO C++ COMPILADO.

2. CERO SCRIPTS WRAPPERS O DE LANZAMIENTO:
   - NUNCA crear ni mencionar archivos como `run_server.sh`, `start.sh` ni scripts intermediarios.
   - Toda la lógica (detección de Wi-Fi, IP en rojo, permisos, SELinux permissive, detección de juegos y socket TCP 8088 multihilo) está compilada 100% DENTRO del propio binario `mem_server.sh` (main.cpp / ndk-build).

3. RUTA DEL BINARIO FINAL:
   - Ubicación en PC tras build: `android_server/libs/arm64-v8a/mem_server.sh`
   - Ubicación en el celular: `/data/local/tmp/mem_server.sh` (permisos 777).

4. EJECUCION EN MT MANAGER:
   - En MT Manager: Tocar `mem_server.sh` -> Ejecutar con Root -> Listo.
