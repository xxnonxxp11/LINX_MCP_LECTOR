#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <vector>
#include <sys/stat.h>

static char g_panorama_status[256] = "Presiona 'Dump Panorama' para volcado completo";

struct FQuatLog { float X, Y, Z, W; };
struct FTransformDataLog {
    FQuatLog Rotation;
    Vector3A Translation;
    float pad0;
    Vector3A Scale3D;
    float pad1;
};

inline FMatrix TransformToMatrixLog(const FTransformDataLog& t) {
    FMatrix m;
    float x2 = t.Rotation.X + t.Rotation.X;
    float y2 = t.Rotation.Y + t.Rotation.Y;
    float z2 = t.Rotation.Z + t.Rotation.Z;
    float xx = t.Rotation.X * x2;
    float xy = t.Rotation.X * y2;
    float xz = t.Rotation.X * z2;
    float yy = t.Rotation.Y * y2;
    float yz = t.Rotation.Y * z2;
    float zz = t.Rotation.Z * z2;
    float wx = t.Rotation.W * x2;
    float wy = t.Rotation.W * y2;
    float wz = t.Rotation.W * z2;

    m.M[0][0] = (1.0f - (yy + zz)) * (t.Scale3D.X != 0.0f ? t.Scale3D.X : 1.0f);
    m.M[0][1] = (xy + wz) * (t.Scale3D.X != 0.0f ? t.Scale3D.X : 1.0f);
    m.M[0][2] = (xz - wy) * (t.Scale3D.X != 0.0f ? t.Scale3D.X : 1.0f);
    m.M[0][3] = 0.0f;

    m.M[1][0] = (xy - wz) * (t.Scale3D.Y != 0.0f ? t.Scale3D.Y : 1.0f);
    m.M[1][1] = (1.0f - (xx + zz)) * (t.Scale3D.Y != 0.0f ? t.Scale3D.Y : 1.0f);
    m.M[1][2] = (yz + wx) * (t.Scale3D.Y != 0.0f ? t.Scale3D.Y : 1.0f);
    m.M[1][3] = 0.0f;

    m.M[2][0] = (xz + wy) * (t.Scale3D.Z != 0.0f ? t.Scale3D.Z : 1.0f);
    m.M[2][1] = (yz - wx) * (t.Scale3D.Z != 0.0f ? t.Scale3D.Z : 1.0f);
    m.M[2][2] = (1.0f - (xx + yy)) * (t.Scale3D.Z != 0.0f ? t.Scale3D.Z : 1.0f);
    m.M[2][3] = 0.0f;

    m.M[3][0] = t.Translation.X;
    m.M[3][1] = t.Translation.Y;
    m.M[3][2] = t.Translation.Z;
    m.M[3][3] = 1.0f;
    return m;
}

inline FMatrix MatrixMultLog(const FMatrix& a, const FMatrix& b) {
    FMatrix r;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            r.M[i][j] = a.M[i][0] * b.M[0][j] +
                        a.M[i][1] * b.M[1][j] +
                        a.M[i][2] * b.M[2][j] +
                        a.M[i][3] * b.M[3][j];
        }
    }
    return r;
}

inline std::string GetCurrentTimestampStr() {
    time_t now = time(nullptr);
    struct tm tstruct;
    localtime_r(&now, &tstruct);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tstruct);
    return std::string(buf);
}

inline void DumpPanoramaLog(uintptr_t libUE4Base, uintptr_t gNamesPool, uintptr_t uWorld, pid_t targetPid) {
    if (!driver || targetPid <= 0) {
        snprintf(g_panorama_status, sizeof(g_panorama_status), "[!] Error: Driver o PID no inicializado");
        return;
    }

    std::string ts = GetCurrentTimestampStr();
    
    // Ensure output directories
    system("mkdir -p /data/local/tmp/linx_panorama");
    system("chmod 777 /data/local/tmp/linx_panorama");

    std::string outPath1 = "/data/local/tmp/linx_panorama/panorama_" + ts + ".txt";
    std::string outPath2 = "/sdcard/Download/linx_panorama/panorama_" + ts + ".txt";

    FILE* f = fopen(outPath1.c_str(), "w");
    if (!f) {
        system("mkdir -p /sdcard/Download/linx_panorama");
        f = fopen(outPath2.c_str(), "w");
    }
    if (!f) {
        snprintf(g_panorama_status, sizeof(g_panorama_status), "[!] Error de permisos al guardar dump (out1=%s)", outPath1.c_str());
        return;
    }

    fprintf(f, "================================================================================\n");
    fprintf(f, "               ARENA BREAKOUT LITE - PANORAMA DIAGNOSTIC LOG                    \n");
    fprintf(f, "================================================================================\n");
    fprintf(f, "Timestamp:    %s\n", ts.c_str());
    fprintf(f, "PID:          %d\n", targetPid);
    fprintf(f, "libUE4 Base:  0x%lx\n", (unsigned long)libUE4Base);
    fprintf(f, "GNames Pool:  0x%lx\n", (unsigned long)gNamesPool);
    fprintf(f, "UWorld:       0x%lx\n", (unsigned long)uWorld);
    fprintf(f, "Screen Res:   %.0fx%.0f\n", screen_x, screen_y);
    fprintf(f, "--------------------------------------------------------------------------------\n\n");

    if (!uWorld) {
        fprintf(f, "[!] UWorld es NULL. El juego no esta en partida o el puntero cambio.\n");
        fclose(f);
        snprintf(g_panorama_status, sizeof(g_panorama_status), "[!] UWorld NULL - Entra a partida primero");
        return;
    }

    // 1. Hierarchy analysis
    uintptr_t gameInstance    = driver->read<uintptr_t>(uWorld + 0x180);
    uintptr_t localPlayerPtr  = driver->read<uintptr_t>(gameInstance + 0x38);
    uintptr_t localPlayer     = driver->read<uintptr_t>(localPlayerPtr + 0x0);
    uintptr_t playerController= driver->read<uintptr_t>(localPlayer + 0x30);
    uintptr_t localPawn       = driver->read<uintptr_t>(playerController + 0x380);
    uintptr_t cameraManager   = driver->read<uintptr_t>(playerController + 0x398);
    uintptr_t persistentLevel = driver->read<uintptr_t>(uWorld + 0x30);
    uintptr_t actorArrayPtr   = driver->read<uintptr_t>(persistentLevel + 0x98);
    int       actorCount      = driver->read<int>(persistentLevel + 0xA0);

    fprintf(f, "[1] HIERARCHY & CONTROLLER INFO:\n");
    fprintf(f, "  GameInstance:       0x%lx\n", (unsigned long)gameInstance);
    fprintf(f, "  LocalPlayers Array: 0x%lx\n", (unsigned long)localPlayerPtr);
    fprintf(f, "  LocalPlayer[0]:     0x%lx\n", (unsigned long)localPlayer);
    fprintf(f, "  PlayerController:   0x%lx\n", (unsigned long)playerController);
    fprintf(f, "  AcknowledgedPawn:   0x%lx\n", (unsigned long)localPawn);
    fprintf(f, "  CameraManager:      0x%lx\n", (unsigned long)cameraManager);
    fprintf(f, "  PersistentLevel:    0x%lx\n", (unsigned long)persistentLevel);
    fprintf(f, "  Actor Array Ptr:    0x%lx\n", (unsigned long)actorArrayPtr);
    fprintf(f, "  Actor Count:        %d\n\n", actorCount);

    // 2. Camera Manager Probing
    fprintf(f, "[2] CAMERA MANAGER PROBING & VIEW INFO:\n");
    if (cameraManager) {
        uintptr_t camOffsets[] = { 0x1EC0, 0x1ED0, 0x1EE0, 0x1EB0, 0x1240, 0x1250 };
        for (uintptr_t coff : camOffsets) {
            Vector3A loc = driver->read<Vector3A>(cameraManager + coff);
            FRotator rot = driver->read<FRotator>(cameraManager + coff + 0xC);
            float fov    = driver->read<float>(cameraManager + coff + 0x18);
            fprintf(f, "  Offset +0x%lx: Loc=(%.2f, %.2f, %.2f) Rot=(P=%.2f, Y=%.2f, R=%.2f) FOV=%.2f\n",
                    (unsigned long)coff, loc.X, loc.Y, loc.Z, rot.Pitch, rot.Yaw, rot.Roll, fov);
        }

        fprintf(f, "\n  [CameraManager Raw Memory 0x1EA0 - 0x1F20]:\n");
        for (uintptr_t roff = 0x1EA0; roff <= 0x1F10; roff += 0x10) {
            float f0 = driver->read<float>(cameraManager + roff);
            float f1 = driver->read<float>(cameraManager + roff + 4);
            float f2 = driver->read<float>(cameraManager + roff + 8);
            float f3 = driver->read<float>(cameraManager + roff + 12);
            fprintf(f, "    +0x%04lx:  %12.2f  %12.2f  %12.2f  %12.2f\n",
                    (unsigned long)roff, f0, f1, f2, f3);
        }
    } else {
        fprintf(f, "  [!] CameraManager is NULL\n");
    }
    fprintf(f, "\n");

    // Camera in use for projection
    MinimalViewInfo activeCam;
    activeCam.Location = driver->read<Vector3A>(cameraManager + 0x1EC0);
    activeCam.Rotation = driver->read<FRotator>(cameraManager + 0x1ECC);
    activeCam.FOV      = driver->read<float>(cameraManager + 0x1ED8);
    if (activeCam.FOV < 10.0f || activeCam.FOV > 170.0f) activeCam.FOV = 90.0f;

    fprintf(f, "[3] ACTIVE CAMERA PROJECTION MATRIX:\n");
    FMatrix camMat = RotToMatrix(activeCam.Rotation);
    for (int r = 0; r < 4; r++) {
        fprintf(f, "  [Row %d]: %8.4f %8.4f %8.4f %8.4f\n",
                r, camMat.M[r][0], camMat.M[r][1], camMat.M[r][2], camMat.M[r][3]);
    }
    fprintf(f, "\n");

    // 3. Local Pawn Details
    fprintf(f, "[4] LOCAL PAWN ANALYSIS:\n");
    if (localPawn) {
        int pawnNameId = driver->read<int>(localPawn + 0x18);
        char* pawnClass = 获取类名(pawnNameId);
        fprintf(f, "  Class: %s (ID: %d)\n", pawnClass ? pawnClass : "Unknown", pawnNameId);

        uintptr_t rootOffsets[] = { 0x158, 0x160, 0x168, 0x170, 0x180, 0x188, 0x228, 0x230 };
        for (uintptr_t roff : rootOffsets) {
            uintptr_t rootComp = driver->read<uintptr_t>(localPawn + roff);
            if (rootComp) {
                Vector3A loc154 = driver->read<Vector3A>(rootComp + 0x154);
                Vector3A loc160 = driver->read<Vector3A>(rootComp + 0x160);
                Vector3A loc190 = driver->read<Vector3A>(rootComp + 0x190);
                Vector3A loc1A0 = driver->read<Vector3A>(rootComp + 0x1A0);
                Vector3A loc1D0 = driver->read<Vector3A>(rootComp + 0x1D0);
                fprintf(f, "    RootComp at +0x%lx (Ptr: 0x%lx):\n", (unsigned long)roff, (unsigned long)rootComp);
                fprintf(f, "      +0x154: (%.2f, %.2f, %.2f)\n", loc154.X, loc154.Y, loc154.Z);
                fprintf(f, "      +0x160: (%.2f, %.2f, %.2f)\n", loc160.X, loc160.Y, loc160.Z);
                fprintf(f, "      +0x190: (%.2f, %.2f, %.2f)\n", loc190.X, loc190.Y, loc190.Z);
                fprintf(f, "      +0x1A0: (%.2f, %.2f, %.2f)\n", loc1A0.X, loc1A0.Y, loc1A0.Z);
                fprintf(f, "      +0x1D0: (%.2f, %.2f, %.2f)\n", loc1D0.X, loc1D0.Y, loc1D0.Z);
            }
        }
    } else {
        fprintf(f, "  [!] LocalPawn is NULL\n");
    }
    fprintf(f, "\n");

    // 4. Full Actor Scan & Coordinate Decryption / Probing
    fprintf(f, "[5] ACTORS DETAILED PROBE (Total: %d):\n", actorCount);
    fprintf(f, "================================================================================\n");

    int validActorCount = 0;
    int characterCount = 0;
    int itemCount = 0;

    for (int i = 0; i < actorCount && i < 1500; i++) {
        uintptr_t actor = driver->read<uintptr_t>(actorArrayPtr + 0x8 * i);
        if (!actor) continue;
        validActorCount++;

        int nameId = driver->read<int>(actor + 0x18);
        char* className = 获取类名(nameId);
        if (!className) className = (char*)"Unknown";

        bool isChar = (strstr(className, "Character") || strstr(className, "Player") || 
                       strstr(className, "Pawn") || strstr(className, "Bot") || strstr(className, "AI"));
        bool isItem = (strstr(className, "Item") || strstr(className, "Inventory") || 
                       strstr(className, "Box") || strstr(className, "Loot") || strstr(className, "Drop"));

        if (isChar) {
            characterCount++;
            fprintf(f, "\n--------------------------------------------------------------------------------\n");
            fprintf(f, "[CHAR #%d] Actor: 0x%lx | Class: %s (ID: %d)%s\n",
                    characterCount, (unsigned long)actor, className, nameId, (actor == localPawn) ? " [LOCAL PLAYER]" : "");

            // Probe RootComponent & Locations
            uintptr_t rootOffsets[] = { 0x158, 0x160, 0x168, 0x180, 0x188 };
            for (uintptr_t roff : rootOffsets) {
                uintptr_t rootComp = driver->read<uintptr_t>(actor + roff);
                if (rootComp) {
                    Vector3A loc154 = driver->read<Vector3A>(rootComp + 0x154);
                    Vector3A loc1A0 = driver->read<Vector3A>(rootComp + 0x1A0);
                    Vector3A loc1D0 = driver->read<Vector3A>(rootComp + 0x1D0);
                    
                    Vector2A scr154 = WorldToScreen(loc154, activeCam);
                    float dist154 = (loc154 - activeCam.Location).length() / 100.0f;

                    fprintf(f, "  RootComp at +0x%lx (0x%lx):\n", (unsigned long)roff, (unsigned long)rootComp);
                    fprintf(f, "    -> Loc[+0x154] = (%.2f, %.2f, %.2f) | Dist: %.1fm | Screen: (%.1f, %.1f) [InScreen: %s]\n",
                            loc154.X, loc154.Y, loc154.Z, dist154, scr154.X, scr154.Y,
                            (scr154.X >= 0 && scr154.X <= screen_x && scr154.Y >= 0 && scr154.Y <= screen_y) ? "YES" : "NO");
                    fprintf(f, "    -> Loc[+0x1A0] = (%.2f, %.2f, %.2f)\n", loc1A0.X, loc1A0.Y, loc1A0.Z);
                    fprintf(f, "    -> Loc[+0x1D0] = (%.2f, %.2f, %.2f)\n", loc1D0.X, loc1D0.Y, loc1D0.Z);
                }
            }

            // [ENCRIPTADO] Bones & Mesh:
            // Sabiendo que FTransform está encriptado por VM::TransformEncrypt, saltamos el cálculo de huesos
            // para evitar escupir basura NaN en los logs. En su lugar, calculamos la caja 2D esperada.
            uintptr_t rootComp = driver->read<uintptr_t>(actor + 0x158);
            if (rootComp) {
                Vector3A rootLoc = driver->read<Vector3A>(rootComp + 0x154);
                Vector3A head3D = rootLoc;
                head3D.Z += 80.0f;
                Vector3A feet3D = rootLoc;
                feet3D.Z -= 90.0f;
                
                Vector2A headScr = WorldToScreen(head3D, activeCam);
                Vector2A feetScr = WorldToScreen(feet3D, activeCam);
                
                float boxHeight = abs(headScr.Y - feetScr.Y);
                float boxWidth = boxHeight * 0.5f;
                float boxTop = headScr.Y - (boxHeight * 0.1f);
                float boxLeft = headScr.X - (boxWidth / 2.0f);
                
                fprintf(f, "    [VM ENCRYPTED] Mesh/Bones Skipped.\n");
                fprintf(f, "    [2D BOX PROJECTION] Head: (%.1f, %.1f) | Feet: (%.1f, %.1f)\n", headScr.X, headScr.Y, feetScr.X, feetScr.Y);
                fprintf(f, "    [2D BOX CALC] X: %.1f, Y: %.1f, W: %.1f, H: %.1f\n", boxLeft, boxTop, boxWidth, boxHeight);
            }

            // Player State & Attributes
            uintptr_t playerState = driver->read<uintptr_t>(actor + 0x330);
            uintptr_t attrComp    = driver->read<uintptr_t>(actor + 0xA88);
            if (!attrComp) attrComp = driver->read<uintptr_t>(actor + 0x930);
            float health = driver->read<float>(attrComp + 0x110);
            int teamId   = driver->read<int>(playerState + 0x388);
            fprintf(f, "  PlayerState: 0x%lx | TeamID: %d | AttrComp: 0x%lx | Health: %.1f\n",
                    (unsigned long)playerState, teamId, (unsigned long)attrComp, health);
        }
        else if (isItem && itemCount < 50) {
            itemCount++;
            uintptr_t rootComp = driver->read<uintptr_t>(actor + 0x158);
            Vector3A itemPos = driver->read<Vector3A>(rootComp + 0x154);
            Vector2A itemScr = WorldToScreen(itemPos, activeCam);
            float itemDist = (itemPos - activeCam.Location).length() / 100.0f;
            
            fprintf(f, "[ITEM #%d] Actor: 0x%lx | Class: %s | 3D: (%.1f, %.1f, %.1f) | Dist: %.1fm | Screen: (%.1f, %.1f)\n",
                    itemCount, (unsigned long)actor, className, itemPos.X, itemPos.Y, itemPos.Z, itemDist, itemScr.X, itemScr.Y);
        }
    }

    fprintf(f, "\n================================================================================\n");
    fprintf(f, "SUMMARY:\n");
    fprintf(f, "  Total Actors Scanned: %d\n", validActorCount);
    fprintf(f, "  Characters Found:     %d\n", characterCount);
    fprintf(f, "  Items Logged:         %d\n", itemCount);
    fprintf(f, "================================================================================\n");
    fclose(f);

    snprintf(g_panorama_status, sizeof(g_panorama_status), "[+] Log guardado: /sdcard/Download/linx_panorama/panorama_%s.txt", ts.c_str());
}
