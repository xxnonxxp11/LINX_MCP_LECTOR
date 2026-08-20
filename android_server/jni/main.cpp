// ============================================================
//  LINX_MCP_LECTOR - Android Root Memory Daemon & ESP Overlay
//  Powered by: uam.lol/j
//  Hilo principal : run_esp_loop() en src/main.cpp (Vulkan+ImGui)
//  Hilo 2        : TcpServer socket @memsvc (control por PC/USB)
// ============================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>

#include "tcp_server.h"
#include "mem_reader.h"
#include "logger.h"
#include "esp_bridge.h"

// run_esp_loop() está definida en src/main.cpp bajo #ifdef UNIFIED_BUILD
extern void run_esp_loop();

// ── Instancia global compartida entre hilos ──────────────────
ESPSharedState g_esp;

// ── Servidor socket en hilo 2 ────────────────────────────────
static TcpServer* g_server = nullptr;

static void* socket_server_thread(void*) {
    TcpServer server;
    g_server = &server;

    if (!server.start()) {
        LOG_ERROR("SYS", "Failed to start Unix abstract socket @memsvc");
        return nullptr;
    }

    printf("[+] Socket @memsvc activo (hilo 2)\n");
    server.run();
    return nullptr;
}

void sig_handler(int sig) {
    printf("\n[!] Signal %d recibido, deteniendo...\n", sig);
    if (g_server) g_server->stop();
    exit(0);
}

int main(int argc, char** argv) {
    // 0. AutoUpdater test check: exit 0 immediately when called with -h / --help
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("mem_server_unified v2.0 - OK\n");
            return 0;
        }
    }

    // Stealth: ocultar nombre del proceso
    prctl(PR_SET_NAME, "kworker/u8:2", 0, 0, 0);

    Logger::getInstance().init("/data/local/tmp/mem_server.log", 1000);
    LOG_INFO("SYS", "mem_server+ESP unificado arrancando...");

    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN);

    // Comprobar SELinux
    int selinux_enforce = -1;
    FILE* sf = fopen("/sys/fs/selinux/enforce", "r");
    if (sf) {
        char ch = 0;
        if (fread(&ch, 1, 1, sf) == 1) selinux_enforce = (ch == '1') ? 1 : 0;
        fclose(sf);
    }

    printf("========================================================\n");
    printf("   [+] LINX_MCP_LECTOR - ANDROID ROOT MEMORY DAEMON     \n");
    printf("   [+] Vulkan/ImGui ESP + Socket @memsvc                \n");
    printf("   [+] Pure /proc/mem | Zero Injection | SELinux-Safe   \n");
    printf("   [+] Info & Community: uam.lol/j                      \n");
    printf("========================================================\n");

    if (getuid() != 0) {
        printf("\033[1;31m[!] Sin root (UID=%d). Ejecutar con root en MT Manager.\033[0m\n", getuid());
    } else {
        printf("\033[1;32m[+] ROOT confirmado (UID=0)\033[0m\n");
    }

    if (selinux_enforce == 1) {
        printf("\033[1;32m[+] SELinux: Enforcing (Anti-Cheat Safe)\033[0m\n");
    } else if (selinux_enforce == 0) {
        printf("\033[1;33m[*] SELinux: Permissive\033[0m\n");
    }

    // Detectar juego
    int lite_pid = MemReader::find_pid("com.proximabeta.mf.liteuamo");
    int main_pid = MemReader::find_pid("com.proximabeta.mf.uamo");
    if (lite_pid > 0) {
        printf("\033[1;36m[+] Arena Breakout Lite -> PID: %d\033[0m\n", lite_pid);
    } else if (main_pid > 0) {
        printf("\033[1;36m[+] Arena Breakout Standard -> PID: %d\033[0m\n", main_pid);
    } else {
        printf("[*] Juego en espera. Puedes abrirlo en cualquier momento.\n");
    }

    printf("--------------------------------------------------------\n");
    printf("[*] Iniciando hilo socket @memsvc...\n");
    printf("[PC] adb forward tcp:8088 localabstract:memsvc\n");
    printf("--------------------------------------------------------\n");

    // Hilo 2: servidor socket
    pthread_t sock_thread;
    pthread_create(&sock_thread, nullptr, socket_server_thread, nullptr);
    pthread_detach(sock_thread);

    // Hilo principal: Vulkan/ImGui loop (vive en src/main.cpp)
    printf("[+] Iniciando Vulkan overlay (ESP)...\n");
    run_esp_loop();

    return 0;
}
