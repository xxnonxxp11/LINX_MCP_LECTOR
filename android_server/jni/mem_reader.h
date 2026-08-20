#ifndef MEM_READER_H
#define MEM_READER_H

#include <stdint.h>
#include <unistd.h>
#include <string>
#include <vector>
#include "protocol.h"

struct UE4CameraInfo {
    float x, y, z;              // Camera Location
    float pitch, yaw, roll;     // Camera Rotation
    float fov;                  // Field of view
    uintptr_t local_player;     // ULocalPlayer pointer
    uintptr_t player_controller;// APlayerController pointer
    uintptr_t local_pawn;       // AcknowledgedPawn pointer (Your player)
    float local_x, local_y, local_z; // Local player position
    int32_t local_team_id;      // Local player team
    bool valid;
};

struct UE4ActorInfo {
    uintptr_t address;
    uint32_t name_id;
    std::string name;
    std::string class_name;
    float x, y, z;
    float head_x, head_y, head_z;
    uintptr_t root_component;
    uintptr_t player_state;
    std::string player_name;
    int32_t team_id;
    float distance;             // Distance in meters from local player
    bool is_local_player;
    bool is_teammate;
    bool is_bot;
    bool is_player;
    bool is_item;
};

struct UE4DynamicOffsets {
    uint32_t persistent_level_offset = 0x30;   // UWorld -> ULevel fallback
    uint32_t actors_offset           = 0x98;   // ULevel -> Actors TArray
    uint32_t root_component_offset   = 0x158;  // ACharacter -> RootComponent (CollisionCylinder)
    uint32_t mesh_offset             = 0x370;  // ACharacter -> CharacterMesh0
    uint32_t camera_manager_offset   = 0x1100; // PlayerCameraManager -> CameraCache (POV)
    uint32_t player_state_offset     = 0x310;  // ACharacter -> PlayerState
    uint32_t bone_array_offset       = 0x4c0;  // USkeletalMeshComponent -> BoneSpaceTransforms
    uint32_t component_to_world_off  = 0x140;  // USceneComponent -> ComponentToWorld
    bool     auto_detect_pl          = true;
    bool     enable_bone_decrypt     = false;
};

struct FVector3D {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct FRotator3D {
    float pitch = 0.0f, yaw = 0.0f, roll = 0.0f;
};

struct UE4ActorField {
    uint32_t offset;
    std::string type;
    std::string name;
    std::string class_name;
    std::string value_str;
};

struct UE4ActorDiagnostic {
    uintptr_t actor_ptr;
    std::string name;
    std::string class_name;
    bool is_player;
    bool is_bot;
    bool is_teammate;
    
    // Components
    uintptr_t root_component;
    std::string root_comp_name;
    FVector3D root_location;
    FRotator3D root_rotation;
    
    uintptr_t mesh_component;
    std::string mesh_comp_name;
    uintptr_t skeletal_mesh;
    std::string skeletal_mesh_name;
    uintptr_t bone_transforms_ptr;
    int32_t bone_count;
    
    uintptr_t player_state;
    std::string player_state_name;
    std::string player_name;
    int32_t team_id;
    
    uintptr_t controller;
    std::string controller_name;
    
    std::vector<UE4ActorField> components;
};

struct ESPDrawConfig {
    bool box             = true;
    bool skeleton        = true;
    bool snapline        = true;
    bool name            = true;
    bool distance        = true;
    bool health          = true;
    bool weapon          = true;
    bool radar           = false;
    bool fov_circle      = true;
    bool ignore_bots     = false;
    bool loot            = true;
    bool loot_price      = true;
    float min_loot_price = 5000.0f;
    float fov_radius     = 120.0f;
};

struct UE4WorldSnapshot {
    UE4CameraInfo camera;
    std::vector<UE4ActorInfo> actors;
    uintptr_t uworld;
    uintptr_t persistent_level;
    UE4DynamicOffsets active_offsets;
    ESPDrawConfig draw_config;
};

// ─── ARM64 Hook Capture (Soft-Trampoline, no ptrace, no .so injection) ───────
// A HookEntry stores the original bytes replaced by our trampoline at hook_addr,
// and the capture_buf address where X0-X7 are written on every function call.
struct HookEntry {
    uintptr_t hook_addr;        // Address of the patched function prologue
    uint8_t   orig_bytes[16];   // Saved original 4 ARM64 instructions (16 bytes)
    uintptr_t capture_buf;      // Address in game heap/BSS where regs are written
    std::string label;          // Human-readable label (e.g. "TransformEncrypt")
};

// Result of a capture poll: the 8 argument registers X0-X7 snapshotted
struct HookCaptureResult {
    bool      valid;            // true = new capture since last poll
    uintptr_t hook_addr;
    std::string label;
    uint64_t  x[8];            // X0..X7 values written by trampoline
    std::string x_hex[8];      // Hex string representation
};

// ─── VM::TransformEncrypt Decryption ────────────────────────────────────────
// Arena Breakout encrypts every FTransform (bone matrix, ComponentToWorld)
// via a kernel VM function. We capture the public key with hook_capture,
// then store it here to decrypt raw bone data at read-time.
struct VMDecryptKey {
    bool     valid    = false;
    uint32_t key_len  = 0;
    uint8_t  key[256] = {};
    // Detected algorithm after known-plaintext or entropy analysis:
    enum class Algo { UNKNOWN, XOR_FIXED, XOR_NONCE, AES_ECB } algo = Algo::UNKNOWN;
    // Pointer to the public key buffer in game memory (X1 register value)
    uint64_t key_ptr_in_game = 0;
    std::string algo_name() const {
        switch (algo) {
            case Algo::XOR_FIXED: return "XOR_FIXED";
            case Algo::XOR_NONCE: return "XOR_NONCE";
            case Algo::AES_ECB:   return "AES_ECB";
            default:              return "UNKNOWN";
        }
    }
};

// Raw 48-byte FTransform as stored by UE4 (and encrypted by VM)
// Layout: Quaternion(X,Y,Z,W) 16 bytes + Translation(X,Y,Z,_) 16 bytes + Scale(X,Y,Z,_) 16 bytes
#pragma pack(push, 1)
struct FTransformRaw {
    float qx, qy, qz, qw;          // Quaternion rotation
    float tx, ty, tz, _pad1;       // Translation (world position)
    float sx, sy, sz, _pad2;       // Scale3D
};
#pragma pack(pop)
static_assert(sizeof(FTransformRaw) == 48, "FTransformRaw must be exactly 48 bytes");

// Result of decrypting and converting a single bone to world position
struct UE4BonePos {
    bool     valid = false;
    int      bone_idx = -1;
    float    wx, wy, wz;   // World-space position
    float    qx, qy, qz, qw; // Quaternion (for future aim prediction)
};

struct UE4Roots {
    uintptr_t lib_base;
    uintptr_t fname_pool;
    uintptr_t guobject_array;
    uintptr_t gworld;
};

class MemReader {
public:
    MemReader();
    ~MemReader();

    bool attach(int pid);
    bool attach_by_name(const std::string& process_or_pkg_name);
    void detach();

    bool is_attached() const { return target_pid > 0 && mem_fd >= 0; }
    int get_pid() const { return target_pid; }
    std::string get_process_name() const { return target_name; }

    bool read(uintptr_t address, void* buffer, size_t size);
    bool write(uintptr_t address, const void* buffer, size_t size);

    template<typename T>
    T read_val(uintptr_t address, T default_val = T()) {
        T val;
        if (read(address, &val, sizeof(T))) {
            return val;
        }
        return default_val;
    }

    template<typename T>
    bool write_val(uintptr_t address, const T& val) {
        return write(address, &val, sizeof(T));
    }

    uintptr_t read_ptr_chain(uintptr_t base, const std::vector<uintptr_t>& offsets, std::vector<uintptr_t>* out_chain = nullptr);
    std::string read_string(uintptr_t address, size_t max_len = 128);

    std::vector<ModuleInfo> get_modules();
    ModuleInfo get_module_by_name(const std::string& name);
    uintptr_t get_module_base(const std::string& name);

    std::vector<uintptr_t> pattern_scan(uintptr_t start_addr, uintptr_t end_addr, const std::string& pattern);
    std::vector<uintptr_t> pattern_scan_module(const std::string& module_name, const std::string& pattern);
    std::vector<uintptr_t> pattern_scan_all(const std::string& pattern, size_t max_matches = 100);

    // ─── UE4 Real-time Reflection & World Parsers ───────────────
    UE4Roots get_ue4_roots();
    std::string resolve_fname(uint32_t index, uintptr_t fname_pool = 0);
    uintptr_t get_uobject_ptr(uint32_t index, uintptr_t guobject_array = 0);
    std::string get_uobject_name(uintptr_t uobj, uintptr_t fname_pool = 0);
    std::string get_uobject_class_name(uintptr_t uobj, uintptr_t fname_pool = 0);
    std::vector<UE4ActorInfo> get_world_actors(uintptr_t gworld_addr = 0, size_t max_actors = 1024);
    uintptr_t find_valid_uworld();
    UE4WorldSnapshot get_world_snapshot(uintptr_t gworld_addr = 0, size_t max_actors = 1024);
    std::vector<UE4ActorInfo> scan_all_actors(size_t max_actors = 256);
    UE4ActorDiagnostic inspect_actor(uintptr_t actor_ptr);

    // ─── VM::TransformEncrypt Decryption ─────────────────────────
    // Stores the VM key captured via hook_capture/hook_poll for use in
    // all subsequent FTransform reads. Call after successful hook_poll.
    void set_decrypt_key(const VMDecryptKey& key);
    const VMDecryptKey& get_decrypt_key() const { return vm_decrypt_key; }
    void clear_decrypt_key() { vm_decrypt_key = VMDecryptKey{}; }

    // Decrypt a raw 48-byte FTransform cipher block using the stored key.
    // Returns a cleared (zeroed) transform if key is not valid.
    FTransformRaw decrypt_transform(const uint8_t* cipher_48) const;

    // Read and decrypt a single bone from a SkeletalMeshComponent's BoneArray.
    //   skel_mesh_comp: pointer to USkeletalMeshComponent
    //   bone_idx:       index into the BoneArray (e.g. 82 = head)
    //   mesh_comp_to_world_off: offset of ComponentToWorld in the component (default 0x250)
    UE4BonePos get_bone_world_pos(uintptr_t skel_mesh_comp, int bone_idx,
                                  uintptr_t bone_array_off   = 0x4A0,
                                  uintptr_t mesh_world_off   = 0x250);

    // Known-plaintext attack: given a cipher FTransform and its known plaintext
    // world position (obtained from RootComponent+0x154), tries to deduce the
    // XOR key stream. Stores the result in vm_decrypt_key if successful.
    // Returns true if a plausible key was found.
    bool known_plaintext_attack(const uint8_t* cipher_48, float known_tx, float known_ty, float known_tz);

    // ─── ARM64 Hook Capture (Soft-Trampoline) ────────────────────
    // Places a 16-byte trampoline at func_addr that writes X0-X7 to capture_buf
    // then jumps to a scratch stub that restores and continues execution.
    // No ptrace, no .so injection, fully stealth.
    bool hook_capture(uintptr_t func_addr, uintptr_t capture_buf, const std::string& label);
    bool hook_restore(uintptr_t func_addr);
    HookCaptureResult hook_poll(uintptr_t func_addr);   // Read captured registers
    std::vector<HookEntry> hook_list_all();             // List all active hooks

    // ─── Dynamic Live Offset Overrides ─────────────────────────
    void set_dynamic_offsets(const UE4DynamicOffsets& offsets) { dynamic_offsets = offsets; }
    const UE4DynamicOffsets& get_dynamic_offsets() const { return dynamic_offsets; }
    void reset_dynamic_offsets() { dynamic_offsets = UE4DynamicOffsets{}; }

    // ─── Dynamic Live Draw / ESP Configuration ───────────────────
    void set_draw_config(const ESPDrawConfig& config) { draw_config = config; }
    const ESPDrawConfig& get_draw_config() const { return draw_config; }
    void reset_draw_config() { draw_config = ESPDrawConfig{}; }

    static std::vector<ProcessInfo> list_processes();
    static int find_pid(const std::string& name);

private:
    int target_pid;
    int mem_fd;
    std::string target_name;
    UE4Roots ue4_roots;
    UE4DynamicOffsets dynamic_offsets;
    ESPDrawConfig draw_config;
    VMDecryptKey vm_decrypt_key;          // Active VM decryption key (from hook_capture)
    std::vector<HookEntry> hook_entries;  // Active hook trampolines

    bool open_proc_mem();
    void close_proc_mem();
    void scan_ue4_roots();
};

#endif // MEM_READER_H
