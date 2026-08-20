# 🛠️ Guía de Compilación e Instalación: LINX_MCP_LECTOR

> **Powered by [uam.lol/j](https://uam.lol/j)**

Esta guía explica paso a paso cómo compilar el binario nativo para Android ARM64, cómo ejecutarlo en tu celular con Root usando MT Manager, cómo iniciar el cliente PC y cómo conectar la Inteligencia Artificial vía MCP.

---

## 📋 Requisitos Previos

### En tu Computadora (PC):
1. **Android NDK** (r25c, r26d o r27d recomendados).
2. **Node.js** (v18 o superior).
3. **Android SDK Platform-Tools** (`adb` en el PATH).

### En tu Celular:
1. **Android con ROOT** (Magisk, KernelSU o APatch).
2. **MT Manager** (para permisos y ejecución directa).

---

## 📱 1. Compilación del Binario Android (`mem_server.sh`)

El servidor de Android se compila como un **único archivo binario autónomo**:

1. Abre una terminal en la carpeta `android_server/`.
2. Ejecuta el script de compilación:
   ```cmd
   build.bat
   ```
   *(El script detectará automáticamente tu NDK y compilará con `ndk-build -j4`)*.
3. El binario resultante se generará en:
   ```text
   android_server/libs/arm64-v8a/mem_server.sh
   ```

> **Nota Crítica sobre MT Manager**:
> Aunque tiene extensión `.sh`, este archivo **es un ejecutable binario ELF nativo compilado en C++**. MT Manager ejecuta binarios ELF directamente con Root si tienen extensión `.sh` y permisos `777`.

---

## 📲 2. Ejecución en el Celular (Paso a Paso)

1. Conecta tu teléfono por cable USB a la PC o usa MT Manager con FTP.
2. Copia el archivo **`mem_server.sh`** a la ruta `/data/local/tmp/` en tu teléfono.
3. Abre **MT Manager**, navega a `/data/local/tmp/` y mantén pulsado sobre `mem_server.sh`.
4. Selecciona **Propiedades** ➔ **Permisos** y marca todas las casillas (**`777`** o `rwxrwxrwx`).
5. Toca sobre `mem_server.sh` y presiona **Ejecutar** (concediendo permisos de Superusuario/Root).

El terminal mostrará:
```text
========================================================
   [+] LINX_MCP_LECTOR - ANDROID ROOT MEMORY DAEMON     
   [+] Vulkan/ImGui ESP + Socket @memsvc                
   [+] Pure /proc/mem | Zero Injection | SELinux-Safe   
   [+] Info & Community: uam.lol/j                      
========================================================
[+] ROOT confirmado (UID=0)
[+] SELinux: Permissive
[+] Arena Breakout Lite -> PID: 14250
[PC] adb forward tcp:8088 localabstract:memsvc
```

---

## 💻 3. Iniciar el Cliente PC y la Web UI

1. En tu computadora, ve a la carpeta `pc_client/`.
2. Si es la primera vez, instala las dependencias de Node.js:
   ```bash
   npm install
   ```
3. Ejecuta el lanzador automático:
   ```cmd
   start_client.bat
   ```
4. El script configurará automáticamente el túnel ADB stealth (`adb forward tcp:8088 localabstract:memsvc`) y abrirá el navegador en:
   👉 **`http://localhost:3000`**

---

## 🤖 4. Conectar con Antigravity IDE / Claude / Gemini (MCP)

Para permitir que una Inteligencia Artificial controle el lector de memoria, agregue esta configuración en su archivo `mcp_config.json`:

```json
{
  "mcpServers": {
    "linx-memory": {
      "command": "node",
      "args": [
        "C:\\Users\\Usuario\\Documents\\C\\AB\\LINX_MCP_LECTOR\\pc_client\\server\\mcp_server.js"
      ]
    }
  }
}
```

Una vez configurado, puedes pedirle a la IA:
* *"Verifica el estado del juego y la conexión"*
* *"Lee los jugadores en el mapa y sus coordenadas 3D"*
* *"Activa las cajas 2D y el radar en el overlay del móvil"*
* *"Ajusta el offset de cámara a 0x1100"*

---

> ¿Tienes dudas o problemas de compilación? Visita **[uam.lol/j](https://uam.lol/j)**
