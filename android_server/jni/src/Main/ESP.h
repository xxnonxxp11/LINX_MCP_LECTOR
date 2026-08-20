#pragma once
#include "Structs.h"
#include <chrono>
int MaxPlayerCount = 0;
char PlayerName[100];
float Health;
float PushX;
string SaveDir;
FILE *numSave = nullptr;
float PushY;
int PlayerCount = 0, style_idx2 = 2;
ImColor BoxColor = { 255, 155, 155, 255 };
ImColor BotBoxColor = ImColor(0, 255, 0, 255);
ImColor LineColor = ImColor(255, 155, 155, 255);
ImColor BotLineColor = ImColor(0, 255, 0, 255);
ImColor BoneColor = ImColor(255, 0, 0, 255);
ImColor BotBoneColor = ImColor(0, 255, 0, 255);
ImColor RightColor = ImColor(255, 200, 0, 255);
ImColor BotRightColor = ImColor(0, 255, 0, 255);
ImColor WarningColor = ImColor(255, 155, 155, 255);
ImColor BotWarningColor = ImColor(0, 255, 0, 255);
bool cshzt = false;
Vector2A Head2;

Vector2A Target, Slide;
static float TargetX = 0;
static float TargetY = 0;

// 定义TouchX和TouchY，初始设置为屏幕宽高的60%，用于表示触摸点的位置
int TouchX = TouchPosx;
int TouchY = TouchPosy;

// AimBotAuto函数：自动瞄准功能


void AimBotAuto(Vector2A gg)
{
	// 计算TargetX：屏幕中心X坐标(px)与HeadX坐标的差值
	/*TargetX = px - gg.X;

	// 计算TargetY：屏幕中心Y坐标(py)与HeadY坐标（翻转后）的差值
	TargetY = py - (displayInfo.height - gg.Y);
    Screen_AimBone*/
    float Aimspeace = NumIo[4];
    gg.Y = screen_y - gg.Y;
    Target.Y = screen_y / 2 - gg.Y - 10;
    Target.X = screen_x / 2 - gg.X - 13;
	// 将TargetX和TargetY分别除以NumIo[10]，用于调整瞄准的精细度
	Slide.X = Target.X / Aimspeace;
	Slide.Y = Target.Y / Aimspeace;

	// 更新TouchX和TouchY，减去对应的TargetX和TargetY，用于移动触摸点以对准目标
	TouchX -= Slide.X;
	TouchY -= Slide.Y;

	// 设定边界值
	float minX = displayInfo.width * 0.6f - 150;
	float maxX = displayInfo.width * 0.6f + 150;
	float minY = displayInfo.height * 0.6f - 150;
	//bool isDown = true;
	float maxY = displayInfo.height * 0.6f + 150;

	// 检查TouchX是否超出边界
	/*if (TouchX < minX || TouchX > maxX || TouchY < minY || TouchY > maxY) {
		Touch::Up();
		
		/*TouchX = displayInfo.width * 0.6f;
		TouchY = displayInfo.height * 0.6f;*/
	//}
    if (displayInfo.orientation != 1) {
            		Touch_Down(screen_y - TouchY, screen_x - TouchX);
            		//bool isDown = true;
			 	} else {
					Touch_Down(TouchY, TouchX);
					
				}
				
	//Touch::setOtherTouch(false);
	// 调用Touch_Down函数，模拟触摸屏幕操作，完成瞄准动作
	//Touch::Move(TouchX, TouchY);
}


void screen_config2() {
    ::displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
    ::screen_x = displayInfo.width;
    ::screen_y = displayInfo.height;
}


ImColor Colors[20] = {0};
int 热成像判断;
void LoadMain() {
    Colors[0] = {255,0,0,255};
    Colors[1] = {0,255,0,255};
    Colors[2] = {255,255,255,255};
    Colors[3] = {255,255,255,255};
    Colors[4] = {255,255,255,255};
    Colors[5] = {255,255,255,255};
}
using namespace std;
using namespace std::chrono;

long long getTotalCPUTime() {
    ifstream file("/proc/stat");
    string line;
    getline(file, line); // 读取第一行包含cpu时间的行
    file.close();
    
    long long total_time = 0;
    string temp;
    istringstream ss(line);
    ss >> temp; // 忽略"cpu"字符串
    
    long long user, nice, system, idle, iowait, irq, softirq, steal;
    ss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    total_time = user + nice + system + idle + iowait + irq + softirq + steal;

    return total_time;
}

long long getProcessCPUTime(int pid) {
    string stat_file = "/proc/" + to_string(pid) + "/stat";
    ifstream file(stat_file);
    string line;
    getline(file, line); // 读取进程的stat文件
    file.close();
    
    long long utime, stime;
    istringstream ss(line);
    string temp;
    
    for (int i = 0; i < 13; i++) { // 忽略前13个字段
        ss >> temp;
    }
    
    ss >> utime >> stime; // 第14和第15字段为utime和stime
    
    return utime + stime;
}


#include "../res/cJSON.h"
#ifndef CJSON_INCLUDED_ONCE
#define CJSON_INCLUDED_ONCE

#endif

void NumIoSave(const char *name) {
    system("mkdir -p /data/local/tmp/linxsave");
    cJSON *root = cJSON_CreateObject();
    
    cJSON *drawArray = cJSON_CreateArray();
    for (int i = 0; i < 50; i++) {
        cJSON_AddItemToArray(drawArray, cJSON_CreateBool(DrawIo[i]));
    }
    cJSON_AddItemToObject(root, "DrawIo", drawArray);
    
    cJSON *numArray = cJSON_CreateArray();
    for (int i = 0; i < 50; i++) {
        cJSON_AddItemToArray(numArray, cJSON_CreateNumber(NumIo[i]));
    }
    cJSON_AddItemToObject(root, "NumIo", numArray);
    
    cJSON_AddNumberToObject(root, "CoordinatesSwitch", CoordSwitch);
    
    char *json_str = cJSON_Print(root);
    FILE *f = fopen("/data/local/tmp/linxsave/config.json", "w");
    if (f) {
        fputs(json_str, f);
        fclose(f);
    }
    free(json_str);
    cJSON_Delete(root);
}

void NumIoLoad(const char *name) {
    FILE *f = fopen("/data/local/tmp/linxsave/config.json", "r");
    if (!f) {
        NumIo[1] = 300.0f; NumIo[2] = 400.0f; NumIo[3] = 150.0f; NumIo[4] = 20.0f;
        DrawIo[9] = true;  // Cajas 2D: ON por defecto
        DrawIo[5] = true;  // Distancia: ON por defecto
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *json_str = (char *)malloc(fsize + 1);
    fread(json_str, 1, fsize, f);
    fclose(f);
    json_str[fsize] = 0;
    
    cJSON *root = cJSON_Parse(json_str);
    if (root) {
        cJSON *drawArray = cJSON_GetObjectItem(root, "DrawIo");
        if (drawArray) {
            for (int i = 0; i < 50 && i < cJSON_GetArraySize(drawArray); i++) {
                DrawIo[i] = cJSON_IsTrue(cJSON_GetArrayItem(drawArray, i));
            }
        }
        
        cJSON *numArray = cJSON_GetObjectItem(root, "NumIo");
        if (numArray) {
            for (int i = 0; i < 50 && i < cJSON_GetArraySize(numArray); i++) {
                NumIo[i] = cJSON_GetArrayItem(numArray, i)->valuedouble;
            }
        }
        
        cJSON *coord = cJSON_GetObjectItem(root, "CoordinatesSwitch");
        if (coord) {
            CoordSwitch = coord->valueint;
        }
        
        cJSON_Delete(root);
    }
    free(json_str);
}



inline char* 获取类名(int 标识符) {
    static char buf[256];
    if (标识符 <= 0 || libbase == 0) return (char*)"";
    uint32_t block_idx = (标识符 >> 16);
    uint32_t offset = (标识符 & 0xFFFF);
    uintptr_t block_ptr = driver->read<uintptr_t>(libbase + 0xEDE48C0 + 0x48 + block_idx * 8);
    if (!block_ptr) return (char*)"";
    uintptr_t entry = block_ptr + 2 * offset;
    uint16_t header = driver->read<uint16_t>(entry);
    int len = header >> 6;
    if (len <= 0 || len >= 256) return (char*)"";
    driver->read(entry + 2, buf, len);
    buf[len] = '\0';
    return buf;
}

#include "PanoramaLogger.h"

static volatile uintptr_t cached_uworld = 0;
static volatile bool       world_scan_running = false;
static volatile uintptr_t cached_pcm = 0;        // PlayerCameraManager directo
static volatile uintptr_t cached_cam_off = 0;     // Offset dentro del PCM donde esta la camara
static char g_cam_debug[128] = "buscando...";     // Para debug en ImGui

static void* WorldScanThread(void*) {
    while (true) {
        usleep(1000000); // 1 segundo por ciclo

        if (!libbase) continue;
        uintptr_t guobj   = libbase + 0xEE047F0;
        uintptr_t objects = driver->read<uintptr_t>(guobj + 0x10);
        if (!objects) continue;

        // Buscar la posicion del jugador no es posible aqui (globals declarados despues)
        // La validacion de camara se hara solo por FOV + rango de posicion valida
        Vector3A selfPos = {}; // vacio = sin validacion de proximidad

        uintptr_t best_world = 0;
        uintptr_t best_pcm   = 0;
        uintptr_t best_off   = 0;

        for (int ci = 0; ci < 15; ci++) {
            uintptr_t chunk = driver->read<uintptr_t>(objects + ci * 8);
            if (!chunk) continue;
            for (int i = 0; i < 65536; i++) {
                uintptr_t obj = driver->read<uintptr_t>(chunk + i * 24);
                if (!obj || obj < 0x1000000000ULL) continue;
                uintptr_t cls = driver->read<uintptr_t>(obj + 0x10);
                if (!cls || cls < 0x1000000000ULL) continue;
                uint32_t cls_name_id = driver->read<uint32_t>(cls + 0x18);
                char* cname = 获取类名(cls_name_id);
                if (!cname || cname[0] == '\0') continue;

                // Buscar UWorld y extraer el PCM REAL de su cadena (fuente autoritativa)
                uintptr_t chain_pcm = 0;
                if (!best_world && (strstr(cname, "World") || strstr(cname, "world"))) {
                    uintptr_t ginst = driver->read<uintptr_t>(obj + 0x180);
                    if (ginst > 0x1000000000ULL) {
                        uintptr_t lp_arr = driver->read<uintptr_t>(ginst + 0x38);
                        uintptr_t lp     = driver->read<uintptr_t>(lp_arr);
                        uintptr_t pc     = driver->read<uintptr_t>(lp + 0x30);
                        uintptr_t pcm    = driver->read<uintptr_t>(pc + 0x398);
                        if (pcm > 0x1000000000ULL) {
                            best_world = obj;
                            chain_pcm  = pcm;
                        }
                    }
                }

                // Si tenemos el PCM real de la cadena, escanear el offset de camara sobre EL
                if (chain_pcm && !best_pcm) {
                    static const uintptr_t cam_try_offsets[] = {
                        0x1100,0x1150,0x1200,0x1250,0x1300,0x1350,0x1400,0x1450,
                        0x1500,0x1550,0x1600,0x1650,0x1700,0x1750,0x1800,0x1850,
                        0x1900,0x1950,0x1A00,0x1A50,0x1B00,0x1B50,0x1BC0,0x1C00,
                        0x1D00,0x1D50,0x1E00,0x1ED0,0x1F00,0x1F50,0x2000
                    };
                    for (uintptr_t try_off : cam_try_offsets) {
                        MinimalViewInfo ci_test = driver->read<MinimalViewInfo>(chain_pcm + try_off);
                        if (ci_test.FOV < 35.0f || ci_test.FOV > 145.0f) continue;
                        float ax = ci_test.Location.X < 0 ? -ci_test.Location.X : ci_test.Location.X;
                        float ay = ci_test.Location.Y < 0 ? -ci_test.Location.Y : ci_test.Location.Y;
                        if (ax < 50.0f && ay < 50.0f) continue;
                        if (ax > 5000000.0f || ay > 5000000.0f) continue;
                        best_pcm = chain_pcm;
                        best_off = try_off;
                        char tmp[160];
                        snprintf(tmp, sizeof(tmp), "chain PCM +0x%lX FOV=%.0f", try_off, (double)ci_test.FOV);
                        strncpy(g_cam_debug, tmp, 127);
                        g_cam_debug[127] = '\0';
                        break;
                    }
                }

                // Fallback 1: clase PlayerCameraManager especifica
                if (!best_pcm && (strstr(cname, "CameraManager") || strstr(cname, "PlayerCamera"))) {
                    static const uintptr_t cam_try_offsets[] = {
                        0x1100,0x1150,0x1200,0x1250,0x1300,0x1350,0x1400,0x1450,
                        0x1500,0x1550,0x1600,0x1650,0x1700,0x1750,0x1800,0x1850,
                        0x1900,0x1950,0x1A00,0x1A50,0x1B00,0x1B50,0x1BC0,0x1C00,
                        0x1D00,0x1D50,0x1E00,0x1ED0,0x1F00,0x1F50,0x2000
                    };
                    for (uintptr_t try_off : cam_try_offsets) {
                        MinimalViewInfo ci_test = driver->read<MinimalViewInfo>(obj + try_off);
                        if (ci_test.FOV < 35.0f || ci_test.FOV > 145.0f) continue;
                        float ax = ci_test.Location.X < 0 ? -ci_test.Location.X : ci_test.Location.X;
                        float ay = ci_test.Location.Y < 0 ? -ci_test.Location.Y : ci_test.Location.Y;
                        if (ax < 50.0f && ay < 50.0f) continue;
                        if (ax > 5000000.0f || ay > 5000000.0f) continue;
                        best_pcm = obj;
                        best_off = try_off;
                        char tmp[160];
                        snprintf(tmp, sizeof(tmp), "%s +0x%lX FOV=%.0f", cname, try_off, (double)ci_test.FOV);
                        strncpy(g_cam_debug, tmp, 127);
                        g_cam_debug[127] = '\0';
                        break;
                    }
                }

                // Fallback 2: cualquier clase con Camera (ultimo recurso)
                if (!best_pcm && strstr(cname, "Camera")) {
                    strncpy(g_cam_debug, cname, 127);
                    g_cam_debug[127] = '\0';
                    static const uintptr_t cam_try_offsets[] = {
                        0x1100,0x1150,0x1200,0x1250,0x1300,0x1350,0x1400,0x1450,
                        0x1500,0x1550,0x1600,0x1650,0x1700,0x1750,0x1800,0x1850,
                        0x1900,0x1950,0x1A00,0x1A50,0x1B00,0x1B50,0x1BC0,0x1C00,
                        0x1D00,0x1D50,0x1E00,0x1ED0,0x1F00,0x1F50,0x2000
                    };
                    for (uintptr_t try_off : cam_try_offsets) {
                        MinimalViewInfo ci_test = driver->read<MinimalViewInfo>(obj + try_off);
                        if (ci_test.FOV < 35.0f || ci_test.FOV > 145.0f) continue;
                        float ax = ci_test.Location.X < 0 ? -ci_test.Location.X : ci_test.Location.X;
                        float ay = ci_test.Location.Y < 0 ? -ci_test.Location.Y : ci_test.Location.Y;
                        if (ax < 50.0f && ay < 50.0f) continue;
                        if (ax > 5000000.0f || ay > 5000000.0f) continue;
                        best_pcm = obj;
                        best_off = try_off;
                        char tmp[160];
                        snprintf(tmp, sizeof(tmp), "%s +0x%lX FOV=%.0f", cname, try_off, (double)ci_test.FOV);
                        strncpy(g_cam_debug, tmp, 127);
                        g_cam_debug[127] = '\0';
                        break;
                    }
                }

                if (best_world && best_pcm) break;
            }
            if (best_world && best_pcm) break;
        }

        if (best_world > 0x1000000000ULL) cached_uworld = best_world;
        if (best_pcm   > 0x1000000000ULL) {
            cached_pcm     = best_pcm;
            cached_cam_off = best_off;
        }
    }
    return nullptr;
}

static uint64_t 本人地址 = 0;
static std::vector<uintptr_t> g_char_actors;
static pthread_mutex_t g_actor_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool actor_thread_started = false;
// Debug: clase names vistas en el scan (para diagnostico)
static char g_debug_classes[8][128] = {};
static int  g_debug_class_count = 0;
static pthread_mutex_t g_debug_mutex = PTHREAD_MUTEX_INITIALIZER;

static void* ActorScanThread(void*) {
    while (true) {
        usleep(300000); // 300ms en segundo plano
        if (!libbase) continue;

        uintptr_t guobj   = libbase + 0xEE047F0;
        uintptr_t objects = driver->read<uintptr_t>(guobj + 0x10);
        if (!objects) continue;

        std::vector<uintptr_t> local_actors;
        local_actors.reserve(32);

        // Debug: recolectar primeras clases que se vean con posicion valida
        char local_debug[8][128] = {};
        int local_debug_count = 0;

        for (int ci = 0; ci < 15; ci++) {
            uintptr_t chunk = driver->read<uintptr_t>(objects + ci * 8);
            if (!chunk) continue;
            for (int i = 0; i < 65536; i++) {
                uintptr_t obj = driver->read<uintptr_t>(chunk + i * 24);
                if (!obj || obj < 0x1000000000ULL) continue;
                uintptr_t cls = driver->read<uintptr_t>(obj + 0x10);
                if (!cls || cls < 0x1000000000ULL) continue;

                uint32_t cls_name_id = driver->read<uint32_t>(cls + 0x18);
                char* cname = 获取类名(cls_name_id);
                if (!cname || cname[0] == '\0') continue;

                bool is_pawn = false;
                if (strcmp(cname, "BP_UamCharacter_C") == 0 ||
                    strcmp(cname, "BP_RoundCharacter_C") == 0 ||
                    strcmp(cname, "BP_ClientCharacter_C") == 0 ||
                    strcmp(cname, "BP_PlayerCharacter_C") == 0 ||
                    strcmp(cname, "BP_UamBotCharacter_C") == 0) {
                    is_pawn = true;
                }
                if (!is_pawn && strstr(cname, "Character_C") != nullptr) {
                    is_pawn = true;
                }
                if (!is_pawn && strstr(cname, "Pawn") != nullptr && strstr(cname, "BP_") != nullptr) {
                    is_pawn = true;
                }
                if (!is_pawn) continue;

                uintptr_t root = driver->read<uintptr_t>(obj + 0x158);
                if (root < 0x1000000000ULL) continue;
                Vector3A rloc = driver->read<Vector3A>(root + 0x154);
                float ax = rloc.X < 0 ? -rloc.X : rloc.X;
                float ay = rloc.Y < 0 ? -rloc.Y : rloc.Y;
                float az = rloc.Z < 0 ? -rloc.Z : rloc.Z;
                if ((ax < 1.0f && ay < 1.0f) || ax > 2000000.f || ay > 2000000.f || az > 2000000.f) continue;

                // Guardar clase para debug
                if (local_debug_count < 8) {
                    bool already = false;
                    for (int d = 0; d < local_debug_count; d++) {
                        if (strncmp(local_debug[d], cname, 127) == 0) { already = true; break; }
                    }
                    if (!already) {
                        strncpy(local_debug[local_debug_count++], cname, 127);
                    }
                }

                local_actors.push_back(obj);
            }
        }

        pthread_mutex_lock(&g_actor_mutex);
        g_char_actors = std::move(local_actors);
        pthread_mutex_unlock(&g_actor_mutex);

        // Actualizar debug clases
        pthread_mutex_lock(&g_debug_mutex);
        memcpy(g_debug_classes, local_debug, sizeof(local_debug));
        g_debug_class_count = local_debug_count;
        pthread_mutex_unlock(&g_debug_mutex);
    }
    return nullptr;
}

static bool world_thread_started = false;
inline uintptr_t GetLiveWorld() {
    if (!world_thread_started && libbase) {
        world_thread_started = true;
        pthread_t t;
        pthread_create(&t, nullptr, WorldScanThread, nullptr);
        pthread_detach(t);
    }
    if (!actor_thread_started && libbase) {
        actor_thread_started = true;
        pthread_t at;
        pthread_create(&at, nullptr, ActorScanThread, nullptr);
        pthread_detach(at);
    }
    return cached_uworld;
}

bool isZM;
pid_t pid = 0;
Vector3A BoneXY;
float BoneMax = 200;
Vector3A AIMpos;
uint64_t safe;

int dl,dt,dj;
long d1, d2, d3;
bool 初始 = false;
void ArenaBreakoutDraw(ImDrawList * Draw)
{
   ImDrawList* FgDraw = ImGui::GetForegroundDrawList();
   if (!初始){
        LoadMain();
        pid = driver->get_name_pid("com.proximabeta.mf.liteuamo");
        if (pid <= 0) {
            pid = driver->get_name_pid("com.proximabeta.mf.uamo");
        }
        if (pid <= 0) {
            return;
        }

        driver->initialize(pid);
        libbase = driver->get_module_base("libUE4.so");
        if (libbase == 0) {
            return;
        }
        GName = libbase + 0xEDE48C0;  // FNamePool: offset relativo a libbase
        初始 = true;
   }
   screen_config2();
   drawn_positions.clear();
   float top,right,left,bottom,x1,top1; 
   py = screen_y/2;  
   px = screen_x/2;

   double closestDistance = std::numeric_limits<double>::infinity();

       // Siempre re-validar UWorld cada frame (no cachear un valor malo)
   {
       uintptr_t candidate = 0;

       // 1) Preferir el valor del WorldScanThread (busca en GUObjectArray con FName correcto)
       candidate = GetLiveWorld(); // arranca los threads si no corren

       // 2) Si WorldScanThread aún no encontró nada, probar offsets estáticos conocidos
       if (!candidate) {
           uintptr_t gworld_offsets[] = {
               0xEE6E1D8ULL,  // Offset correcto: gworld_ptr - libbase (0x752204a1d8 - 0x75131dc000)
               0xED40AD8ULL, 0xEDB51D8ULL, 0xc753c08ULL, 0x100ae640ULL
           };
           for (uintptr_t gwo : gworld_offsets) {
               uintptr_t cand = driver->read<uintptr_t>(libbase + gwo);
               if (cand > 0x1000000000ULL) {
                   uintptr_t vtbl = driver->read<uintptr_t>(cand);
                   if (vtbl > 0x1000000000ULL && vtbl != cand) {
                       candidate = cand; break;
                   }
               }
           }
       }
       Uworld = candidate;
   }

   // Resolve local pawn: requires valid UWorld
   long GameInstance = 0, LocalPlayer = 0, PlayerController = 0;
   long PlayerCameraManager = 0;
   bool isFiring = false, isFiring2 = false;
   if (Uworld) {
       GameInstance = driver->read<long>(Uworld + 0x180);
       LocalPlayer = driver->read<long>(driver->read<long>(GameInstance + 0x38) + 0x0);
       PlayerController = driver->read<long>(LocalPlayer + 0x30);
       PlayerCameraManager = driver->read<long>(PlayerController + 0x398);
       long PlayerInput = driver->read<long>(PlayerController + 0x438);
       isFiring = driver->read<bool>(PlayerInput + 0x3E1);
       isFiring2 = driver->read<bool>(PlayerInput + 0x3E0);
       本人地址 = driver->read<long>(PlayerController + 0x380);
   }

   MinimalViewInfo camera = {};
   bool camera_found = false;
   uintptr_t cam_mgr_cands[] = {
       (uintptr_t)PlayerCameraManager,
       (uintptr_t)driver->read<long>(PlayerController + 0x2D8),
       (uintptr_t)driver->read<long>(PlayerController + 0x2C0),
       (uintptr_t)driver->read<long>(PlayerController + 0x2E0)
   };
   uintptr_t cam_offsets[] = { 0x1100, 0x1ED0, 0x1FE0, 0x1BC0, 0x1B80, 0x1AE0, 0x1F00, 0x2000, 0x1A00, 0x1C00, 0x0EE4, 0x0824 };

   for (uintptr_t cm : cam_mgr_cands) {
       if (cm < 0x1000000000ULL) continue;
       for (uintptr_t coff : cam_offsets) {
           MinimalViewInfo ci = {};
           if (driver->read(cm + coff, &ci, sizeof(ci))) {
               if (ci.FOV >= 35.0f && ci.FOV <= 145.0f && (ci.Location.X != 0.0f || ci.Location.Y != 0.0f)) {
                   camera = ci;
                   camera_found = true;
                   break;
               }
           }
       }
       if (camera_found) break;
   }
   // Guardia: si la camara no tiene Location valida, no dibujar actores
   // (evita recuadros en posiciones aleatorias con camara basura)
   bool camera_usable = camera_found && (camera.Location.X != 0.0f || camera.Location.Y != 0.0f || camera.Location.Z != 0.0f);

   // Fuente primaria: cached_pcm del WorldScanThread (evita cadena UWorld->GameInstance->PC->PCM)
   if (!camera_usable && cached_pcm > 0x1000000000ULL && cached_cam_off > 0) {
       MinimalViewInfo cv = driver->read<MinimalViewInfo>(cached_pcm + cached_cam_off);
       if (cv.FOV >= 35.0f && cv.FOV <= 145.0f &&
           (cv.Location.X != 0.0f || cv.Location.Y != 0.0f)) {
           camera = cv;
           camera_usable = true;
       }
   }
   // Fallback: leer desde PCM de la cadena si es valido
   if (!camera_usable && PlayerCameraManager > 0x1000000000ULL) {
       camera.Location = driver->read<Vector3A>(PlayerCameraManager + 0x1ED0);
       camera.Rotation = driver->read<FRotator>(PlayerCameraManager + 0x1EDC);
       camera.FOV      = driver->read<float>(PlayerCameraManager + 0x1EE8);
       if (camera.FOV < 10.0f || camera.FOV > 170.0f) camera.FOV = 90.0f;
       camera_usable = (camera.Location.X != 0.0f || camera.Location.Y != 0.0f);
   }

   long LPlayerState = driver->read<uintptr_t>(本人地址 + 0x330);
   long SGCharacterWeaponManagerComponent = driver->read<uintptr_t>(本人地址 + 0x1780);
   long CurrentWeapon = driver->read<uintptr_t>(SGCharacterWeaponManagerComponent + 0x138);
   long SGInventoryCommonDataComponent = driver->read<uintptr_t>(CurrentWeapon + 0xB00);
   long displayName = driver->read<uintptr_t>(driver->read<uintptr_t>(driver->read<uintptr_t>(SGInventoryCommonDataComponent + 0x138) + 8) + 0);
   long SGWeaponAssembleComponent = driver->read<uintptr_t>(CurrentWeapon + 0xa48);
   long CachedMagazine = driver->read<uintptr_t>(SGWeaponAssembleComponent + 0x270);
   long SGWeaponContainerComponent = driver->read<uintptr_t>(CachedMagazine + 0x898);
   long BulletContainerInfo = driver->read<uintptr_t>(SGWeaponContainerComponent + 0x1A8);
   int MaxStackCount = driver->read<int>(SGWeaponContainerComponent + 0x108);
   int 本人弹量 = driver->read<int>(BulletContainerInfo + 0x8);
   int 本人团队编号 = driver->read<int>(LPlayerState + 0x5A0);
   int KillEnemyCount = driver->read<int>(LPlayerState + 0x864);
   long SGWeaponFiringComponent = driver->read<uintptr_t>(CurrentWeapon + 0xA68);
   int  bIsShooting = driver->read<int>(SGWeaponFiringComponent + 0x316);

   long LSGWeaponRecoilComponent = driver->read<uintptr_t>(CurrentWeapon + 0xA78);
   long LSGWeaponSpreadComponent = driver->read<uintptr_t>(CurrentWeapon + 0xAC0);

   long Self = 本人地址;
   int 真人数量 = 0;
   int 人机数量 = 0;

   int myTeam = driver->read<int>(driver->read<long>(Self + 0x330) + 0x5A0);
   int myid = driver->read<int>(Self + 0x18);
   int HHCount = 0;
   auto MyPos = driver->read<Vector3A>(driver->read<long>(Self + 0x158) + 0x154);
   long AcknowledgedPawn = 本人地址;
   long CharacterMovement = driver->read<uintptr_t>(AcknowledgedPawn + 0x378);

   std::vector<uintptr_t> char_actors;
   pthread_mutex_lock(&g_actor_mutex);
   char_actors = g_char_actors;
   pthread_mutex_unlock(&g_actor_mutex);
   Count = (int)char_actors.size();

   // ─── Dibujar recuadro del personaje LOCAL siempre (lobby y partida) ────
   if (DrawIo[9] && 本人地址 > 0x1000000000ULL && camera_usable) {
       uintptr_t selfRoot = driver->read<uintptr_t>(本人地址 + 0x158);
       if (selfRoot > 0x1000000000ULL) {
           Vector3A selfLoc = driver->read<Vector3A>(selfRoot + 0x154);
           if (selfLoc.X != 0.0f || selfLoc.Y != 0.0f) {
               Vector3A selfHead = selfLoc; selfHead.Z += 85.0f;
               Vector3A selfFeet = selfLoc; selfFeet.Z -= 95.0f;
               Vector2A sHead = WorldToScreen(selfHead, camera);
               Vector2A sFeet = WorldToScreen(selfFeet, camera);
               if (sHead.X > -500.0f && sHead.Y > -500.0f &&
                   sHead.X < screen_x + 500.0f && sHead.Y < screen_y + 500.0f) {
                   float bh = std::abs(sHead.Y - sFeet.Y);
                   if (bh < 5.0f) bh = 5.0f;
                   float bw = bh * 0.5f;
                   float bl = sHead.X - bw / 2.0f;
                   float bt = sHead.Y - bh * 0.1f;
                   ImColor selfColor(0, 220, 255, 255); // Cyan
                   FgDraw->AddRect({bl, bt}, {bl + bw, bt + bh * 1.1f}, selfColor, 0, 0, 2.0f);
                   FgDraw->AddCircleFilled({sHead.X, sHead.Y}, 4.0f, selfColor);
               }
           }
       }
   }

   // Si la camara no es usable, no dibujar actores (evita basura visual)
   if (!camera_usable) goto skip_actor_draw;

   for (int i = 0; i < Count; i++) {
        long obj = (long)char_actors[i];
        if (!obj) continue;

        int 对象标识符 = driver->read<int>(obj + 0x18);
        char* 对象类名 = 获取类名(对象标识符);
        bool is_character = true; // ya filtrado arriba

        Pos = driver->read<Vector3A>(driver->read<long>(obj + 0x158) + 0x154);
        if (Pos.X == 0 && Pos.Y == 0 && Pos.Z == 0)
            continue;

        if (true) { // clase ya validada arriba


            int TeamIndex;
            int 信息数量 = 0;
            ImColor retColor;
            long PlayerState = driver->read<long>(obj + 0x330);
            bool 是否人机;
            char PlayerNamePrivate[64] = "";
            GetUTF8Text(PlayerNamePrivate, driver->read<uintptr_t>(PlayerState + 0x440));
            if (obj == (long)本人地址)
            {
                retColor = ImColor(0, 220, 255, 255); // Cyan para el personaje local
                是否人机 = false;
            }
            else if (strcmp(PlayerNamePrivate, "") == 0)
            {
                retColor = White;
                人机数量++;
                是否人机 = true;
                if (DrawIo[14])
                    continue;
            }
            else
            {
                TeamIndex = driver->read<int>(PlayerState + 0x5A0);
                if (myTeam != 0 && myTeam == TeamIndex)
                {
                    if (DrawIo[13]) continue;
                    retColor = ImColor(50, 255, 100, 255); // Verde para aliados
                }
                else
                {
                    retColor = ImColor(255, 60, 60, 255); // Rojo para enemigos
                    真人数量++;
                }
                是否人机 = false;
            }
            // Calcular caja 2D desde RootComponent (no está encriptado)
            uintptr_t rootComp = driver->read<long>(obj + 0x158);
            if (!rootComp) continue;
            Vector3A RootLoc = driver->read<Vector3A>(rootComp + 0x154);
            if (RootLoc.X == 0 && RootLoc.Y == 0) continue;

            Vector3A Head3D = RootLoc; Head3D.Z += 85.0f;   // tope de la cabeza
            Vector3A Feet3D = RootLoc; Feet3D.Z -= 95.0f;   // planta de los pies

            Vector2A Head = WorldToScreen(Head3D, camera);
            Vector2A Feet = WorldToScreen(Feet3D, camera);

            // Si el personaje esta detras de la camara o fuera de rango de pantalla, saltar
            if (Head.X < -500.0f || Head.Y < -500.0f || Head.X > (screen_x + 500.0f) || Head.Y > (screen_y + 500.0f)) continue;
            if (Feet.X < -500.0f || Feet.Y < -500.0f || Feet.X > (screen_x + 500.0f) || Feet.Y > (screen_y + 500.0f)) continue;

            // Variables de caja unificadas (todo el texto las usa)
            float boxHeight = std::abs(Head.Y - Feet.Y);
            if (boxHeight < 5.0f) boxHeight = 5.0f;
            float boxWidth  = boxHeight * 0.45f;
            float X    = Head.X - boxWidth / 2.0f;
            float Y    = Head.Y;
            float W    = boxWidth;
            float TOP  = Head.Y;
            float bottom = Feet.Y;

            int 距离 = getDistance(MyPos, RootLoc);
            float angle = driver->read<float>(driver->read<long>(Self + 0x158) + 0x164) - 90;
            Vector2A Radar = rotateCoord(angle, (MyPos.X - RootLoc.X) / 200, (MyPos.Y - RootLoc.Y) / 200);

            long SGCharacterWeaponManagerComponent = driver->read<uintptr_t>(obj + 0x1780);
            long CurrentWeapon = driver->read<uintptr_t>(SGCharacterWeaponManagerComponent + 0x138);
            long SGInventoryCommonDataComponent = driver->read<uintptr_t>(CurrentWeapon + 0xB00);
            long displayName = driver->read<uintptr_t>(driver->read<uintptr_t>(driver->read<uintptr_t>(SGInventoryCommonDataComponent + 0x138) + 8) + 0);
            long SGWeaponAssembleComponent = driver->read<uintptr_t>(CurrentWeapon + 0xa48);
            long CachedMagazine = driver->read<uintptr_t>(SGWeaponAssembleComponent + 0x270);
            long SGWeaponContainerComponent = driver->read<uintptr_t>(CachedMagazine + 0x898);
            long BulletContainerInfo = driver->read<uintptr_t>(SGWeaponContainerComponent + 0x1A8);
            int StackCount = driver->read<int>(BulletContainerInfo + 0x8);
            char HandheldStr[256];
            GetUTF8Text(HandheldStr, displayName);
            long SGCharacterArmorManagerComponent = driver->read<uintptr_t>(obj + 0x17b8);
            long ProtectiveArmorList = driver->read<uintptr_t>(SGCharacterArmorManagerComponent + 0x278);
            int 头 = driver->read<int>(driver->read<uintptr_t>(ProtectiveArmorList) + 0x6d0);
            int 甲 = driver->read<int>(driver->read<uintptr_t>(ProtectiveArmorList + 0x8) + 0x6d0);

            if (W > 0){
            if (DrawIo[2]) FgDraw->AddLine({ px,0 }, {Head.X ,Head.Y }, retColor, 1.5);
                if (DrawIo[3]) {
                    // Esqueletos deshabilitados porque los huesos están encriptados
                }
           if (NumIo[8]==1.0f) {
							Head2 = Head;
							} else if (NumIo[8]==2.0f) {
							Head2 = Chest;
							} else if (NumIo[8]==3.0f) {
							Head2 = Right_Knee;
							} else {
							Head2 = Chest;
							}     
          double distance = std::sqrt(std::pow(Head2.X - px, 2) + std::pow(Head2.Y - py, 2));
						if (distance < closestDistance && distance < NumIo[3]) {
							closestDistance = distance;
							Head2 = Head2;
							
						}
		    if (DrawIo[20])FgDraw->AddCircle({ px,py }, NumIo[3], ImColor(255, 255, 255, 255), 0, 1.5);
    if (Head2.X > 0 && Head2.Y > 0) {

			if (DrawIo[19])FgDraw->AddLine({ px,py }, { Head2.X, Head2.Y }, ImColor(255, 255, 255, 255), 1.5);
            if (DrawIo[20])
			{
				if (isFiring || isFiring2) {
					AimBotAuto(Head2);
					//bool isDown = true;
				/*} else {
				Target.X = 0;
            	Target.Y = 0;
            	TouchX = TouchPosx;
    			TouchY = TouchPosy;
            	if (isDown) {
                	Touch::Up();
               		isDown = false;				
            	}
				
			}*/} else {
			Touch_Up();
			}
				}
			

			
				
			}
			if (DrawIo[21])
    {	
        FgDraw->AddCircleFilled( {TouchPosx, TouchPosy},100.0f,ImColor(255, 157, 0),0);
        
    }  
          if (!是否人机){
           if (DrawIo[4]){
			信息数量++;
			std::string a;
			a += "T";
			a += std::to_string(TeamIndex);
			a += ".";
			a += PlayerNamePrivate;
			auto textSize = ImGui::CalcTextSize(a.c_str(), 0, 25);
            DrawTextStroke(30, X + (W / 2) - (textSize.x / 2) + 15,TOP - 60, retColor, a.c_str());
		}
		if (DrawIo[6]){
			std::string HandheldText = "";
			HandheldText += HandheldStr;
			HandheldText += "[";
			HandheldText += std::to_string(StackCount);
			HandheldText += "]";
			auto textSize = ImGui::CalcTextSize(HandheldText.c_str(), 0, 25);
                         
            DrawTextStroke(30, X + (W / 2) - (textSize.x / 2) + 15, Y + 20, Green, HandheldText.c_str());
		}
		if (DrawIo[7]){
			std::string a = "H:";
			a += std::to_string(头);
			a += "| A :";
			a += std::to_string(甲);
		    auto textSize = ImGui::CalcTextSize(a.c_str(), 0, 25);
            DrawTextStroke(30, X + (W / 2) - (textSize.x / 2) + 15, Y - 40, White, a.c_str());
		}
	}
		if (DrawIo[5]){
		    std::string a = "";
		    if (热成像判断 == 1) {
             a += " <T7 Thermal Imaging>\n";
             热成像判断 = 0;
            }
			int 真实距离 = 距离/100;
		    a += std::to_string(真实距离);
		    a += "M";
			auto textSize = ImGui::CalcTextSize(a.c_str(), 0, 25);
                        if(!是否人机){
            DrawTextStroke(30, X + (W / 2) - (textSize.x / 2) + 15, Y - 110, retColor, a.c_str());
            } else{
            DrawTextStroke(30, X + (W / 2) - (textSize.x / 2) + 15, Y - 60, retColor, a.c_str());
            }
		}
				
		if (DrawIo[8]){
		
        FgDraw->AddRect({NumIo[1] - 200, NumIo[2] - 200}, {NumIo[1] + 200, NumIo[2] + 200}, ImColor(ImVec4(255/255.f, 255/255.f, 258/255.f, 1.0f)));
        FgDraw->AddLine({NumIo[1] + 200, NumIo[2]}, {NumIo[1] - 200, NumIo[2]}, ImColor(ImVec4(255/255.f, 255/255.f, 258/255.f, 0.5f)), 2);
        FgDraw->AddLine({NumIo[1], NumIo[2] + 200}, {NumIo[1], NumIo[2] - 200}, ImColor(ImVec4(255/255.f, 255/255.f, 258/255.f, 0.5f)), 2);
        FgDraw->AddCircleFilled({NumIo[1] + Radar.X, NumIo[2] + Radar.Y}, 8, retColor);
		}
		if (DrawIo[9]){
            float maxY = Feet.Y;
            if (maxY < Head.Y || maxY > Head.Y + 2000.f) {
                maxY = Head.Y + 200.0f; // Fallback
            }
            float boxHeight = abs(Head.Y - maxY);
            float boxWidth = boxHeight * 0.5f;
            float boxTop = Head.Y - (boxHeight * 0.1f);
            float boxLeft = Head.X - (boxWidth / 2.0f);
            FgDraw->AddRect({boxLeft, boxTop}, {boxLeft + boxWidth, boxTop + boxHeight * 1.1f}, retColor, 0, 0, 2.0f);
		}

                
      		}
		}
		else {
		// Loot / items
			Vector2A screenItem = WorldToScreen(Pos, camera);
			float X = screenItem.X;
			float Y = screenItem.Y;
			float W = 1.0f; // dummy, items usan X/Y directamente
			int 距离 = getDistance(MyPos, Pos);
         	Vector2A 绘制点;
         	ImColor retColor;
				
				if (DrawIo[22])
					{
							short 是否损坏 = driver->read<short>(obj + 0x69);//bool bNetLoadOnClient;
						if (是否损坏 == 1039 || 是否损坏 == 1037) continue;

						long int SGInventoryCommonDataComponent = driver->read<uint64_t>(obj + 0x858); //Class:BP_ItemBase_C.SGInventory.Actor.Object - 》 SGInventoryCommonDataComponent * SGInventoryCommonData
						long int DisplayName = driver->read<uint64_t>(driver->read<uint64_t>(driver->read<uint64_t>(SGInventoryCommonDataComponent + 0x138) + 8) + 0);// FText DisplayName
						char ItemNameKK[64];
						GetUTF8Text(ItemNameKK, DisplayName);

						if (strcmp(ItemNameKK, "") == 0)
						{
							SGInventoryCommonDataComponent = driver->read<uint64_t>(obj + 0x860); //Class:BP_ItemBase_C.SGInventory.Actor.Object - 》 SGInventoryCommonDataComponent * SGInventoryCommonData
							DisplayName = driver->read<uint64_t>(driver->read<uint64_t>(driver->read<uint64_t>(SGInventoryCommonDataComponent + 0x138) + 8) + 0);// FText DisplayName
							GetUTF8Text(ItemNameKK, DisplayName);
						}
						if (strcmp(ItemNameKK, "") == 0)
						{
							SGInventoryCommonDataComponent = driver->read<uint64_t>(obj + 0x870); //Class:BP_ItemBase_C.SGInventory.Actor.Object - 》 SGInventoryCommonDataComponent * SGInventoryCommonData
							DisplayName = driver->read<uint64_t>(driver->read<uint64_t>(driver->read<uint64_t>(SGInventoryCommonDataComponent + 0x138) + 8) + 0);// FText DisplayName
							GetUTF8Text(ItemNameKK, DisplayName);
						}
						if (strcmp(ItemNameKK, "") == 0)
						{
							SGInventoryCommonDataComponent = driver->read<uint64_t>(obj + 0x868); //Class:BP_ItemBase_C.SGInventory.Actor.Object - 》 SGInventoryCommonDataComponent * SGInventoryCommonData
							DisplayName = driver->read<uint64_t>(driver->read<uint64_t>(driver->read<uint64_t>(SGInventoryCommonDataComponent + 0x138) + 8) + 0);// FText DisplayName
							GetUTF8Text(ItemNameKK, DisplayName);
						}
						if (strcmp(ItemNameKK, "") == 0)
						{
							SGInventoryCommonDataComponent = driver->read<uint64_t>(obj + 0x878); //Class:BP_ItemBase_C.SGInventory.Actor.Object - 》 SGInventoryCommonDataComponent * SGInventoryCommonData
							DisplayName = driver->read<uint64_t>(driver->read<uint64_t>(driver->read<uint64_t>(SGInventoryCommonDataComponent + 0x138) + 8) + 0);// FText DisplayName
							GetUTF8Text(ItemNameKK, DisplayName);
						}
						if (strcmp(ItemNameKK, "") == 0)
						{
							SGInventoryCommonDataComponent = driver->read<uint64_t>(obj + 0x880); //Class:BP_ItemBase_C.SGInventory.Actor.Object - 》 SGInventoryCommonDataComponent * SGInventoryCommonData
							DisplayName = driver->read<uint64_t>(driver->read<uint64_t>(driver->read<uint64_t>(SGInventoryCommonDataComponent + 0x138) + 8) + 0);// FText DisplayName
							GetUTF8Text(ItemNameKK, DisplayName);
						}
						if (strcmp(ItemNameKK, "") == 0)
						{
							SGInventoryCommonDataComponent = driver->read<uint64_t>(obj + 0x700); //Class:BP_ItemBase_C.SGInventory.Actor.Object - 》 SGInventoryCommonDataComponent * SGInventoryCommonData
							DisplayName = driver->read<uint64_t>(driver->read<uint64_t>(driver->read<uint64_t>(SGInventoryCommonDataComponent + 0x138) + 8) + 0);// FText DisplayName
							GetUTF8Text(ItemNameKK, DisplayName);
						}
						if (strcmp(ItemNameKK, "") == 0)
						{
							SGInventoryCommonDataComponent = driver->read<uint64_t>(obj + 0x708); //Class:BP_ItemBase_C.SGInventory.Actor.Object - 》 SGInventoryCommonDataComponent * SGInventoryCommonData
							DisplayName = driver->read<uint64_t>(driver->read<uint64_t>(driver->read<uint64_t>(SGInventoryCommonDataComponent + 0x138) + 8) + 0);// FText DisplayName
							GetUTF8Text(ItemNameKK, DisplayName);
						}

						if (strcmp(ItemNameKK, "") == 0) continue;
						int 物品价值 = driver->read<int>(SGInventoryCommonDataComponent + 0xf0); //int StandardPrice
						int 稀有度 = driver->read<int>(SGInventoryCommonDataComponent + 0xf4); //int rarity
						int TotalCount = driver->read<int>(SGInventoryCommonDataComponent + 0xEC); //int TotalCount
						物品价值 = 物品价值 * TotalCount;
						if (物品价值 <= NumIo[5])continue;
						retColor = LevelColor3;

						if (物品价值 <= 0)continue;
						if (物品价值 == 1)continue;
						if (物品价值 > 80000000)continue;
						string a = "";
						a += ItemNameKK;
						if (DrawIo[23])
						{
							a += "(Value:";
							a += to_string(物品价值);
							a += ")";
						}

						if (DrawIo[24])
						{
							
			int 真实距离 = 距离/100;
		    a += std::to_string(真实距离);
							a += "M";
						}

						if (稀有度 == 1)
						{
							retColor = LevelColor1;
						}
						if (稀有度 == 4)
						{
							retColor = LevelColor2;
						}
						if (稀有度 == 5)
						{
							retColor = LevelColor3;
						}
                           if (screenItem.X <= 0 && screenItem.Y <= 0)
                            continue;
                            绘制点.X = X;
                            绘制点.Y = Y;
   
					
						// 在绘制文本前，先检查该位置是否已经被绘制过
						if (drawn_positions.find(绘制点) == drawn_positions.end()) {
							// 没有找到该位置，初始化对应的偏移量为0
							drawn_positions[绘制点] = 0;
						}
						else {
							// 找到该位置，偏移量加1
							drawn_positions[绘制点]++;
						}

						// 根据偏移量调整位置
						Vector2A actual_position = 绘制点;
						actual_position.Y += drawn_positions[绘制点] * 23;  // OFFSET is the vertical distance between texts

						DrawTextStroke(32, actual_position.X, actual_position.Y, retColor, a.c_str());
					}

				
		
		}
	}
    skip_actor_draw:
    std::string str1;
	str1 += " PLAYERs:";
    str1 += std::to_string((int)真人数量);   
	str1 += "|BOTs:";
	str1 += std::to_string((int)人机数量); 
	str1 += "\n    Remain People:";
    str1 += std::to_string((int)HHCount);   
    auto textSize = ImGui::CalcTextSize(str1.c_str(), 0, 25);
    DrawTextStroke(56, px + 3 - (textSize.x / 2) + 15, 85, White, str1.c_str());           int pid = getpid(); // 获取当前进程ID
    
    
    std::string str11;
    str11 += "UAMO: LINX ArenaBrekout";
	str11 += "  FPS:";
    str11 += std::to_string((int)ImGui::GetIO().Framerate);   
    str11 += "  Self PID:";
    str11 += std::to_string((int)pid);   
    str11 += "  World:";
    str11 += std::to_string(Uworld);   
    str11 += "  Count:";
    str11 += std::to_string((int)Count);   
    str11 += "  state:";
    str11 += std::to_string(dt);   
    
    DrawTextStroke(36,180, 25, Green, str11.c_str());         
    if (DrawIo[10]){
    Draw->AddLine({(float)px+25, (float)py}, {(float)px-25,(float)py}, Green, 2);
    Draw->AddLine({(float)px, (float)py+25}, {(float)px, (float)py-25}, Green, 2);
    }
    }
    


