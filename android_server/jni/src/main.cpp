// Weiyan Network Verification //
// If AIDE compiles jni, please delete the original main.cpp and change this injected file to main.cpp
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <fstream>
#include <string.h>
#include "oxorany.h"
#include "oxorany.h"
#include <time.h>
#include <malloc.h>
#include <iostream>
#include <fstream>
#include<iostream>
#include<ctime>
using namespace std;

using namespace std;
#include "Android_draw/draw.h"
#include "VulkanUtils.h"
#include "Main/ESP.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "res/weiyan.h"
#include "res/cJSON.h"
#include "res/Encrypt.h"
char buffer[512];
char appgg[80];

int FrameRate = 120;
int FPS;
bool main_thread_flag = true;
int ScreenX = 0;
int ScreenY = 0;
const char* path = "/data/user/0/com.tencent.mf.uam/files/ano_tmp/custom_cache/";
const char* path1 = "/data/user/0/com.tencent.tmgp.dfm/files/ano_tmp/custom_cache/";
std::vector<std::string> GetFilesInDirectory(const char* path) {
    std::vector<std::string> files;
    DIR* dir = opendir(path);
    if (dir == nullptr) {
        std::cerr << "Error opening directory: " << path << std::endl;
        return files;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) {  // Only process regular files
            files.push_back(entry->d_name);
        }
    }
    closedir(dir);

    // Sort file names alphabetically
    std::sort(files.begin(), files.end());

    return files;
}

#include <sys/prctl.h>

int Selection = 0;

#ifndef UNIFIED_BUILD
int main(int argc, char *argv[]) {
    // 1. Stealth: mask process name in /proc (anti-detection)
    prctl(PR_SET_NAME, "kworker/u8:0", 0, 0, 0);

    // 2. Optional background execution via CLI argument (-bg, --background, or 1)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-bg") == 0 || strcmp(argv[i], "--background") == 0 || strcmp(argv[i], "1") == 0) {
            pid_t bg_pid = fork();
            if (bg_pid < 0) {
                std::cerr << "Fork failed" << std::endl;
                return 1;
            }
            if (bg_pid > 0) {
                // Parent process exits cleanly
                return 0;
            }
            setsid(); // Child runs detached in background
            break;
        }
    }




    screen_config();
    ::ScreenX = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    ::ScreenY = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);
    ::native_window_screen_x = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    ::native_window_screen_y = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);
    if (!initGUI_draw(native_window_screen_x, native_window_screen_y, true)) {
        return -1;
    }
    
    Touch_Init(displayInfo.width, displayInfo.height, displayInfo.orientation, true);
  /*  int GameSelection;
    printf("ArenaBreakout Delta ArenaBreakoutIntl[1 2 3]\nPlease select the game version in the game");
    cin >> GameSelection;
    if (GameSelection == 1){
    while (true) {        
        drawBegin();
        ArenaBreakoutUI();
        ArenaBreakoutDraw(ImGui::GetBackgroundDrawList());
        PermissionModification();
        drawEnd();
    }
   } else if (GameSelection == 2){*/
   NumIoLoad(oxorany("R2HTEAM"));
    
while (true) {        
        drawBegin();
        ArenaBreakoutUI();
        ArenaBreakoutDraw(ImGui::GetBackgroundDrawList());
        drawEnd();
    }
 /* } else if (GameSelection == 3){
   while (true) {        
        drawBegin();
        ArenaBreakoutUI();
        ArenaBreakoutIntlDraw(ImGui::GetBackgroundDrawList());
        drawEnd();
    }
  }*/
    shutdown();
    Touch_Close();
    return 0;
}
#endif // UNIFIED_BUILD

// ── Función llamada por el main() unificado (jni/main.cpp) ──
#ifdef UNIFIED_BUILD
void run_esp_loop() {
    screen_config();
    ::ScreenX = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    ::ScreenY = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);
    ::native_window_screen_x = ::ScreenX;
    ::native_window_screen_y = ::ScreenY;

    if (!initGUI_draw(::native_window_screen_x, ::native_window_screen_y, true)) {
        printf("[-] Vulkan: fallo al inicializar. Saliendo.\n");
        return;
    }

    Touch_Init(displayInfo.width, displayInfo.height, displayInfo.orientation, true);
    NumIoLoad("R2HTEAM");

    printf("[+] Loop ESP activo.\n");
    while (true) {
        drawBegin();
        ArenaBreakoutUI();
        ArenaBreakoutDraw(ImGui::GetBackgroundDrawList());
        drawEnd();
    }

    shutdown();
    Touch_Close();
}
#endif // UNIFIED_BUILD

static bool show_menu = true;

void ArenaBreakoutUI() {
    if (!show_menu) {
        ImGui::SetNextWindowPos(ImVec2(30, 30), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(140, 50), ImGuiCond_Always);
        ImGui::Begin("##OpenMenuPill", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);
        if (ImGui::Button("☰ ABRIR MENÚ", ImVec2(130, 40))) {
            show_menu = true;
        }
        ImGui::End();
        return;
    }

    ImGuiStyle& Style = ImGui::GetStyle();
    // Modern ImGui Dark Theme with Cyan Accent
    Style.WindowRounding = 8.0f;
    Style.FrameRounding = 6.0f;
    Style.PopupRounding = 6.0f;
    Style.ScrollbarRounding = 6.0f;
    Style.GrabRounding = 6.0f;
    Style.TabRounding = 6.0f;
    Style.ChildRounding = 6.0f;

    Style.WindowBorderSize = 0.0f;
    Style.FrameBorderSize = 0.0f;
    Style.PopupBorderSize = 1.0f;

    ImVec4* colors = Style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.12f, 0.12f, 0.12f, 0.94f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.15f, 0.15f, 0.15f, 0.90f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.20f, 0.21f, 0.22f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.40f, 0.40f, 0.40f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.18f, 0.18f, 0.18f, 0.67f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.26f, 0.98f, 0.85f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.88f, 0.77f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.98f, 0.85f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]              = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("LINX Lite - ESP", nullptr, ImGuiWindowFlags_None);
    g_window = ImGui::GetCurrentWindow();

    // Button in top right to hide menu
    if (ImGui::Button("[-] Ocultar Menú", ImVec2(140, 30))) {
        show_menu = false;
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.26f, 0.98f, 0.85f, 1.0f), "Kernel 4.14.199 Native Driver Active");

    if (ImGui::BeginTabBar("MainTabBar"))
    {

        if (ImGui::BeginTabItem("    Log / Debug    "))
        {
            ImGui::Text("Base libUE4: %p", (void*)libbase);
            ImGui::Text("GNames Pool: %p", (void*)GName);
            ImGui::Text("UWorld:      %p", (void*)Uworld);
            ImGui::Text("Actor Count: %d", Count);
            // Debug: clases detectadas por ActorScanThread
            pthread_mutex_lock(&g_debug_mutex);
            ImGui::Text("Clases ESP (%d):", g_debug_class_count);
            for (int _d = 0; _d < g_debug_class_count; _d++) {
                ImGui::TextColored(ImVec4(1,0.8f,0.2f,1), "  %s", g_debug_classes[_d]);
            }
            if (g_debug_class_count == 0) ImGui::TextDisabled("  (ninguna aun)");
            pthread_mutex_unlock(&g_debug_mutex);
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", g_panorama_status);
            if (ImGui::Button("📸 Generar Dump Panorama Completo", { -1, 45 })) {
                DumpPanoramaLog(libbase, GName, Uworld, pid);
            }
            ImGui::Separator();
            if (ImGui::Button("Save Config", { -1, 40 })) NumIoSave("R2HTEAM");
            if (ImGui::Button("Load Config", { -1, 40 })) NumIoLoad("R2HTEAM");
            if (ImGui::Button("Exit ESP", { -1, 40 })) exit(0);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Main ESP"))
        {
            ImGui::Checkbox("Enemy Box", &DrawIo[9]);
            ImGui::SameLine();
            ImGui::Checkbox("Skeleton", &DrawIo[3]);
            ImGui::SameLine();
            ImGui::Checkbox("Ray / Snapline", &DrawIo[2]);
            ImGui::Checkbox("Player Name", &DrawIo[4]);
            ImGui::SameLine();
            ImGui::Checkbox("Distance", &DrawIo[5]);
            ImGui::SameLine();
            ImGui::Checkbox("Ammo Count", &DrawIo[6]);
            ImGui::Checkbox("Armor", &DrawIo[7]);
            ImGui::SameLine();
            ImGui::Checkbox("Radar", &DrawIo[8]);
            ImGui::SameLine();
            ImGui::Checkbox("Crosshair", &DrawIo[10]);
            ImGui::Checkbox("Ignore Bots", &DrawIo[14]);
            ImGui::SliderFloat("RadarX", &NumIo[1], 0, 3400);
            ImGui::SliderFloat("RadarY", &NumIo[2], 0, 1080);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Supplies")) {
            ImGui::Checkbox("Initialize supplies", &DrawIo[22]);
            ImGui::Checkbox("Material prices", &DrawIo[23]);
            ImGui::SameLine();
            ImGui::Checkbox("Material Distance", &DrawIo[24]);
            ImGui::SliderFloat("Material price filter", &NumIo[5], 0.0f, 100000000, "%.0f", 1);
            if (ImGui::Button("-1000 ", { 200, 50})) {
                NumIo[5] = NumIo[5] - 1000;
            }
            ImGui::SameLine();
            if (ImGui::Button("+1000 ", { 200, 50 })) {
                NumIo[5] = NumIo[5] + 1000;
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("    Settings    ")) {
            if (ImGui::Button(" Save Settings ", {-1, 50})) {
                NumIoSave(oxorany("R2HTEAM"));
            }
            ImGui::Text("Average frame rate: %.1f FPS (%.2f ms)",
                        ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    // Visual touch indicator on screen
    if (ImGui::GetIO().MouseDown[0]) {
    }
}
  