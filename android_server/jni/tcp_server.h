#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <string>
#include <vector>
#include <mutex>
#include "mem_reader.h"

// Unix Abstract Socket name - NOT visible in /proc/net/tcp (AB cannot detect it)
// AB can only read /proc/net/tcp and /proc/net/tcp6 without root.
// Abstract sockets only appear in /proc/net/unix with no path, invisible to per-app scanners.
#define UNIX_SOCKET_NAME "memsvc"

class TcpServer {
public:
    TcpServer();  // No port - uses Unix Abstract Socket
    ~TcpServer();

    bool start();
    void run();
    void stop();

private:
    int server_fd;  // Unix Abstract Socket fd (no TCP port)
    bool is_running;
    MemReader mem_reader;
    std::mutex cmd_mutex;

    void handle_client(int client_fd);
    std::string process_command(const std::string& raw_cmd);
    
    // Command handlers
    std::string cmd_attach(const std::string& target);
    std::string cmd_detach();
    std::string cmd_status();
    std::string cmd_list_processes();
    std::string cmd_get_modules();
    std::string cmd_read(uintptr_t addr, size_t size);
    std::string cmd_write(uintptr_t addr, const std::string& hex_data);
    std::string cmd_write_typed(uintptr_t addr, const std::string& type, const std::string& val_str);
    std::string cmd_patch(uintptr_t addr, const std::string& hex_patch);
    std::string cmd_restore(uintptr_t addr, const std::string& hex_orig);
    std::string cmd_read_ptr(uintptr_t base, const std::vector<uintptr_t>& offsets);
    std::string cmd_read_string(uintptr_t addr, size_t max_len);
    std::string cmd_scan(uintptr_t start, uintptr_t end, const std::string& pattern);
    std::string cmd_scan_mod(const std::string& mod_name, const std::string& pattern);
    std::string cmd_scan_all(const std::string& pattern);
    
    // UE4 Reflection Commands
    std::string cmd_ue4_roots();
    std::string cmd_fname(uint32_t index);
    std::string cmd_uobj(uint32_t index);
    std::string cmd_actors(uintptr_t gworld, size_t limit);
    std::string cmd_inspect_actor(uintptr_t actor_ptr);
    std::string cmd_dump_elf(const std::string& mod_name, const std::string& out_path);
    // Diagnostic & Logging Commands
    std::string cmd_get_logs(size_t limit, int min_lvl);
    std::string cmd_clear_logs();

    // Archive / Compression Commands
    std::string cmd_compress(const std::string& in_path, const std::string& out_path, const std::string& format);
    std::string cmd_decompress(const std::string& archive_path, const std::string& out_dir);

    // File Transfer & Management Commands
    std::string cmd_fs_list(const std::string& dir_path);
    std::string cmd_fs_stat(const std::string& file_path);
    std::string cmd_fs_read_chunk(const std::string& file_path, uint64_t offset, size_t size);
    std::string cmd_fs_write_chunk(const std::string& file_path, uint64_t offset, const std::string& b64_data, bool truncate);
    std::string cmd_fs_delete(const std::string& path, bool recursive);
    std::string cmd_fs_rename(const std::string& old_path, const std::string& new_path);
    std::string cmd_fs_mkdir(const std::string& dir_path);

    // Auto Update & Hot Replacement Command
    std::string cmd_update_server(const std::string& update_path, const std::string& target_path);

    // ─── ARM64 Hook Capture Commands ───────────────────────────────
    std::string cmd_hook_capture(uintptr_t func_addr, uintptr_t capture_buf, const std::string& label);
    std::string cmd_hook_restore(uintptr_t func_addr);
    std::string cmd_hook_poll(uintptr_t func_addr);
    std::string cmd_hook_list();

    // ─── VM::TransformEncrypt Decryption Commands ───────────────────
    // set_decrypt_key <algo> <hex_key>
    //   algo: XOR_FIXED | XOR_NONCE | AES_ECB
    //   hex_key: hex string of the key bytes
    // kpa <cipher_hex> <tx> <ty> <tz>
    //   Runs known-plaintext attack with a known position
    // get_bone <skel_mesh_ptr> <bone_idx> [bone_array_off] [mesh_world_off]
    //   Reads and decrypts a single bone from the game
    // decrypt_key_status
    //   Returns current key info (algo, len, valid)
    std::string cmd_set_decrypt_key(const std::string& algo, const std::string& hex_key);
    std::string cmd_known_plaintext(const std::string& cipher_hex, float tx, float ty, float tz);
    std::string cmd_get_bone(uintptr_t skel_mesh_comp, int bone_idx, uintptr_t bone_arr_off, uintptr_t mesh_world_off);
    std::string cmd_decrypt_key_status();
    std::string cmd_clear_decrypt_key();

    // ─── Dynamic Live Offset Overrides ─────────────────────────────
    std::string cmd_set_ue4_config(const std::string& key, const std::string& val);
    std::string cmd_get_ue4_config();

    // ─── Dynamic Live Draw / ESP Configuration ─────────────────────
    std::string cmd_set_draw_config(const std::string& key, const std::string& val);
    std::string cmd_get_draw_config();
};

#endif // TCP_SERVER_H
