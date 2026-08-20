#pragma once
// ============================================================
//  esp_bridge.h — Puente thread-safe entre el servidor socket
//  (TcpServer) y el loop de renderizado Vulkan/ImGui (ESP).
//
//  El hilo del servidor escribe g_esp.actors cada vez que
//  procesa un comando "actors".  El hilo gráfico lo lee en
//  cada frame para dibujar cajas, esqueletos y distancias.
// ============================================================

#include <mutex>
#include <vector>
#include <string>
#include <atomic>
#include <cstdint>

// ── Actor que comparte ambos hilos ───────────────────────────
struct ESPActorShared {
    float    x, y, z;              // Posición 3D (root / cápsula)
    float    head_x, head_y, head_z; // Cabeza (si disponible)
    bool     is_bot    = false;
    bool     is_enemy  = true;
    int      team_id   = 0;
    float    health    = 100.0f;
    float    distance  = 0.0f;
    char     name[48]  = {};
    uint64_t ptr       = 0;
};

// ── Estado global compartido ─────────────────────────────────
struct ESPSharedState {
    std::mutex             mtx;
    std::vector<ESPActorShared> actors;

    // Cámara local (actualizada junto con los actores)
    float  cam_x     = 0, cam_y = 0, cam_z = 0;
    float  cam_pitch = 0, cam_yaw = 0, cam_fov = 90.f;
    bool   cam_valid = false;
    int    local_team = 0;

    // Toggles de dibujado (controlados por socket o menú ImGui)
    std::atomic<bool> draw_box      {true};
    std::atomic<bool> draw_skeleton {false};
    std::atomic<bool> draw_snapline {false};
    std::atomic<bool> draw_distance {true};
    std::atomic<bool> draw_health   {false};
    std::atomic<bool> draw_radar    {false};
    std::atomic<bool> ignore_bots   {true};
};

// Instancia global — definida en main.cpp, usada en todos los
// módulos que incluyan este header.
extern ESPSharedState g_esp;
