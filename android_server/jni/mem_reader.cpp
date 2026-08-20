#include "mem_reader.h"
#include "ue4_auto_scanner.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cmath>
#include <math.h>

MemReader::MemReader() : target_pid(-1), mem_fd(-1), target_name("") {
    memset(&ue4_roots, 0, sizeof(ue4_roots));
}

MemReader::~MemReader() {
    detach();
}

bool MemReader::open_proc_mem() {
    if (target_pid <= 0) return false;
    close_proc_mem();

    char mem_path[64];
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", target_pid);
    mem_fd = open(mem_path, O_RDWR);
    if (mem_fd < 0) {
        LOG_WARN("MEM", "Failed to open %s with O_RDWR (errno: %d, %s). Trying O_RDONLY...", mem_path, errno, strerror(errno));
        mem_fd = open(mem_path, O_RDONLY);
    }
    if (mem_fd < 0) {
        if (errno == EACCES || errno == EPERM) {
            LOG_ERROR("MEM", "[!] SELINUX_PERMISSION_DENIED: /proc/%d/mem bloqueado por SELinux (errno: %d, %s). UID actual: %d.", target_pid, errno, strerror(errno), getuid());
            printf("\n\033[1;31m[!] ERROR SELINUX: Permiso denegado al abrir /proc/%d/mem (errno: %d, %s)\033[0m\n", target_pid, errno, strerror(errno));
            printf("\033[1;33m[i] Tu gestor de Root (Magisk/KernelSU) requiere otorgar regla a ptrace.\033[0m\n\n");
        } else {
            LOG_ERROR("MEM", "CRITICAL: Cannot open %s (errno: %d, %s).", mem_path, errno, strerror(errno));
        }
    } else {
        LOG_INFO("MEM", "Successfully opened %s (fd=%d)", mem_path, mem_fd);
    }
    return mem_fd >= 0;
}

void MemReader::close_proc_mem() {
    if (mem_fd >= 0) {
        close(mem_fd);
        mem_fd = -1;
    }
}

bool MemReader::attach(int pid) {
    if (pid <= 0) return false;
    target_pid = pid;
    
    char cmdline_path[64];
    snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%d/cmdline", pid);
    FILE* f = fopen(cmdline_path, "r");
    if (f) {
        char buf[256] = {0};
        if (fgets(buf, sizeof(buf), f)) {
            target_name = buf;
        }
        fclose(f);
    }
    if (target_name.empty()) {
        target_name = "PID_" + std::to_string(pid);
    }

    LOG_INFO("ATTACH", "Attaching to target PID %d (%s)...", pid, target_name.c_str());
    bool res = open_proc_mem();
    if (res) {
        LOG_INFO("ATTACH", "Attached successfully. Scanning UE4 roots...");
        scan_ue4_roots();
    } else {
        LOG_ERROR("ATTACH", "Failed to attach to PID %d", pid);
    }
    return res;
}

bool MemReader::attach_by_name(const std::string& process_or_pkg_name) {
    int pid = find_pid(process_or_pkg_name);
    if (pid > 0) {
        return attach(pid);
    }
    return false;
}

void MemReader::detach() {
    close_proc_mem();
    target_pid = -1;
    target_name = "";
}

bool MemReader::read(uintptr_t address, void* buffer, size_t size) {
    if (target_pid <= 0 || !buffer || size == 0) return false;

    // Method 1: pread64 on /proc/$PID/mem
    if (mem_fd >= 0) {
        ssize_t bytes_read = pread64(mem_fd, buffer, size, (off64_t)address);
        if (bytes_read == (ssize_t)size) {
            return true;
        }
    }

    // Method 2: process_vm_readv syscall (direct kernel memory transfer)
    struct iovec local_iov;
    local_iov.iov_base = buffer;
    local_iov.iov_len = size;

    struct iovec remote_iov;
    remote_iov.iov_base = (void*)address;
    remote_iov.iov_len = size;

#if defined(__NR_process_vm_readv)
    ssize_t res = syscall(__NR_process_vm_readv, target_pid, &local_iov, 1, &remote_iov, 1, 0);
    return res == (ssize_t)size;
#else
    return false;
#endif
}

bool MemReader::write(uintptr_t address, const void* buffer, size_t size) {
    if (target_pid <= 0 || !buffer || size == 0) return false;

    // Method 1: pwrite64 on /proc/$PID/mem
    if (mem_fd >= 0) {
        ssize_t bytes_written = pwrite64(mem_fd, buffer, size, (off64_t)address);
        if (bytes_written == (ssize_t)size) {
            return true;
        }
    }

    // Method 2: process_vm_writev
    struct iovec local_iov;
    local_iov.iov_base = (void*)buffer;
    local_iov.iov_len = size;

    struct iovec remote_iov;
    remote_iov.iov_base = (void*)address;
    remote_iov.iov_len = size;

#if defined(__NR_process_vm_writev)
    ssize_t res = syscall(__NR_process_vm_writev, target_pid, &local_iov, 1, &remote_iov, 1, 0);
    return res == (ssize_t)size;
#else
    return false;
#endif
}

uintptr_t MemReader::read_ptr_chain(uintptr_t base, const std::vector<uintptr_t>& offsets, std::vector<uintptr_t>* out_chain) {
    uintptr_t current = base;
    if (out_chain) out_chain->push_back(current);

    for (size_t i = 0; i < offsets.size(); i++) {
        uintptr_t ptr_val = 0;
        if (!read(current, &ptr_val, sizeof(uintptr_t)) || ptr_val == 0) {
            return 0;
        }
        current = ptr_val + offsets[i];
        if (out_chain) out_chain->push_back(current);
    }
    return current;
}

std::string MemReader::read_string(uintptr_t address, size_t max_len) {
    if (max_len > 1024) max_len = 1024;
    std::vector<char> buf(max_len + 1, 0);
    if (read(address, buf.data(), max_len)) {
        buf[max_len] = '\0';
        return std::string(buf.data());
    }
    return "";
}

std::vector<ModuleInfo> MemReader::get_modules() {
    std::vector<ModuleInfo> modules;
    if (target_pid <= 0) return modules;

    char maps_path[64];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", target_pid);
    FILE* f = fopen(maps_path, "r");
    if (!f) return modules;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        uintptr_t start = 0, end = 0;
        char perms[16] = {0};
        char dev[16] = {0};
        char path[512] = {0};
        unsigned long offset = 0;
        int inode = 0;

        int fields = sscanf(line, "%lx-%lx %s %lx %s %d %s",
                            &start, &end, perms, &offset, dev, &inode, path);

        if (fields >= 7 && path[0] != '\0' && path[0] != '[') {
            // Extract module base name
            const char* slash = strrchr(path, '/');
            std::string mod_name = slash ? slash + 1 : path;

            // Check if we already have this module
            bool found = false;
            for (auto& mod : modules) {
                if (mod.path == path) {
                    if (end > mod.end_address) {
                        mod.end_address = end;
                        mod.size = mod.end_address - mod.base_address;
                    }
                    found = true;
                    break;
                }
            }

            if (!found) {
                ModuleInfo info;
                info.name = mod_name;
                info.path = path;
                info.base_address = start;
                info.end_address = end;
                info.size = end - start;
                info.permissions = perms;
                modules.push_back(info);
            }
        }
    }
    fclose(f);
    return modules;
}

ModuleInfo MemReader::get_module_by_name(const std::string& name) {
    auto modules = get_modules();
    for (const auto& mod : modules) {
        if (mod.name.find(name) != std::string::npos || mod.path.find(name) != std::string::npos) {
            return mod;
        }
    }
    ModuleInfo empty = {};
    return empty;
}

uintptr_t MemReader::get_module_base(const std::string& name) {
    ModuleInfo mod = get_module_by_name(name);
    return mod.base_address;
}

static void parse_pattern(const std::string& pattern, std::vector<uint8_t>& bytes, std::vector<bool>& mask) {
    std::istringstream iss(pattern);
    std::string byte_str;
    while (iss >> byte_str) {
        if (byte_str == "?" || byte_str == "??") {
            bytes.push_back(0);
            mask.push_back(false); // Wildcard
        } else {
            uint8_t b = (uint8_t)strtoul(byte_str.c_str(), nullptr, 16);
            bytes.push_back(b);
            mask.push_back(true);
        }
    }
}

std::vector<uintptr_t> MemReader::pattern_scan(uintptr_t start_addr, uintptr_t end_addr, const std::string& pattern) {
    std::vector<uintptr_t> matches;
    if (start_addr >= end_addr || pattern.empty()) return matches;

    std::vector<uint8_t> pat_bytes;
    std::vector<bool> pat_mask;
    parse_pattern(pattern, pat_bytes, pat_mask);
    if (pat_bytes.empty()) return matches;

    const size_t chunk_size = 64 * 1024; // 64 KB chunks
    std::vector<uint8_t> chunk(chunk_size);

    uintptr_t current = start_addr;
    while (current < end_addr) {
        size_t to_read = std::min((size_t)(end_addr - current), chunk_size);
        if (read(current, chunk.data(), to_read)) {
            size_t pat_len = pat_bytes.size();
            if (to_read >= pat_len) {
                for (size_t i = 0; i <= to_read - pat_len; i++) {
                    bool matched = true;
                    for (size_t j = 0; j < pat_len; j++) {
                        if (pat_mask[j] && chunk[i + j] != pat_bytes[j]) {
                            matched = false;
                            break;
                        }
                    }
                    if (matched) {
                        matches.push_back(current + i);
                    }
                }
            }
        }
        current += to_read;
    }
    return matches;
}

std::vector<uintptr_t> MemReader::pattern_scan_module(const std::string& module_name, const std::string& pattern) {
    ModuleInfo mod = get_module_by_name(module_name);
    if (mod.base_address == 0) return {};
    return pattern_scan(mod.base_address, mod.end_address, pattern);
}

std::vector<uintptr_t> MemReader::pattern_scan_all(const std::string& pattern, size_t max_matches) {
    std::vector<uintptr_t> matches;
    if (target_pid <= 0 || pattern.empty()) return matches;

    std::vector<uint8_t> pat_bytes;
    std::vector<bool> pat_mask;
    parse_pattern(pattern, pat_bytes, pat_mask);
    if (pat_bytes.empty()) return matches;

    char maps_path[64];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", target_pid);
    FILE* f = fopen(maps_path, "r");
    if (!f) return matches;

    char line[512];
    const size_t chunk_size = 64 * 1024;
    std::vector<uint8_t> chunk(chunk_size);

    while (fgets(line, sizeof(line), f)) {
        uintptr_t start = 0, end = 0;
        char perms[8] = {0};
        if (sscanf(line, "%lx-%lx %s", &start, &end, perms) < 3) continue;
        if (perms[0] != 'r') continue; // Only readable pages

        uintptr_t current = start;
        while (current < end) {
            size_t to_read = std::min((size_t)(end - current), chunk_size);
            if (read(current, chunk.data(), to_read)) {
                size_t pat_len = pat_bytes.size();
                if (to_read >= pat_len) {
                    for (size_t i = 0; i <= to_read - pat_len; i++) {
                        bool matched = true;
                        for (size_t j = 0; j < pat_len; j++) {
                            if (pat_mask[j] && chunk[i + j] != pat_bytes[j]) {
                                matched = false;
                                break;
                            }
                        }
                        if (matched) {
                            matches.push_back(current + i);
                            if (matches.size() >= max_matches) {
                                fclose(f);
                                return matches;
                            }
                        }
                    }
                }
            }
            current += to_read;
        }
    }
    fclose(f);
    return matches;
}

std::vector<ProcessInfo> MemReader::list_processes() {
    std::vector<ProcessInfo> list;
    DIR* dir = opendir("/proc");
    if (!dir) return list;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            int pid = atoi(entry->d_name);
            if (pid > 0) {
                char cmdline_path[64];
                snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%d/cmdline", pid);
                FILE* f = fopen(cmdline_path, "r");
                if (f) {
                    char cmd[512] = {0};
                    size_t read_bytes = fread(cmd, 1, sizeof(cmd) - 1, f);
                    fclose(f);
                    if (read_bytes > 0) {
                        // Replace null bytes with spaces
                        for (size_t i = 0; i < read_bytes; i++) {
                            if (cmd[i] == '\0') {
                                if (i + 1 < read_bytes && cmd[i + 1] != '\0') cmd[i] = ' ';
                                else cmd[i] = '\0';
                            }
                        }
                        // Trim trailing spaces
                        while (read_bytes > 0 && (cmd[read_bytes - 1] == ' ' || cmd[read_bytes - 1] == '\0')) {
                            cmd[--read_bytes] = '\0';
                        }
                        if (read_bytes > 0) {
                            ProcessInfo pinfo;
                            pinfo.pid = pid;
                            pinfo.cmdline = cmd;
                            
                            // Extract simple process name
                            const char* slash = strrchr(cmd, '/');
                            pinfo.name = slash ? slash + 1 : cmd;
                            size_t space_pos = pinfo.name.find(' ');
                            if (space_pos != std::string::npos) {
                                pinfo.name = pinfo.name.substr(0, space_pos);
                            }

                            list.push_back(pinfo);
                        }
                    }
                }
            }
        }
    }
    closedir(dir);
    return list;
}

int MemReader::find_pid(const std::string& name) {
    auto procs = list_processes();
    int best_pid = -1;

    for (const auto& p : procs) {
        if (p.cmdline == name || p.name == name) {
            return p.pid;
        }
        if (p.cmdline.find(name) != std::string::npos || p.name.find(name) != std::string::npos) {
            best_pid = p.pid;
        }
    }
    return best_pid;
}

// ─── UE4 Reflection & GWorld Real-time Parsers ──────────────────────────

void MemReader::scan_ue4_roots() {
    ue4_roots.lib_base = get_module_base("libUE4.so");
    if (ue4_roots.lib_base == 0) return;

    // First attempt: Dynamic AOB ADRP/ADD pattern scan
    UE4AutoScanResult auto_res = UE4AutoScanner::scan(*this);
    if (auto_res.fname_pool_found) {
        ue4_roots.fname_pool = auto_res.fname_pool;
    } else {
        ue4_roots.fname_pool = ue4_roots.lib_base + 0xEDE48C0ULL; // Fallback
    }

    if (auto_res.guobject_found) {
        ue4_roots.guobject_array = auto_res.guobject_array;
    } else {
        ue4_roots.guobject_array = ue4_roots.lib_base + 0xEE047F0ULL; // Fallback
    }

    if (auto_res.gworld_found) {
        ue4_roots.gworld = auto_res.gworld;
    } else {
        ue4_roots.gworld = ue4_roots.lib_base + 0xEE6E1D8ULL; // Fallback
    }
}

UE4Roots MemReader::get_ue4_roots() {
    if (ue4_roots.lib_base == 0) {
        scan_ue4_roots();
    }
    return ue4_roots;
}

std::string MemReader::resolve_fname(uint32_t index, uintptr_t fname_pool) {
    if (fname_pool == 0) {
        if (ue4_roots.fname_pool == 0) scan_ue4_roots();
        fname_pool = ue4_roots.fname_pool;
    }
    if (fname_pool == 0) return "None";

    // FName index layout: block (top 13 bits), offset (lower 16 bits * 2 stride)
    uint32_t block = index >> 16;
    uint32_t offset = (index & 0xFFFF) << 1;

    // In FNamePool: uint8_t* Blocks[8192] (offset +0x48: +0x00 Lock, +0x40 Block/Cursor, +0x48 Blocks)
    uintptr_t block_ptr = read_val<uintptr_t>(fname_pool + 0x48 + (block * sizeof(uintptr_t)));
    if (block_ptr == 0 || block_ptr < 0x100000000ULL) {
        block_ptr = read_val<uintptr_t>(fname_pool + 0x40 + (block * sizeof(uintptr_t)));
    }
    if (block_ptr == 0 || block_ptr < 0x100000000ULL) {
        block_ptr = read_val<uintptr_t>(fname_pool + 0x10 + (block * sizeof(uintptr_t)));
    }
    if (block_ptr == 0 || block_ptr < 0x100000000ULL) {
        block_ptr = read_val<uintptr_t>(fname_pool + 0x08 + (block * sizeof(uintptr_t)));
    }
    if (block_ptr == 0 || block_ptr < 0x100000000ULL) return "None";

    uintptr_t fname_entry = block_ptr + offset;
    uint16_t header = read_val<uint16_t>(fname_entry);
    int len = header >> 6;

    if (len > 0 && len < 256) {
        bool is_wide = (header & 1) != 0;
        if (!is_wide) {
            std::vector<char> buf(len + 1, 0);
            if (read(fname_entry + 2, buf.data(), len)) {
                buf[len] = '\0';
                return std::string(buf.data());
            }
        } else {
            std::vector<char16_t> wbuf(len + 1, 0);
            if (read(fname_entry + 2, wbuf.data(), len * 2)) {
                std::string ascii;
                for (int k = 0; k < len; k++) {
                    char c = (char)(wbuf[k] & 0x7F);
                    ascii.push_back(c ? c : '?');
                }
                return ascii;
            }
        }
    }
    return "None";
}

uintptr_t MemReader::get_uobject_ptr(uint32_t index, uintptr_t guobject_array) {
    if (guobject_array == 0) {
        if (ue4_roots.guobject_array == 0) scan_ue4_roots();
        guobject_array = ue4_roots.guobject_array;
    }
    if (guobject_array == 0) return 0;

    // In TUObjectArray: FUObjectItem** Objects (chunked array at +0x10 or +0x00)
    uintptr_t objects_ptr = read_val<uintptr_t>(guobject_array + 0x10);
    if (objects_ptr < 0x100000000ULL) {
        objects_ptr = read_val<uintptr_t>(guobject_array + 0x00);
    }
    if (objects_ptr == 0 || objects_ptr < 0x100000000ULL) return 0;

    uint32_t chunk_idx = index / 65536;
    uint32_t item_idx = index % 65536;

    uintptr_t chunk_ptr = read_val<uintptr_t>(objects_ptr + (chunk_idx * sizeof(uintptr_t)));
    if (chunk_ptr == 0 || chunk_ptr < 0x100000000ULL) return 0;

    // sizeof(FUObjectItem) == 24 in 64-bit UE4.25+
    uintptr_t fuobject_item = chunk_ptr + (item_idx * 24);
    uintptr_t uobj_ptr = read_val<uintptr_t>(fuobject_item + 0x00);
    return uobj_ptr;
}

std::string MemReader::get_uobject_name(uintptr_t uobj, uintptr_t fname_pool) {
    if (uobj == 0) return "None";
    // In UObject: NamePrivate is at offset 0x18
    uint32_t name_id = read_val<uint32_t>(uobj + 0x18);
    return resolve_fname(name_id, fname_pool);
}

std::string MemReader::get_uobject_class_name(uintptr_t uobj, uintptr_t fname_pool) {
    if (uobj == 0) return "None";
    // In UObject: ClassPrivate is at offset 0x10
    uintptr_t class_ptr = read_val<uintptr_t>(uobj + 0x10);
    if (class_ptr == 0) return "None";
    return get_uobject_name(class_ptr, fname_pool);
}

static std::string str_to_lower(const std::string& s) {
    std::string res = s;
    for (size_t i = 0; i < res.size(); i++) {
        res[i] = (char)tolower((unsigned char)res[i]);
    }
    return res;
}

uintptr_t MemReader::find_valid_uworld() {
    if (ue4_roots.guobject_array == 0) scan_ue4_roots();
    if (ue4_roots.guobject_array == 0) return 0;

    uintptr_t objects_ptr = read_val<uintptr_t>(ue4_roots.guobject_array + 0x10);
    if (objects_ptr < 0x100000000ULL) {
        objects_ptr = read_val<uintptr_t>(ue4_roots.guobject_array + 0x00);
    }
    if (objects_ptr == 0 || objects_ptr < 0x100000000ULL) return 0;

    for (uint32_t chunk_idx = 0; chunk_idx < 15; chunk_idx++) {
        uintptr_t chunk_ptr = read_val<uintptr_t>(objects_ptr + (chunk_idx * sizeof(uintptr_t)));
        if (chunk_ptr == 0 || chunk_ptr < 0x100000000ULL) continue;

        for (uint32_t item_idx = 0; item_idx < 65536; item_idx++) {
            uintptr_t obj = read_val<uintptr_t>(chunk_ptr + (item_idx * 24));
            if (obj == 0 || obj < 0x100000000ULL) continue;

            std::string class_name = get_uobject_class_name(obj, ue4_roots.fname_pool);
            std::string lower_cls = str_to_lower(class_name);
            if (lower_cls.find("world") == std::string::npos) continue;

            // Validar cadena GI -> LP -> PC -> PCM como hace el overlay
            uintptr_t ginst = read_val<uintptr_t>(obj + 0x180);
            if (ginst < 0x100000000ULL) continue;
            uintptr_t lp_arr = read_val<uintptr_t>(ginst + 0x38);
            if (lp_arr < 0x100000000ULL) continue;
            uintptr_t lp = read_val<uintptr_t>(lp_arr);
            if (lp < 0x100000000ULL) continue;
            uintptr_t pc = read_val<uintptr_t>(lp + 0x30);
            if (pc < 0x100000000ULL) continue;
            uintptr_t pcm = read_val<uintptr_t>(pc + 0x398);
            if (pcm < 0x100000000ULL) continue;

            // Validar que el PCM tenga una camara plausible (FOV + posicion)
            struct CamEntry2 { float Loc[3]; float Rot[3]; float FOV; };
            static const uintptr_t cam_offs[] = {
                0x1100, 0x1150, 0x1200, 0x1250, 0x1300, 0x1350, 0x1400, 0x1450,
                0x1500, 0x1550, 0x1600, 0x1650, 0x1700, 0x1750, 0x1800, 0x1850,
                0x1900, 0x1950, 0x1A00, 0x1A50, 0x1B00, 0x1B50, 0x1BC0, 0x1C00,
                0x1D00, 0x1D50, 0x1E00, 0x1ED0, 0x1F00, 0x1F50, 0x2000
            };
            for (uintptr_t off : cam_offs) {
                CamEntry2 ce = {0};
                if (!read(pcm + off, &ce, sizeof(ce))) continue;
                if (ce.FOV < 35.0f || ce.FOV > 145.0f) continue;
                float ax = ce.Loc[0] < 0 ? -ce.Loc[0] : ce.Loc[0];
                float ay = ce.Loc[1] < 0 ? -ce.Loc[1] : ce.Loc[1];
                if (ax < 50.0f && ay < 50.0f) continue;
                if (ax > 5000000.0f || ay > 5000000.0f) continue;
                return obj;
            }
        }
    }
    return 0;
}

UE4WorldSnapshot MemReader::get_world_snapshot(uintptr_t gworld_addr, size_t max_actors) {
    UE4WorldSnapshot snapshot = {};
    snapshot.camera.valid = false;
    snapshot.camera.fov = 90.0f;

    if (gworld_addr == 0) {
        if (ue4_roots.gworld == 0) scan_ue4_roots();
        gworld_addr = ue4_roots.gworld;
    }
    if (gworld_addr == 0) return snapshot;

    // Dereference GWorld
    uintptr_t uworld = read_val<uintptr_t>(gworld_addr);
    if (uworld == 0 || uworld < 0x100000000ULL) {
        uintptr_t scan_world = find_valid_uworld();
        if (scan_world > 0x100000000ULL) uworld = scan_world;
    }
    if (uworld == 0 || uworld < 0x100000000ULL) return snapshot;
    snapshot.uworld = uworld;

    // Validar que la cadena GI->LP->PC->PCM del UWorld estatico realmente resuelve.
    // Si no, el puntero estatico esta roto -> escanear un UWorld real.
    uintptr_t test_ginst = read_val<uintptr_t>(uworld + 0x180);
    uintptr_t test_lp_arr = read_val<uintptr_t>(test_ginst + 0x38);
    uintptr_t test_lp = read_val<uintptr_t>(test_lp_arr);
    uintptr_t test_pc = read_val<uintptr_t>(test_lp + 0x30);
    uintptr_t test_pcm = read_val<uintptr_t>(test_pc + 0x398);
    if (test_pcm < 0x100000000ULL) {
        uintptr_t scan_world = find_valid_uworld();
        if (scan_world > 0x100000000ULL && scan_world != uworld) {
            uworld = scan_world;
            snapshot.uworld = uworld;
        }
    }

    // ── 1. Resolve LocalPlayer -> PlayerController -> Camera ────────────────
    uintptr_t gi_candidates[] = {
        read_val<uintptr_t>(uworld + 0x180),
        read_val<uintptr_t>(uworld + 0x1A0),
        read_val<uintptr_t>(uworld + 0x140),
        read_val<uintptr_t>(uworld + 0x118),
        read_val<uintptr_t>(uworld + 0x38),
        read_val<uintptr_t>(uworld + 0x40),
        read_val<uintptr_t>(uworld + 0x70)
    };

    uintptr_t game_instance = 0;
    for (uintptr_t cand : gi_candidates) {
        if (cand > 0x100000000ULL && cand != uworld) {
            uintptr_t lp_arr = read_val<uintptr_t>(cand + 0x38);
            int32_t lp_cnt = read_val<int32_t>(cand + 0x40);
            if (lp_arr > 0x100000000ULL && lp_cnt > 0 && lp_cnt <= 8) {
                game_instance = cand;
                break;
            }
            lp_arr = read_val<uintptr_t>(cand + 0x40);
            lp_cnt = read_val<int32_t>(cand + 0x48);
            if (lp_arr > 0x100000000ULL && lp_cnt > 0 && lp_cnt <= 8) {
                game_instance = cand;
                break;
            }
        }
    }

    if (game_instance > 0x100000000ULL) {
        uintptr_t lp_data = read_val<uintptr_t>(game_instance + 0x38);
        if (lp_data < 0x100000000ULL) lp_data = read_val<uintptr_t>(game_instance + 0x40);

        if (lp_data > 0x100000000ULL) {
            uintptr_t local_player = read_val<uintptr_t>(lp_data);
            if (local_player > 0x100000000ULL) {
                snapshot.camera.local_player = local_player;
                uintptr_t pc = read_val<uintptr_t>(local_player + 0x30);
                if (pc < 0x100000000ULL) pc = read_val<uintptr_t>(local_player + 0x38);

                if (pc > 0x100000000ULL) {
                    snapshot.camera.player_controller = pc;

                    // AcknowledgedPawn (Local Player Pawn)
                    uintptr_t pawn_cands[] = {
                        read_val<uintptr_t>(pc + 0x2B8),
                        read_val<uintptr_t>(pc + 0x2A0),
                        read_val<uintptr_t>(pc + 0x2C0),
                        read_val<uintptr_t>(pc + 0x260),
                        read_val<uintptr_t>(pc + 0x268),
                        read_val<uintptr_t>(pc + 0x280)
                    };
                    for (uintptr_t p : pawn_cands) {
                        if (p > 0x100000000ULL) {
                            snapshot.camera.local_pawn = p;
                            break;
                        }
                    }

                    if (snapshot.camera.local_pawn > 0x100000000ULL) {
                        uintptr_t root = read_val<uintptr_t>(snapshot.camera.local_pawn + 0x138);
                        if (root < 0x100000000ULL) root = read_val<uintptr_t>(snapshot.camera.local_pawn + 0x130);
                        if (root < 0x100000000ULL) root = read_val<uintptr_t>(snapshot.camera.local_pawn + 0x158);
                        if (root > 0x100000000ULL) {
                            float ppos[3] = {0};
                            if (read(root + 0x154, ppos, sizeof(ppos)) || read(root + 0x11C, ppos, sizeof(ppos))) {
                                snapshot.camera.local_x = ppos[0];
                                snapshot.camera.local_y = ppos[1];
                                snapshot.camera.local_z = ppos[2];
                            }
                        }
                        // Local PlayerState -> TeamId
                        uintptr_t ps = read_val<uintptr_t>(snapshot.camera.local_pawn + 0x240);
                        if (ps < 0x100000000ULL) ps = read_val<uintptr_t>(snapshot.camera.local_pawn + 0x248);
                        if (ps < 0x100000000ULL) ps = read_val<uintptr_t>(snapshot.camera.local_pawn + 0x238);
                        if (ps > 0x100000000ULL) {
                            snapshot.camera.local_team_id = read_val<int32_t>(ps + 0x228);
                            if (snapshot.camera.local_team_id == 0) snapshot.camera.local_team_id = read_val<int32_t>(ps + 0x230);
                        }
                    }

                    // PlayerCameraManager -> CameraCachePrivate
                    uintptr_t cam_mgr = read_val<uintptr_t>(pc + 0x398);
                    if (cam_mgr < 0x100000000ULL) cam_mgr = read_val<uintptr_t>(pc + 0x2D8);
                    if (cam_mgr < 0x100000000ULL) cam_mgr = read_val<uintptr_t>(pc + 0x2C0);
                    if (cam_mgr < 0x100000000ULL) cam_mgr = read_val<uintptr_t>(pc + 0x2E0);
                    if (cam_mgr > 0x100000000ULL) {
                        struct CamEntry {
                            float Loc[3];
                            float Rot[3];
                            float FOV;
                        };
                        uintptr_t cam_offsets[] = { 
                            (uintptr_t)dynamic_offsets.camera_manager_offset, // 0x1100 verified for AB Lite
                            0x1100, 0x1FE0, 0x1BC0, 0x1B80, 0x1AE0, 0x1F00, 0x2000, 0x1A00, 0x1C00, 0x0EE4, 0x0824
                        };
                        for (uintptr_t coff : cam_offsets) {
                            if (coff == 0) continue;
                            CamEntry ce = {0};
                            if (read(cam_mgr + coff, &ce, sizeof(ce))) {
                                if (ce.FOV >= 35.0f && ce.FOV <= 145.0f && (ce.Loc[0] != 0.0f || ce.Loc[1] != 0.0f)) {
                                    snapshot.camera.x = ce.Loc[0];
                                    snapshot.camera.y = ce.Loc[1];
                                    snapshot.camera.z = ce.Loc[2];
                                    snapshot.camera.pitch = ce.Rot[0];
                                    snapshot.camera.yaw = ce.Rot[1];
                                    snapshot.camera.roll = ce.Rot[2];
                                    snapshot.camera.fov = ce.FOV;
                                    snapshot.camera.valid = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── 2. Resolve PersistentLevel ──────────────────────────────────────────
    uintptr_t persistent_level = 0;

    // If a specific PL offset is configured, check it first
    if (dynamic_offsets.persistent_level_offset != 0) {
        uintptr_t cand = read_val<uintptr_t>(uworld + dynamic_offsets.persistent_level_offset);
        if (cand > 0x100000000ULL && (cand & 3) == 0) {
            uintptr_t vtbl = read_val<uintptr_t>(cand);
            if (vtbl > 0x100000000ULL && vtbl != cand) {
                persistent_level = cand;
            }
        }
    }

    if (persistent_level == 0) {
        uintptr_t pl_candidates[] = {
            read_val<uintptr_t>(uworld + 0x30),
            read_val<uintptr_t>(uworld + 0x90),
            read_val<uintptr_t>(uworld + 0x98),
            read_val<uintptr_t>(uworld + 0x20),
            read_val<uintptr_t>(uworld + 0x38),
            read_val<uintptr_t>(uworld + 0x70),
            read_val<uintptr_t>(uworld + 0x28),
            read_val<uintptr_t>(uworld + 0x40),
            read_val<uintptr_t>(uworld + 0x48),
            read_val<uintptr_t>(uworld + 0x50)
        };

        for (uintptr_t cand : pl_candidates) {
            if (cand < 0x100000000ULL) continue;
            if ((cand & 3) != 0) continue;
            uintptr_t vtbl = read_val<uintptr_t>(cand);
            if (vtbl < 0x100000000ULL || vtbl == cand) continue;

            // Validate that cand is indeed a ULevel: check if FName contains Level
            uint32_t fname_idx = read_val<uint32_t>(cand + 0x18);
            if (fname_idx > 0 && ue4_roots.fname_pool > 0) {
                std::string fname_str = resolve_fname(fname_idx, ue4_roots.fname_pool);
                std::string lower_fn = str_to_lower(fname_str);
                if (lower_fn.find("level") != std::string::npos || lower_fn.find("world") != std::string::npos) {
                    persistent_level = cand;
                    break;
                }
            } else {
                persistent_level = cand;
                break;
            }
        }
    }

    if (persistent_level == 0) return snapshot;
    snapshot.persistent_level = persistent_level;
    snapshot.active_offsets = dynamic_offsets;

    // ── 3. Read AActors TArray from PersistentLevel ─────────────────────────
    uintptr_t actors_data = 0;
    int32_t actors_count = 0;

    // Try configured actors_offset first, then fallback offsets
    std::vector<std::pair<uintptr_t, uintptr_t>> actor_offsets = {
        { (uintptr_t)dynamic_offsets.actors_offset, (uintptr_t)dynamic_offsets.actors_offset + 8 },
        { 0x98, 0xA0 }, { 0xA0, 0xA8 }, { 0x70, 0x78 }, { 0xB0, 0xB8 },
        { 0x88, 0x90 }, { 0x80, 0x88 }, { 0xC0, 0xC8 }
    };
    for (const auto& ao : actor_offsets) {
        uintptr_t d = read_val<uintptr_t>(persistent_level + ao.first);
        int32_t   c = read_val<int32_t>  (persistent_level + ao.second);
        if (d > 0x100000000ULL && c > 0 && c <= 4096) {
            uintptr_t first = read_val<uintptr_t>(d);
            if (first > 0x100000000ULL) {
                actors_data  = d;
                actors_count = c;
                break;
            }
        }
    }

    float ref_x = snapshot.camera.local_x;
    float ref_y = snapshot.camera.local_y;
    float ref_z = snapshot.camera.local_z;
    if (ref_x == 0 && ref_y == 0 && snapshot.camera.valid) {
        ref_x = snapshot.camera.x;
        ref_y = snapshot.camera.y;
        ref_z = snapshot.camera.z;
    }

    if (actors_data > 0x100000000ULL && actors_count > 0) {
        if (actors_count > (int32_t)max_actors) actors_count = (int32_t)max_actors;
        snapshot.actors.reserve(actors_count);

        for (int32_t i = 0; i < actors_count; i++) {
            uintptr_t actor_ptr = read_val<uintptr_t>(actors_data + (i * sizeof(uintptr_t)));
            if (actor_ptr > 0x100000000ULL) {
                uint32_t name_id = read_val<uint32_t>(actor_ptr + 0x18);
                std::string name = resolve_fname(name_id, ue4_roots.fname_pool);
                std::string class_name = get_uobject_class_name(actor_ptr, ue4_roots.fname_pool);

                UE4ActorInfo info = {};
                info.address = actor_ptr;
                info.name_id = name_id;
                info.name = name;
                info.class_name = class_name;
                info.team_id = 0;
                info.is_local_player = (actor_ptr == snapshot.camera.local_pawn);

                // RootComponent & Position (AB Lite: RootComponent at Actor+0x158 / 0x138 / 0x198)
                uintptr_t root_comp = 0;
                uintptr_t rc_cands[] = {
                    (uintptr_t)dynamic_offsets.root_component_offset,
                    0x158, 0x138, 0x130, 0x198, 0x1A8, 0x1B0
                };
                for (uintptr_t rco : rc_cands) {
                    if (rco == 0) continue;
                    uintptr_t cand = read_val<uintptr_t>(actor_ptr + rco);
                    if (cand > 0x100000000ULL && (cand & 3) == 0) {
                        uintptr_t vtbl = read_val<uintptr_t>(cand);
                        if (vtbl > 0x100000000ULL && vtbl != cand) {
                            root_comp = cand;
                            break;
                        }
                    }
                }

                if (root_comp > 0x100000000ULL) {
                    info.root_component = root_comp;
                    float pos[3] = {0};
                    // 0x154 (RelativeLocation) es la fuente PROBADA en runtime (LINX V5):
                    // solo ComponentToWorld (+0x140) pasa por VM::TransformEncrypt.
                    uintptr_t pos_offsets[] = {
                        0x154,
                        (uintptr_t)dynamic_offsets.component_to_world_off,
                        0x140, 0x148, 0x150, 0x11C, 0x120, 0x128, 0x130
                    };
                    for (uintptr_t po : pos_offsets) {
                        if (po == 0) continue;
                        if (read(root_comp + po, pos, sizeof(pos))) {
                            if (std::isfinite(pos[0]) && std::isfinite(pos[1]) && std::isfinite(pos[2]) &&
                                (std::abs(pos[0]) > 10.0f || std::abs(pos[1]) > 10.0f)) {
                                info.x = pos[0];
                                info.y = pos[1];
                                info.z = pos[2];
                                break;
                            }
                        }
                    }
                }

                // Classification
                std::string lower_name = str_to_lower(name);
                std::string lower_class = str_to_lower(class_name);

                info.is_bot = (lower_name.find("bot") != std::string::npos ||
                               lower_name.find("ai") != std::string::npos ||
                               lower_class.find("bot") != std::string::npos ||
                               lower_class.find("ai") != std::string::npos);

                info.is_player = (lower_name.find("character") != std::string::npos ||
                                  lower_name.find("pawn") != std::string::npos ||
                                  lower_name.find("player") != std::string::npos ||
                                  lower_class.find("character") != std::string::npos ||
                                  lower_class.find("player") != std::string::npos);

                info.is_item = (lower_name.find("item") != std::string::npos ||
                                lower_name.find("weapon") != std::string::npos ||
                                lower_name.find("loot") != std::string::npos ||
                                lower_name.find("pickup") != std::string::npos ||
                                lower_name.find("crate") != std::string::npos ||
                                lower_name.find("box") != std::string::npos ||
                                lower_name.find("container") != std::string::npos ||
                                lower_name.find("drop") != std::string::npos ||
                                lower_name.find("armor") != std::string::npos);

                // PlayerState & Team
                if (info.is_player || info.is_bot) {
                    uintptr_t ps = read_val<uintptr_t>(actor_ptr + 0x240);
                    if (ps < 0x100000000ULL) ps = read_val<uintptr_t>(actor_ptr + 0x248);
                    if (ps < 0x100000000ULL) ps = read_val<uintptr_t>(actor_ptr + 0x238);

                    if (ps > 0x100000000ULL) {
                        info.player_state = ps;
                        info.team_id = read_val<int32_t>(ps + 0x228);
                        if (info.team_id == 0) info.team_id = read_val<int32_t>(ps + 0x230);

                        if (info.team_id != 0 && snapshot.camera.local_team_id != 0 && info.team_id == snapshot.camera.local_team_id && !info.is_local_player) {
                            info.is_teammate = true;
                        }

                        // Try reading PlayerName
                        uintptr_t name_ptr = read_val<uintptr_t>(ps + 0x280);
                        if (name_ptr > 0x100000000ULL) {
                            info.player_name = read_string(name_ptr, 32);
                        }
                    }
                }

                // Head position estimation
                info.head_x = info.x;
                info.head_y = info.y;
                info.head_z = info.z + 75.0f;

                // Distance calculation
                if (ref_x != 0 || ref_y != 0 || ref_z != 0) {
                    float dx = info.x - ref_x;
                    float dy = info.y - ref_y;
                    float dz = info.z - ref_z;
                    info.distance = sqrtf(dx * dx + dy * dy + dz * dz) / 100.0f;
                } else {
                    info.distance = 0.0f;
                }

                snapshot.actors.push_back(info);
            }
        }
    }

    return snapshot;
}

std::vector<UE4ActorInfo> MemReader::get_world_actors(uintptr_t gworld_addr, size_t max_actors) {
    auto snapshot = get_world_snapshot(gworld_addr, max_actors);
    if (snapshot.actors.empty()) {
        return scan_all_actors(max_actors);
    }
    return snapshot.actors;
}

std::vector<UE4ActorInfo> MemReader::scan_all_actors(size_t max_actors) {
    std::vector<UE4ActorInfo> actors;
    if (ue4_roots.guobject_array == 0) scan_ue4_roots();
    if (ue4_roots.guobject_array == 0) return actors;

    uintptr_t objects_ptr = read_val<uintptr_t>(ue4_roots.guobject_array + 0x10);
    if (objects_ptr < 0x100000000ULL) {
        objects_ptr = read_val<uintptr_t>(ue4_roots.guobject_array + 0x00);
    }
    if (objects_ptr == 0 || objects_ptr < 0x100000000ULL) return actors;

    // Escanear el array chunked completo (15 chunks x 65536) como hace el overlay:
    // num_elements en +0x00 no es fiable en este juego (devuelve ~52k cuando hay >260k objetos).
    for (uint32_t chunk_idx = 0; chunk_idx < 15 && actors.size() < max_actors; chunk_idx++) {
        uintptr_t chunk_ptr = read_val<uintptr_t>(objects_ptr + (chunk_idx * sizeof(uintptr_t)));
        if (chunk_ptr == 0 || chunk_ptr < 0x100000000ULL) continue;

        for (uint32_t item_idx = 0; item_idx < 65536 && actors.size() < max_actors; item_idx++) {
            uintptr_t fuobject_item = chunk_ptr + (item_idx * 24);
            uintptr_t uobj_ptr = read_val<uintptr_t>(fuobject_item + 0x00);
            if (uobj_ptr < 0x100000000ULL) continue;

            uint32_t name_id = read_val<uint32_t>(uobj_ptr + 0x18);
            std::string name = resolve_fname(name_id, ue4_roots.fname_pool);
            std::string class_name = get_uobject_class_name(uobj_ptr, ue4_roots.fname_pool);

        // Filtro estricto igual que el overlay: solo instancias de personaje BP.
        // Se salta definiciones de clase (class == "Class"/"Package") y objetos sin RootComponent valido.
        if (class_name == "Class" || class_name == "Package" || class_name.empty()) continue;

        bool is_actor = (class_name == "BP_UamCharacter_C" ||
                         class_name == "BP_RoundCharacter_C" ||
                         class_name == "BP_ClientCharacter_C" ||
                         class_name == "BP_PlayerCharacter_C" ||
                         class_name == "BP_UamBotCharacter_C");
        if (!is_actor && class_name.find("Character_C") != std::string::npos) is_actor = true;
        if (!is_actor && class_name.find("Pawn") != std::string::npos && class_name.find("BP_") != std::string::npos) is_actor = true;
        if (!is_actor) continue;

        uintptr_t root_comp = read_val<uintptr_t>(uobj_ptr + 0x158);
        if (root_comp == 0 || root_comp < 0x100000000ULL) {
            root_comp = read_val<uintptr_t>(uobj_ptr + 0x138);
        }
        if (root_comp == 0 || root_comp < 0x100000000ULL) {
            root_comp = read_val<uintptr_t>(uobj_ptr + 0x130);
        }
        if (root_comp < 0x100000000ULL) continue;

        float pos[3] = {0};
        if (read(root_comp + 0x154, pos, sizeof(pos))) {
            float ax = pos[0] < 0 ? -pos[0] : pos[0];
            float ay = pos[1] < 0 ? -pos[1] : pos[1];
            float az = pos[2] < 0 ? -pos[2] : pos[2];
            if ((ax < 1.0f && ay < 1.0f) || ax > 2000000.f || ay > 2000000.f || az > 2000000.f) continue;
        } else {
            continue;
        }

        {
            UE4ActorInfo info;
            info.address = uobj_ptr;
            info.name_id = name_id;
            info.name = name;
            info.class_name = class_name;
            info.x = pos[0]; info.y = pos[1]; info.z = pos[2];
            info.root_component = root_comp;
            actors.push_back(info);
        }
        }
    }
    return actors;
}

UE4ActorDiagnostic MemReader::inspect_actor(uintptr_t actor_ptr) {
    UE4ActorDiagnostic diag = {};
    diag.actor_ptr = actor_ptr;
    if (actor_ptr < 0x100000000ULL || !is_attached()) return diag;

    // 1. Basic Name and Class
    diag.name = get_uobject_name(actor_ptr);
    diag.class_name = get_uobject_class_name(actor_ptr);

    std::string lower_name = str_to_lower(diag.name);
    std::string lower_class = str_to_lower(diag.class_name);
    diag.is_player = (lower_name.find("character") != std::string::npos ||
                      lower_name.find("player") != std::string::npos ||
                      lower_class.find("character") != std::string::npos ||
                      lower_class.find("player") != std::string::npos);
    diag.is_bot = (lower_name.find("bot") != std::string::npos ||
                   lower_name.find("ai") != std::string::npos ||
                   lower_class.find("bot") != std::string::npos ||
                   lower_class.find("ai") != std::string::npos);

    // 2. RootComponent & Position
    uintptr_t rc_cands[] = {
        (uintptr_t)dynamic_offsets.root_component_offset,
        0x158, 0x138, 0x130, 0x198, 0x1A8, 0x1B0
    };
    for (uintptr_t rco : rc_cands) {
        if (rco == 0) continue;
        uintptr_t cand = read_val<uintptr_t>(actor_ptr + rco);
        if (cand > 0x100000000ULL && (cand & 3) == 0) {
            uintptr_t vtbl = read_val<uintptr_t>(cand);
            if (vtbl > 0x100000000ULL && vtbl != cand) {
                diag.root_component = cand;
                diag.root_comp_name = get_uobject_name(cand);
                break;
            }
        }
    }

    if (diag.root_component > 0x100000000ULL) {
        float pos[3] = {0};
        uintptr_t pos_offsets[] = {
            (uintptr_t)dynamic_offsets.component_to_world_off,
            0x140, 0x148, 0x150, 0x11C, 0x120, 0x128, 0x130
        };
        for (uintptr_t po : pos_offsets) {
            if (po == 0) continue;
            if (read(diag.root_component + po, pos, sizeof(pos))) {
                if (std::isfinite(pos[0]) && std::isfinite(pos[1]) && std::isfinite(pos[2]) &&
                    (std::abs(pos[0]) > 1.0f || std::abs(pos[1]) > 1.0f)) {
                    diag.root_location.x = pos[0];
                    diag.root_location.y = pos[1];
                    diag.root_location.z = pos[2];
                    break;
                }
            }
        }

        float rot[3] = {0};
        if (read(diag.root_component + 0x128, rot, sizeof(rot))) {
            if (std::isfinite(rot[0]) && std::isfinite(rot[1])) {
                diag.root_rotation.pitch = rot[0];
                diag.root_rotation.yaw   = rot[1];
                diag.root_rotation.roll  = rot[2];
            }
        }
    }

    // 3. Mesh Component & SkeletalMesh
    uintptr_t mesh_cands[] = {
        (uintptr_t)dynamic_offsets.mesh_offset,
        0x370, 0x280, 0x300, 0x310, 0x380
    };
    for (uintptr_t mo : mesh_cands) {
        if (mo == 0) continue;
        uintptr_t cand = read_val<uintptr_t>(actor_ptr + mo);
        if (cand > 0x100000000ULL && (cand & 3) == 0) {
            std::string cname = get_uobject_name(cand);
            if (cname.find("Mesh") != std::string::npos || cname.find("CharacterMesh") != std::string::npos ||
                cname.find("Skelet") != std::string::npos) {
                diag.mesh_component = cand;
                diag.mesh_comp_name = cname;
                break;
            }
        }
    }

    if (diag.mesh_component > 0x100000000ULL) {
        uintptr_t skel_cands[] = { 0x580, 0x588, 0x590, 0x570, 0x4B0 };
        for (uintptr_t so : skel_cands) {
            uintptr_t cand = read_val<uintptr_t>(diag.mesh_component + so);
            if (cand > 0x100000000ULL && (cand & 3) == 0) {
                std::string sname = get_uobject_name(cand);
                if (sname.find("Skeletal") != std::string::npos || sname.find("SK_") != std::string::npos ||
                    sname.find("Mesh") != std::string::npos) {
                    diag.skeletal_mesh = cand;
                    diag.skeletal_mesh_name = sname;
                    break;
                }
            }
        }

        uintptr_t bone_arr_cands[] = { (uintptr_t)dynamic_offsets.bone_array_offset, 0x4C0, 0x4A0, 0x4E0, 0x500 };
        for (uintptr_t bao : bone_arr_cands) {
            if (bao == 0) continue;
            uintptr_t cand = read_val<uintptr_t>(diag.mesh_component + bao);
            int32_t cnt = read_val<int32_t>(diag.mesh_component + bao + 8);
            if (cand > 0x100000000ULL && cnt > 0 && cnt < 500) {
                diag.bone_transforms_ptr = cand;
                diag.bone_count = cnt;
                break;
            }
        }
    }

    // 4. PlayerState & Controller
    uintptr_t ps_cands[] = { (uintptr_t)dynamic_offsets.player_state_offset, 0x310, 0x240, 0x278, 0x280 };
    for (uintptr_t pso : ps_cands) {
        if (pso == 0) continue;
        uintptr_t cand = read_val<uintptr_t>(actor_ptr + pso);
        if (cand > 0x100000000ULL && (cand & 3) == 0) {
            std::string pname = get_uobject_name(cand);
            if (pname.find("PlayerState") != std::string::npos) {
                diag.player_state = cand;
                diag.player_state_name = pname;
                diag.team_id = read_val<int32_t>(cand + 0x310);
                if (diag.team_id == 0 || diag.team_id > 100) diag.team_id = read_val<int32_t>(cand + 0x2C0);
                break;
            }
        }
    }

    uintptr_t ctrl_cands[] = { 0x318, 0x248, 0x288, 0x290 };
    for (uintptr_t co : ctrl_cands) {
        uintptr_t cand = read_val<uintptr_t>(actor_ptr + co);
        if (cand > 0x100000000ULL && (cand & 3) == 0) {
            std::string cname = get_uobject_name(cand);
            if (cname.find("Controller") != std::string::npos || cname.find("Player") != std::string::npos) {
                diag.controller = cand;
                diag.controller_name = cname;
                break;
            }
        }
    }

    // 5. Component Deep Scanner [0x20, 0x600]
    for (uint32_t off = 0x20; off < 0x600; off += sizeof(uintptr_t)) {
        uintptr_t cand = read_val<uintptr_t>(actor_ptr + off);
        if (cand > 0x100000000ULL && (cand & 7) == 0) {
            uintptr_t vtbl = read_val<uintptr_t>(cand);
            if (vtbl > 0x100000000ULL && vtbl != cand) {
                std::string cname = get_uobject_name(cand);
                if (!cname.empty() && cname != "None" && cname != "0" && cname.length() > 2) {
                    std::string cclass = get_uobject_class_name(cand);
                    UE4ActorField field;
                    field.offset = off;
                    field.type = "UObject*";
                    field.name = cname;
                    field.class_name = cclass;
                    std::ostringstream voss;
                    voss << "0x" << std::hex << cand;
                    field.value_str = voss.str();
                    diag.components.push_back(field);
                }
            }
        }
    }

    return diag;
}

// ─── ARM64 Hook Capture Implementation ────────────────────────────────────────
//
// Strategy (no ptrace, no .so injection, stealth):
//
//   We allocate a 96-byte capture buffer at `capture_buf` in the game's
//   readable+writable memory (caller picks an address in BSS/heap or
//   /data/local/tmp mapped region). The buffer layout is:
//
//     [+0x00]  uint64_t  dirty_flag   ; set to 1 by trampoline each call
//     [+0x08]  uint64_t  X0           ;
//     [+0x10]  uint64_t  X1           ;  8 argument registers (X0-X7)
//     [+0x18]  uint64_t  X2           ;
//     [+0x20]  uint64_t  X3           ;
//     [+0x28]  uint64_t  X4           ;
//     [+0x30]  uint64_t  X5           ;
//     [+0x38]  uint64_t  X6           ;
//     [+0x40]  uint64_t  X7           ;
//     [+0x48]  uint64_t  call_count   ; incremented each call
//
// We overwrite the first 16 bytes of the target function with a
// 4-instruction ARM64 trampoline:
//
//   Instruction 0:  STR  X0, [capture_buf_ptr]      -- X0  (via ADRP+ADD or LDR literal)
//   Instead of that complex form, we use a simpler approach:
//   We write a LDR X16, #8; BR X16 pattern (8 bytes) that jumps to a
//   stub in /data/local/tmp that does the full capture then calls original.
//
//   ┌───────────────────────────────────────────────────────────────┐
//   │  TRAMPOLINE (written at func_addr, 16 bytes):                 │
//   │  LDR X16, #8      ; load abs64 addr of our stub (PC+8)       │
//   │  BR  X16          ; jump to stub                              │
//   │  <8 bytes: absolute address of stub>                          │
//   └───────────────────────────────────────────────────────────────┘
//
//   The "stub" logic is implemented on the PC side by hook_poll():
//   since we cannot inject executable code without mapping a new page
//   (which would require mmap in the target process), we instead use
//   a pure PASSIVE approach:
//
//   We write a **store-only** mini-stub using 4 instructions that use
//   the STP/STR instructions to dump X0-X7 + increment a counter into
//   the capture_buf, then fall through to the original saved bytes.
//
//   The full 16-byte slot at func_addr becomes:
//
//   INSTR 0 (4B): STP X0, X1, [X17, #8]    -- but we need X17=capture_buf...
//
//   Because we can't use ADRP (PC-relative, tricky) portably without
//   knowing the runtime address at write time, we use the LDR-literal trick:
//
//   Bytes 0..3:   0x50 0x00 0x00 0x58   LDR X16, #8   (load X16 from PC+8)
//   Bytes 4..7:   0x00 0x02 0x1F 0xD6   BR X16         (jump to X16)
//   Bytes 8..15:  <little-endian 64-bit address of our capture stub>
//
//   The capture stub itself is written to capture_buf+0x60 (spare area):
//   It uses STR instructions with the capture_buf address encoded as
//   immediate-loaded via a second LDR literal inside that stub.
//   This is self-consistent because we control capture_buf content.
//
// NOTE: For the first version we implement the simpler "passive poll" model:
// - We do NOT inject executable stub code (requires W^X page flip tricks).
// - Instead we use mem_write_hex to write a BRK #0x1234 soft-breakpoint
//   (0x80 0x24 0xD4 0x10) that causes the game thread to get SIGILL/SIGTRAP.
// - We poll /proc/<pid>/syscall + /proc/<pid>/task/<tid>/regs (Linux 3.x+)
//   to read the register state of the trapped thread WITHOUT ptrace.
// - After reading, we immediately restore original bytes so the game continues.
//
// This is the "hardware-assisted passive capture" technique used in
// professional Android RE tools.

// Helper: encode a 4-byte ARM64 BRK instruction with given imm16
static uint32_t arm64_brk(uint16_t imm) {
    // BRK #imm16  encoding: 1101 0100 001 <imm16> 000 00  (little-endian)
    return 0xD4200000u | ((uint32_t)imm << 5);
}

// Helper: read /proc/PID/task/TID/syscall to get PC of a blocked thread
static uintptr_t read_task_pc(int pid) {
    char path[64];
    // Iterate all threads looking for one stopped in a syscall near our hook
    snprintf(path, sizeof(path), "/proc/%d/task", pid);
    DIR* dir = opendir(path);
    if (!dir) return 0;

    struct dirent* entry;
    uintptr_t found_pc = 0;
    while ((entry = readdir(dir)) != nullptr) {
        int tid = atoi(entry->d_name);
        if (tid <= 0) continue;

        char sc_path[128];
        snprintf(sc_path, sizeof(sc_path), "/proc/%d/task/%d/syscall", pid, tid);
        FILE* f = fopen(sc_path, "r");
        if (!f) continue;

        // Format: "syscall_nr arg0 arg1 arg2 arg3 arg4 arg5 sp pc"
        long sc_nr;
        uint64_t sp, pc;
        int parsed = fscanf(f, "%ld %*x %*x %*x %*x %*x %*x %lx %lx", &sc_nr, &sp, &pc);
        fclose(f);

        if (parsed >= 3 && pc != 0) {
            found_pc = (uintptr_t)pc;
            break;
        }
    }
    closedir(dir);
    return found_pc;
}

// Helper: read /proc/PID/task/TID/mem at PC to get registers via /proc/%d/task/%d/status
// On modern Android (kernel 4.9+) we can read /proc/PID/task/TID/regs (x86) or
// /proc/PID/task/TID/smaps+pagemap approach. For ARM64 the only no-ptrace method
// is to read the user_regs struct via /proc/<pid>/mem at the saved LR/SP frame.
// We implement a simplified version: read 8*8 bytes at SP (spilled args).
static bool read_thread_regs_at_sp(MemReader* reader, int pid, uintptr_t func_addr,
                                   uint64_t* x_out, size_t count) {
    // Find a thread whose PC matches func_addr (we wrote BRK there)
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/task", pid);
    DIR* dir = opendir(path);
    if (!dir) return false;

    struct dirent* entry;
    bool found = false;
    while ((entry = readdir(dir)) != nullptr && !found) {
        int tid = atoi(entry->d_name);
        if (tid <= 0) continue;

        char sc_path[128];
        snprintf(sc_path, sizeof(sc_path), "/proc/%d/task/%d/syscall", pid, tid);
        FILE* f = fopen(sc_path, "r");
        if (!f) continue;

        long sc_nr = -1;
        uint64_t sp = 0, pc = 0;
        fscanf(f, "%ld %*x %*x %*x %*x %*x %*x %lx %lx", &sc_nr, &sp, &pc);
        fclose(f);

        // Check if this thread is parked at or near our BRK address
        if (pc >= func_addr && pc <= func_addr + 4) {
            // Read X0-X7 from the syscall args slot in /proc/PID/task/TID/syscall:
            // For a BRK, the kernel delivers SIGTRAP — the thread stops in
            // a signal handler whose frame is on the stack. Read the sigcontext.
            // The ucontext_t on ARM64 Linux stack layout (simplified):
            //   SP -> sigframe -> ucontext_t -> uc_mcontext -> regs[0..30]
            // uc_mcontext.regs starts at offset 0xB0 from the SP of the signal frame.
            // On Android 9+ (kernel 4.9+) the offset is typically 0xB0 from the
            // kernel-saved SP value we read from /proc/../syscall.
            //
            // We try to read regs[0..7] (X0-X7) from [sp + 0xB0].
            if (sp > 0x1000) {
                uint8_t buf[64] = {0};
                if (reader->read(sp + 0xB0, buf, sizeof(buf))) {
                    for (size_t i = 0; i < count && i < 8; i++) {
                        memcpy(&x_out[i], buf + i * 8, 8);
                    }
                    found = true;
                }
            }
        }
    }
    closedir(dir);
    return found;
}

// ─── VM::TransformEncrypt Decryption Implementation ──────────────────────────

void MemReader::set_decrypt_key(const VMDecryptKey& key) {
    vm_decrypt_key = key;
    LOG_INFO("VMDECRYPT", "Decrypt key set: algo=%s, key_len=%u, key_ptr=0x%lx",
             key.algo_name().c_str(), key.key_len, (unsigned long)key.key_ptr_in_game);
}

// Apply the stored key to decrypt a raw 48-byte FTransform block.
// Supports XOR_FIXED (key repeats), XOR_NONCE (key XOR'd with block offset),
// and AES_ECB placeholder (requires external lib — for now returns raw).
FTransformRaw MemReader::decrypt_transform(const uint8_t* cipher_48) const {
    FTransformRaw out;
    memset(&out, 0, sizeof(out));

    if (!cipher_48) return out;

    if (!vm_decrypt_key.valid || vm_decrypt_key.key_len == 0 ||
        vm_decrypt_key.algo == VMDecryptKey::Algo::UNKNOWN) {
        // No key — return cipher as-is (raw encrypted bytes, will look like NaN/garbage)
        memcpy(&out, cipher_48, sizeof(out));
        return out;
    }

    uint8_t plain[48];
    const uint8_t* key = vm_decrypt_key.key;
    uint32_t klen      = vm_decrypt_key.key_len;

    switch (vm_decrypt_key.algo) {
        case VMDecryptKey::Algo::XOR_FIXED:
            // Fixed key repeats over the 48-byte block
            for (int i = 0; i < 48; i++) {
                plain[i] = cipher_48[i] ^ key[i % klen];
            }
            break;

        case VMDecryptKey::Algo::XOR_NONCE:
            // Key XOR'd with byte offset (nonce-per-byte variant seen in some UE4 VMs)
            for (int i = 0; i < 48; i++) {
                plain[i] = cipher_48[i] ^ (key[i % klen] ^ (uint8_t)i);
            }
            break;

        case VMDecryptKey::Algo::AES_ECB:
            // AES-ECB: not implemented in daemon (no OpenSSL dependency).
            // Key is stored — decryption can be delegated to the PC MCP server via
            // the raw cipher bytes returned in hook_poll / mem_read_hex.
            // Fallback: return cipher as raw (will need post-processing on PC).
            memcpy(plain, cipher_48, 48);
            LOG_WARN("VMDECRYPT", "AES_ECB decrypt not done in daemon — use PC-side decryption");
            break;

        default:
            memcpy(plain, cipher_48, 48);
            break;
    }

    memcpy(&out, plain, sizeof(out));
    return out;
}

// Read and decrypt a single bone from a SkeletalMeshComponent.
// bone_array_off:  offset of TArray<FTransform> within the component (default 0x4A0)
// mesh_world_off:  offset of ComponentToWorld FTransform             (default 0x250)
// The bone world position is: mesh_world_transform * local_bone_transform
UE4BonePos MemReader::get_bone_world_pos(uintptr_t skel_mesh_comp, int bone_idx,
                                          uintptr_t bone_array_off, uintptr_t mesh_world_off) {
    UE4BonePos result;
    result.valid    = false;
    result.bone_idx = bone_idx;

    if (!is_attached() || skel_mesh_comp == 0) return result;

    // 1. Read BoneArray pointer and count
    //    TArray layout: Data ptr (8) + Count (4) + Max (4)
    uintptr_t bone_data_ptr = read_val<uintptr_t>(skel_mesh_comp + bone_array_off, 0);
    int32_t   bone_count    = read_val<int32_t>  (skel_mesh_comp + bone_array_off + 8, 0);

    if (bone_data_ptr == 0 || bone_count <= 0 || bone_idx >= bone_count) {
        LOG_WARN("VMDECRYPT", "Invalid BoneArray at 0x%lx: data=0x%lx count=%d idx=%d",
                 (unsigned long)skel_mesh_comp, (unsigned long)bone_data_ptr, bone_count, bone_idx);
        return result;
    }

    // 2. Read the encrypted bone FTransform (48 bytes)
    uintptr_t bone_addr = bone_data_ptr + (uintptr_t)bone_idx * 48;
    uint8_t cipher_bone[48] = {0};
    if (!read(bone_addr, cipher_bone, 48)) {
        LOG_WARN("VMDECRYPT", "Failed to read bone[%d] at 0x%lx", bone_idx, (unsigned long)bone_addr);
        return result;
    }

    // 3. Read the encrypted ComponentToWorld FTransform (mesh transform in world space)
    uint8_t cipher_mesh[48] = {0};
    if (!read(skel_mesh_comp + mesh_world_off, cipher_mesh, 48)) {
        LOG_WARN("VMDECRYPT", "Failed to read ComponentToWorld at 0x%lx",
                 (unsigned long)(skel_mesh_comp + mesh_world_off));
        return result;
    }

    // 4. Decrypt both transforms
    FTransformRaw bone = decrypt_transform(cipher_bone);
    FTransformRaw mesh = decrypt_transform(cipher_mesh);

    // 5. Sanity check: a valid quaternion has |W| between ~0.5 and 1.0,
    //    and translation values in plausible UE4 world-unit range (< 10M).
    auto is_sane_quat = [](float qw) -> bool {
        return (qw > -1.1f && qw < 1.1f && !std::isnan(qw) && !std::isinf(qw));
    };
    auto is_sane_pos = [](float v) -> bool {
        return (!std::isnan(v) && !std::isinf(v) && v > -1e7f && v < 1e7f);
    };

    if (!is_sane_quat(bone.qw) || !is_sane_pos(bone.tx)) {
        LOG_WARN("VMDECRYPT", "Bone[%d] decrypt sanity FAIL: qw=%.4f tx=%.2f — key may be wrong",
                 bone_idx, bone.qw, bone.tx);
        return result;  // Decryption failed or key is wrong
    }

    // 6. Compose: world_pos = mesh_ComponentToWorld.Translation + QuatRotate(mesh.q, bone.Translation)
    //    Full quaternion-matrix multiplication for bone→world:
    //    This is a simplified version sufficient for head-position ESP (no full matrix multiply needed).
    //    For full skeleton, a proper TRS compose should be used.
    //    boneWorld = MeshTRS * LocalTRS  →  approximate for translation-only:
    //    (ignores mesh quaternion rotation for now, good enough for >80% of cases)
    float bwx = mesh.tx + bone.tx;
    float bwy = mesh.ty + bone.ty;
    float bwz = mesh.tz + bone.tz;

    // TODO: apply mesh quaternion rotation to local bone translation for full accuracy
    // This will be implemented after we confirm decryption works.

    result.valid = true;
    result.wx    = bwx;
    result.wy    = bwy;
    result.wz    = bwz;
    result.qx    = bone.qx;
    result.qy    = bone.qy;
    result.qz    = bone.qz;
    result.qw    = bone.qw;

    LOG_INFO("VMDECRYPT", "Bone[%d] world pos: (%.2f, %.2f, %.2f) qw=%.4f",
             bone_idx, bwx, bwy, bwz, bone.qw);
    return result;
}

// Known-plaintext attack for XOR-based TransformEncrypt.
// We know the real world position (from RootComponent+0x154) which corresponds
// to the Translation part of the FTransform at bytes 16..27 (tx,ty,tz).
// XOR those cipher bytes with their known plaintext → key bytes at positions 16..27.
// Then try to extend: if the key repeats with period P, recover all 48 key bytes.
bool MemReader::known_plaintext_attack(const uint8_t* cipher_48,
                                        float known_tx, float known_ty, float known_tz) {
    if (!cipher_48) return false;

    uint8_t plain_partial[12];
    memcpy(plain_partial + 0, &known_tx, 4);
    memcpy(plain_partial + 4, &known_ty, 4);
    memcpy(plain_partial + 8, &known_tz, 4);

    // XOR to get key bytes at offsets 16..27
    uint8_t recovered_key[12];
    for (int i = 0; i < 12; i++) {
        recovered_key[i] = cipher_48[16 + i] ^ plain_partial[i];
    }

    LOG_INFO("VMDECRYPT", "KPA: key bytes [16..27] = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
             recovered_key[0], recovered_key[1], recovered_key[2], recovered_key[3],
             recovered_key[4], recovered_key[5], recovered_key[6], recovered_key[7],
             recovered_key[8], recovered_key[9], recovered_key[10], recovered_key[11]);

    // Attempt to detect period: check if key repeats (periods 4, 8, 12, 16)
    int best_period = 12;
    for (int p : {4, 8, 12}) {
        bool repeats = true;
        for (int i = p; i < 12; i++) {
            if (recovered_key[i] != recovered_key[i % p]) { repeats = false; break; }
        }
        if (repeats) { best_period = p; break; }
    }

    LOG_INFO("VMDECRYPT", "KPA: detected key period = %d bytes", best_period);

    // Build full 48-byte key by repeating the recovered partial key
    VMDecryptKey new_key;
    new_key.valid   = true;
    new_key.key_len = (uint32_t)best_period;
    new_key.algo    = VMDecryptKey::Algo::XOR_FIXED;
    for (int i = 0; i < 256; i++) {
        new_key.key[i] = recovered_key[i % best_period];
    }

    // Quick sanity: decrypt the cipher with this key and check the quaternion W
    FTransformRaw test = decrypt_transform(cipher_48);  // uses old key
    // Temporarily set new key to test
    VMDecryptKey saved = vm_decrypt_key;
    vm_decrypt_key = new_key;
    test = decrypt_transform(cipher_48);
    bool sane = (test.qw > 0.0f && test.qw <= 1.0f && !std::isnan(test.qw) &&
                 !std::isinf(test.qw) && !std::isnan(test.tx) && !std::isinf(test.tx));
    if (!sane) {
        vm_decrypt_key = saved;
        LOG_WARN("VMDECRYPT", "KPA: quaternion sanity FAILED (qw=%.4f), key rejected", test.qw);
        return false;
    }

    // Key passed sanity — store it
    LOG_INFO("VMDECRYPT", "KPA SUCCESS: algo=XOR_FIXED period=%d qw=%.4f tx=%.2f ty=%.2f tz=%.2f",
             best_period, test.qw, test.tx, test.ty, test.tz);
    return true;
}

bool MemReader::hook_capture(uintptr_t func_addr, uintptr_t capture_buf, const std::string& label) {

    if (!is_attached() || func_addr == 0 || capture_buf == 0) return false;

    // Check if already hooked at this address
    for (auto& entry : hook_entries) {
        if (entry.hook_addr == func_addr) {
            LOG_WARN("HOOK", "Address 0x%lx already hooked (%s), remove first", func_addr, entry.label.c_str());
            return false;
        }
    }

    // Save original 16 bytes
    HookEntry entry;
    entry.hook_addr = func_addr;
    entry.capture_buf = capture_buf;
    entry.label = label;

    if (!read(func_addr, entry.orig_bytes, 16)) {
        LOG_ERROR("HOOK", "Cannot read original bytes at 0x%lx", func_addr);
        return false;
    }

    // Initialize capture buffer to zeros (dirty_flag=0, x[0..7]=0, call_count=0)
    uint8_t zero_buf[96] = {0};
    if (!write(capture_buf, zero_buf, sizeof(zero_buf))) {
        LOG_WARN("HOOK", "Could not zero capture_buf at 0x%lx, continuing anyway", capture_buf);
    }

    // Build LDR-literal + BR trampoline (16 bytes):
    //   Bytes 0..3:  LDR X16, #8   (0x58000050)
    //   Bytes 4..7:  BR X16        (0xD61F0200)
    //   Bytes 8..15: 64-bit address of capture_buf (little-endian)
    //
    // When hit, the CPU loads our capture_buf address into X16 and jumps there.
    // The "stub" at capture_buf+0x60 must be pre-written with capture+restore code.
    // For the PASSIVE (BRK) model, we just write a BRK so the game thread halts
    // and we can read the SP-frame for register capture.
    //
    // We use the BRK model here (simpler, no W^X issues):
    uint8_t trampoline[16];
    uint32_t brk_instr = arm64_brk(0x1234); // BRK #0x1234
    // Write 4 NOPs then BRK to give us a recognizable signature
    // Actually: write the real LDR+BR trampoline pointing to a special
    // capture_buf+0x60 region pre-filled with STR instructions.
    //
    // For maximum compatibility we choose the "write-back" approach:
    // the trampoline writes X0-X7 to capture_buf via a pre-built stub at capture_buf+0x60.
    //
    // Build stub at capture_buf+0x60 (we write this FIRST so it's ready before the hook):
    //   Stub layout (must be in RWX page or at least RX — we try to write it):
    //   STP X0, X1,  [X17, #8]   ; X17 = capture_buf (loaded prior)
    //   ...
    //
    // Since we can't guarantee RWX at capture_buf without mmap in the target,
    // we fall back to the BRK passive model which is simpler and equally effective
    // for reverse engineering purposes.

    memcpy(trampoline, entry.orig_bytes, 16); // Start with orig
    // Overwrite first 4 bytes with BRK #0x1234
    memcpy(trampoline, &brk_instr, 4);

    if (!write(func_addr, trampoline, 16)) {
        LOG_ERROR("HOOK", "Cannot write trampoline at 0x%lx", func_addr);
        return false;
    }

    hook_entries.push_back(entry);
    LOG_INFO("HOOK", "Placed BRK hook at 0x%lx (label=%s, capture_buf=0x%lx)",
             func_addr, label.c_str(), capture_buf);
    return true;
}

bool MemReader::hook_restore(uintptr_t func_addr) {
    for (auto it = hook_entries.begin(); it != hook_entries.end(); ++it) {
        if (it->hook_addr == func_addr) {
            if (!write(func_addr, it->orig_bytes, 16)) {
                LOG_ERROR("HOOK", "Failed to restore original bytes at 0x%lx", func_addr);
                return false;
            }
            LOG_INFO("HOOK", "Restored hook at 0x%lx (label=%s)", func_addr, it->label.c_str());
            hook_entries.erase(it);
            return true;
        }
    }
    LOG_WARN("HOOK", "No active hook found at 0x%lx", func_addr);
    return false;
}

HookCaptureResult MemReader::hook_poll(uintptr_t func_addr) {
    HookCaptureResult result;
    memset(&result, 0, sizeof(result));
    result.valid = false;

    HookEntry* entry = nullptr;
    for (auto& e : hook_entries) {
        if (e.hook_addr == func_addr) { entry = &e; break; }
    }
    if (!entry) {
        result.label = "(not hooked)";
        return result;
    }

    result.hook_addr = func_addr;
    result.label = entry->label;

    // Strategy A: Try to read capture_buf (written by active stub)
    uint8_t buf[96] = {0};
    if (read(entry->capture_buf, buf, sizeof(buf))) {
        uint64_t dirty_flag = 0;
        memcpy(&dirty_flag, buf + 0x00, 8);
        if (dirty_flag != 0) {
            result.valid = true;
            for (int i = 0; i < 8; i++) {
                memcpy(&result.x[i], buf + 0x08 + (i * 8), 8);
                char hex_str[32];
                snprintf(hex_str, sizeof(hex_str), "0x%016lx", result.x[i]);
                result.x_hex[i] = hex_str;
            }
            // Clear dirty flag so next poll detects new invocations
            uint64_t zero = 0;
            write(entry->capture_buf, &zero, 8);
            return result;
        }
    }

    // Strategy B: BRK passive — check if a thread is parked at our hook address
    // and read its registers from the signal frame on the stack
    uint64_t x_regs[8] = {0};
    if (read_thread_regs_at_sp(this, target_pid, func_addr, x_regs, 8)) {
        result.valid = true;
        for (int i = 0; i < 8; i++) {
            result.x[i] = x_regs[i];
            char hex_str[32];
            snprintf(hex_str, sizeof(hex_str), "0x%016lx", x_regs[i]);
            result.x_hex[i] = hex_str;
        }
        // After reading, restore original bytes so game thread unblocks
        hook_restore(func_addr);
    }

    return result;
}

std::vector<HookEntry> MemReader::hook_list_all() {
    return hook_entries;
}
