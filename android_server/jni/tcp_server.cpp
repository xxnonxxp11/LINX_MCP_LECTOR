#include "tcp_server.h"
#include "elf_fixer.h"
#include "logger.h"
#include "compressor.h"
#include "file_manager.h"
#include "auto_updater.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>      // Unix Domain Sockets
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <thread>

static std::string bytes_to_hex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; i++) {
        oss << std::setw(2) << (int)data[i];
    }
    return oss.str();
}

static std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i + 1 < hex.length(); i += 2) {
        std::string byte_string = hex.substr(i, 2);
        uint8_t byte = (uint8_t)strtoul(byte_string.c_str(), nullptr, 16);
        bytes.push_back(byte);
    }
    return bytes;
}

static std::string escape_json(const std::string& s) {
    std::ostringstream o;
    for (auto c = s.cbegin(); c != s.cend(); c++) {
        switch (*c) {
        case '"': o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\b': o << "\\b"; break;
        case '\f': o << "\\f"; break;
        case '\n': o << "\\n"; break;
        case '\r': o << "\\r"; break;
        case '\t': o << "\\t"; break;
        default:
            if ('\x00' <= *c && *c <= '\x1f') {
                o << "\\u"
                  << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
            } else {
                o << *c;
            }
        }
    }
    return o.str();
}

static uintptr_t parse_hex_or_dec(const std::string& str) {
    if (str.empty()) return 0;
    if (str.length() > 2 && (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))) {
        return strtoull(str.c_str(), nullptr, 16);
    }
    return strtoull(str.c_str(), nullptr, 16); // Default hex
}

TcpServer::TcpServer() : server_fd(-1), is_running(false) {
}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start() {
    // ── Unix Abstract Socket ─────────────────────────────────────────────────
    // Uses AF_UNIX with abstract namespace (sun_path[0] = '\0').
    // Abstract sockets are NOT visible in /proc/net/tcp or /proc/net/tcp6,
    // so Arena Breakout anti-cheat CANNOT detect the daemon via network scanning.
    // Only appears in /proc/net/unix with no readable path — effectively invisible.
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[!] Unix socket creation failed");
        return false;
    }

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    // Abstract namespace: first byte is \0, rest is the socket name
    // Equivalent to: adb forward tcp:8088 localabstract:memsvc
    address.sun_path[0] = '\0';
    strncpy(address.sun_path + 1, UNIX_SOCKET_NAME, sizeof(address.sun_path) - 2);
    // Size = offsetof + 1 (null) + strlen(name)
    socklen_t addrlen = offsetof(struct sockaddr_un, sun_path) + 1 + strlen(UNIX_SOCKET_NAME);

    // Retry bind up to 5 times in case of leftover socket from previous daemon
    bool bound = false;
    for (int retry = 0; retry < 5; retry++) {
        if (bind(server_fd, (struct sockaddr*)&address, addrlen) == 0) {
            bound = true;
            break;
        }
        usleep(300000); // 300ms
    }

    if (!bound) {
        perror("[!] Unix socket bind failed");
        close(server_fd);
        server_fd = -1;
        return false;
    }

    if (listen(server_fd, 16) < 0) {
        perror("[!] Unix socket listen failed");
        close(server_fd);
        server_fd = -1;
        return false;
    }

    is_running = true;
    printf("[+] Memory Daemon listening on UNIX abstract socket @%s (Stealth - not in /proc/net/tcp)\n", UNIX_SOCKET_NAME);
    printf("[+] PC Connect via: adb forward tcp:8088 localabstract:%s\n", UNIX_SOCKET_NAME);
    return true;
}

void TcpServer::stop() {
    is_running = false;
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
    }
    mem_reader.detach();
}

void TcpServer::run() {
    LOG_INFO("NET", "Server listening on Unix abstract socket @memsvc (Multithreaded)...");
    printf("\n");
    printf("\033[1;33m┌────────────────────────────────────────────────────────┐\033[0m\n");
    printf("\033[1;33m│  [ESTADO CELULAR] ACTIVO (MULTIHILO) - LISTO!          │\033[0m\n");
    printf("\033[1;33m│  Socket: @memsvc | Stealth (no TCP port abierto)       │\033[0m\n");
    printf("\033[1;33m└────────────────────────────────────────────────────────┘\033[0m\n\n");
    fflush(stdout);

    while (is_running) {
        // For Unix domain sockets the client address is AF_UNIX (no IP/port info)
        struct sockaddr_un client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_fd < 0) {
            if (!is_running) break;
            continue;
        }

        static int conn_id = 0;
        int cid = ++conn_id;
        printf("\n\033[1;32m┌────────────────────────────────────────────────────────┐\033[0m\n");
        printf("\033[1;32m│  [ESTADO CELULAR] ✅ CLIENTE CONECTADO (NUEVO HILO)    │\033[0m\n");
        printf("\033[1;32m│  Unix socket client #%d (ADB tunnel)                  │\033[0m\n", cid);
        printf("\033[1;32m└────────────────────────────────────────────────────────┘\033[0m\n\n");
        fflush(stdout);

        LOG_INFO("NET", "Client connected via @memsvc (conn #%d, spawning thread)", cid);

        // Despachar cada conexión en su propio hilo independiente (Multithreaded)
        std::thread([this, client_fd, cid]() {
            this->handle_client(client_fd);
            close(client_fd);
            LOG_INFO("NET", "Client #%d disconnected", cid);
        }).detach();
    }
}

static bool send_all(int fd, const char* data, size_t total_len) {
    size_t sent = 0;
    while (sent < total_len) {
        ssize_t n = write(fd, data + sent, total_len - sent);
        if (n <= 0) {
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                usleep(1000);
                continue;
            }
            return false;
        }
        sent += n;
    }
    return true;
}

void TcpServer::handle_client(int client_fd) {
    // Set TCP_NODELAY and larger send buffer for high throughput without drops
    int flag = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
    int buf_size = 256 * 1024;
    setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(int));
    setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(int));

    std::string buffer;
    char temp_buf[4096];

    while (is_running) {
        ssize_t bytes = read(client_fd, temp_buf, sizeof(temp_buf) - 1);
        if (bytes <= 0) break;
        temp_buf[bytes] = '\0';
        buffer += temp_buf;

        size_t newline_pos;
        while ((newline_pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, newline_pos);
            buffer.erase(0, newline_pos + 1);

            // Strip trailing \r
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.empty()) continue;

            std::string response = process_command(line);
            response += "\n";
            if (!send_all(client_fd, response.c_str(), response.length())) {
                LOG_WARN("NET", "Client write failed or disconnected");
                return;
            }
        }
    }
}

std::string TcpServer::process_command(const std::string& raw_cmd) {
    std::lock_guard<std::mutex> lock(cmd_mutex);
    std::istringstream iss(raw_cmd);
    std::string cmd;
    iss >> cmd;

    // Convert cmd to lowercase
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    if (cmd == "ping") {
        return "{\"status\":\"ok\",\"pong\":true,\"attached\":" + 
               std::string(mem_reader.is_attached() ? "true" : "false") + 
               ",\"pid\":" + std::to_string(mem_reader.get_pid()) + 
               ",\"target\":\"" + escape_json(mem_reader.get_process_name()) + "\"}";
    }
    else if (cmd == "status") {
        return cmd_status();
    }
    else if (cmd == "attach") {
        std::string target;
        iss >> target;
        return cmd_attach(target);
    }
    else if (cmd == "detach") {
        return cmd_detach();
    }
    else if (cmd == "ps" || cmd == "list_processes") {
        return cmd_list_processes();
    }
    else if (cmd == "modules" || cmd == "get_modules") {
        return cmd_get_modules();
    }
    else if (cmd == "read") {
        std::string addr_str, size_str;
        iss >> addr_str >> size_str;
        uintptr_t addr = parse_hex_or_dec(addr_str);
        size_t size = size_str.empty() ? 4 : strtoul(size_str.c_str(), nullptr, 0);
        return cmd_read(addr, size);
    }
    else if (cmd == "write") {
        std::string addr_str, hex_data;
        iss >> addr_str >> hex_data;
        uintptr_t addr = parse_hex_or_dec(addr_str);
        return cmd_write(addr, hex_data);
    }
    else if (cmd == "write_typed" || cmd == "write_val") {
        std::string addr_str, type, val_str;
        iss >> addr_str >> type;
        std::getline(iss, val_str);
        size_t first_non_space = val_str.find_first_not_of(" \t");
        if (first_non_space != std::string::npos) val_str = val_str.substr(first_non_space);
        uintptr_t addr = parse_hex_or_dec(addr_str);
        return cmd_write_typed(addr, type, val_str);
    }
    else if (cmd == "write_float" || cmd == "set_float") {
        std::string addr_str, val_str;
        iss >> addr_str >> val_str;
        uintptr_t addr = parse_hex_or_dec(addr_str);
        return cmd_write_typed(addr, "float", val_str);
    }
    else if (cmd == "write_int" || cmd == "write_i32" || cmd == "set_int") {
        std::string addr_str, val_str;
        iss >> addr_str >> val_str;
        uintptr_t addr = parse_hex_or_dec(addr_str);
        return cmd_write_typed(addr, "int32", val_str);
    }
    else if (cmd == "write_int64" || cmd == "write_i64" || cmd == "set_int64") {
        std::string addr_str, val_str;
        iss >> addr_str >> val_str;
        uintptr_t addr = parse_hex_or_dec(addr_str);
        return cmd_write_typed(addr, "int64", val_str);
    }
    else if (cmd == "patch") {
        std::string addr_str, hex_patch;
        iss >> addr_str >> hex_patch;
        uintptr_t addr = parse_hex_or_dec(addr_str);
        return cmd_patch(addr, hex_patch);
    }
    else if (cmd == "restore" || cmd == "unpatch") {
        std::string addr_str, hex_orig;
        iss >> addr_str >> hex_orig;
        uintptr_t addr = parse_hex_or_dec(addr_str);
        return cmd_restore(addr, hex_orig);
    }
    else if (cmd == "read_ptr") {
        std::string base_str;
        iss >> base_str;
        uintptr_t base = parse_hex_or_dec(base_str);
        std::vector<uintptr_t> offsets;
        std::string off_str;
        while (iss >> off_str) {
            offsets.push_back(parse_hex_or_dec(off_str));
        }
        return cmd_read_ptr(base, offsets);
    }
    else if (cmd == "read_str" || cmd == "read_string") {
        std::string addr_str, len_str;
        iss >> addr_str >> len_str;
        uintptr_t addr = parse_hex_or_dec(addr_str);
        size_t max_len = len_str.empty() ? 64 : strtoul(len_str.c_str(), nullptr, 0);
        return cmd_read_string(addr, max_len);
    }
    else if (cmd == "scan") {
        std::string start_str, end_str;
        iss >> start_str >> end_str;
        uintptr_t start = parse_hex_or_dec(start_str);
        uintptr_t end = parse_hex_or_dec(end_str);
        std::string pattern;
        std::getline(iss, pattern);
        // trim leading spaces
        size_t first_non_space = pattern.find_first_not_of(" \t");
        if (first_non_space != std::string::npos) pattern = pattern.substr(first_non_space);
        return cmd_scan(start, end, pattern);
    }
    else if (cmd == "scan_mod") {
        std::string mod_name;
        iss >> mod_name;
        std::string pattern;
        std::getline(iss, pattern);
        size_t first_non_space = pattern.find_first_not_of(" \t");
        if (first_non_space != std::string::npos) pattern = pattern.substr(first_non_space);
        return cmd_scan_mod(mod_name, pattern);
    }
    else if (cmd == "scan_all") {
        std::string pattern;
        std::getline(iss, pattern);
        size_t first_non_space = pattern.find_first_not_of(" \t");
        if (first_non_space != std::string::npos) pattern = pattern.substr(first_non_space);
        return cmd_scan_all(pattern);
    }
    else if (cmd == "roots" || cmd == "ue4_roots") {
        return cmd_ue4_roots();
    }
    else if (cmd == "fname" || cmd == "resolve_fname") {
        std::string idx_str;
        iss >> idx_str;
        uint32_t idx = idx_str.empty() ? 0 : (uint32_t)strtoul(idx_str.c_str(), nullptr, 0);
        return cmd_fname(idx);
    }
    else if (cmd == "uobj" || cmd == "get_uobject") {
        std::string idx_str;
        iss >> idx_str;
        uint32_t idx = idx_str.empty() ? 0 : (uint32_t)strtoul(idx_str.c_str(), nullptr, 0);
        return cmd_uobj(idx);
    }
    else if (cmd == "actors" || cmd == "get_actors") {
        std::string gworld_str, limit_str;
        iss >> gworld_str >> limit_str;
        uintptr_t gworld = parse_hex_or_dec(gworld_str);
        size_t limit = limit_str.empty() ? 512 : strtoul(limit_str.c_str(), nullptr, 0);
        return cmd_actors(gworld, limit);
    }
    else if (cmd == "inspect_actor" || cmd == "inspect_player" || cmd == "inspect") {
        std::string actor_str;
        iss >> actor_str;
        uintptr_t actor_ptr = parse_hex_or_dec(actor_str);
        return cmd_inspect_actor(actor_ptr);
    }
    else if (cmd == "set_config" || cmd == "set_ue4_config" || cmd == "set_offset") {
        std::string key, val;
        iss >> key >> val;
        return cmd_set_ue4_config(key, val);
    }
    else if (cmd == "get_config" || cmd == "get_ue4_config") {
        return cmd_get_ue4_config();
    }
    else if (cmd == "set_draw" || cmd == "set_draw_config" || cmd == "set_esp") {
        std::string key, val;
        iss >> key >> val;
        return cmd_set_draw_config(key, val);
    }
    else if (cmd == "get_draw" || cmd == "get_draw_config" || cmd == "get_esp") {
        return cmd_get_draw_config();
    }
    else if (cmd == "dump_elf" || cmd == "dump_so") {
        std::string mod_name, out_path;
        iss >> mod_name >> out_path;
        if (mod_name.empty()) mod_name = "libUE4.so";
        if (out_path.empty()) out_path = "/data/local/tmp/dumped_fixed.so";
        return cmd_dump_elf(mod_name, out_path);
    }
    else if (cmd == "get_logs" || cmd == "logs") {
        std::string limit_str, min_lvl_str;
        iss >> limit_str >> min_lvl_str;
        size_t limit = limit_str.empty() ? 100 : strtoul(limit_str.c_str(), nullptr, 0);
        int min_lvl = min_lvl_str.empty() ? 0 : atoi(min_lvl_str.c_str());
        return cmd_get_logs(limit, min_lvl);
    }
    else if (cmd == "clear_logs") {
        return cmd_clear_logs();
    }
    else if (cmd == "compress" || cmd == "zip" || cmd == "tar") {
        std::string in_path, out_path, format;
        iss >> in_path >> out_path >> format;
        if (format.empty()) format = "zip";
        return cmd_compress(in_path, out_path, format);
    }
    else if (cmd == "decompress" || cmd == "unzip" || cmd == "untar") {
        std::string archive_path, out_dir;
        iss >> archive_path >> out_dir;
        if (out_dir.empty()) out_dir = "/data/local/tmp/";
        return cmd_decompress(archive_path, out_dir);
    }
    else if (cmd == "fs_list" || cmd == "ls") {
        std::string dir_path;
        iss >> dir_path;
        if (dir_path.empty()) dir_path = "/data/local/tmp/";
        return cmd_fs_list(dir_path);
    }
    else if (cmd == "fs_stat" || cmd == "stat") {
        std::string file_path;
        iss >> file_path;
        return cmd_fs_stat(file_path);
    }
    else if (cmd == "fs_read") {
        std::string file_path, off_str, sz_str;
        iss >> file_path >> off_str >> sz_str;
        uint64_t offset = off_str.empty() ? 0 : strtoull(off_str.c_str(), nullptr, 0);
        size_t size = sz_str.empty() ? 65536 : strtoul(sz_str.c_str(), nullptr, 0);
        return cmd_fs_read_chunk(file_path, offset, size);
    }
    else if (cmd == "fs_write") {
        std::string file_path, off_str, trunc_str, b64_data;
        iss >> file_path >> off_str >> trunc_str >> b64_data;
        uint64_t offset = off_str.empty() ? 0 : strtoull(off_str.c_str(), nullptr, 0);
        bool truncate = (trunc_str == "1" || trunc_str == "true");
        return cmd_fs_write_chunk(file_path, offset, b64_data, truncate);
    }
    else if (cmd == "fs_delete" || cmd == "rm") {
        std::string path, rec_str;
        iss >> path >> rec_str;
        bool recursive = (rec_str == "1" || rec_str == "true" || rec_str == "-r" || rec_str == "-rf");
        return cmd_fs_delete(path, recursive);
    }
    else if (cmd == "fs_rename" || cmd == "mv") {
        std::string old_path, new_path;
        iss >> old_path >> new_path;
        return cmd_fs_rename(old_path, new_path);
    }
    else if (cmd == "fs_mkdir" || cmd == "mkdir") {
        std::string dir_path;
        iss >> dir_path;
        return cmd_fs_mkdir(dir_path);
    }
    else if (cmd == "update" || cmd == "auto_update" || cmd == "hot_update") {
        std::string update_path, target_path;
        iss >> update_path >> target_path;
        if (update_path.empty()) update_path = "/data/local/tmp/updates/mem_server_new.sh";
        if (target_path.empty()) target_path = "/data/local/tmp/mem_server.sh";
        return cmd_update_server(update_path, target_path);
    }
    // ─── ARM64 Hook Capture Commands ────────────────────────────────────────
    // hook_capture <func_addr_hex> <capture_buf_hex> <label>
    // hook_restore <func_addr_hex>
    // hook_poll    <func_addr_hex>
    // hook_list
    else if (cmd == "hook_capture" || cmd == "hook") {
        std::string func_str, buf_str, label;
        iss >> func_str >> buf_str;
        std::getline(iss, label);
        size_t first_ns = label.find_first_not_of(" \t");
        if (first_ns != std::string::npos) label = label.substr(first_ns);
        if (label.empty()) label = "hook_" + func_str;
        uintptr_t func_addr = parse_hex_or_dec(func_str);
        uintptr_t capture_buf = parse_hex_or_dec(buf_str);
        return cmd_hook_capture(func_addr, capture_buf, label);
    }
    else if (cmd == "hook_restore" || cmd == "unhook") {
        std::string func_str;
        iss >> func_str;
        uintptr_t func_addr = parse_hex_or_dec(func_str);
        return cmd_hook_restore(func_addr);
    }
    else if (cmd == "hook_poll" || cmd == "poll") {
        std::string func_str;
        iss >> func_str;
        uintptr_t func_addr = parse_hex_or_dec(func_str);
        return cmd_hook_poll(func_addr);
    }
    else if (cmd == "hook_list" || cmd == "hooks") {
        return cmd_hook_list();
    }
    // ─── VM::TransformEncrypt Decryption Commands ──────────────────────────────
    // set_decrypt_key <algo> <hex_key>
    // kpa <cipher_hex_96chars> <tx> <ty> <tz>
    // get_bone <skel_mesh_ptr_hex> <bone_idx> [bone_arr_off_hex] [mesh_world_off_hex]
    // decrypt_key_status
    // clear_decrypt_key
    else if (cmd == "set_decrypt_key" || cmd == "set_key") {
        std::string algo, hex_key;
        iss >> algo >> hex_key;
        return cmd_set_decrypt_key(algo, hex_key);
    }
    else if (cmd == "kpa" || cmd == "known_plaintext") {
        std::string cipher_hex, tx_s, ty_s, tz_s;
        iss >> cipher_hex >> tx_s >> ty_s >> tz_s;
        float tx = tx_s.empty() ? 0.0f : strtof(tx_s.c_str(), nullptr);
        float ty = ty_s.empty() ? 0.0f : strtof(ty_s.c_str(), nullptr);
        float tz = tz_s.empty() ? 0.0f : strtof(tz_s.c_str(), nullptr);
        return cmd_known_plaintext(cipher_hex, tx, ty, tz);
    }
    else if (cmd == "get_bone") {
        std::string ptr_s, idx_s, boff_s, moff_s;
        iss >> ptr_s >> idx_s >> boff_s >> moff_s;
        uintptr_t skel_ptr    = parse_hex_or_dec(ptr_s);
        int bone_idx          = idx_s.empty() ? 82 : atoi(idx_s.c_str());  // 82 = head default
        uintptr_t bone_arr_off = boff_s.empty() ? 0x4A0 : parse_hex_or_dec(boff_s);
        uintptr_t mesh_off     = moff_s.empty() ? 0x250 : parse_hex_or_dec(moff_s);
        return cmd_get_bone(skel_ptr, bone_idx, bone_arr_off, mesh_off);
    }
    else if (cmd == "decrypt_key_status" || cmd == "key_status") {
        return cmd_decrypt_key_status();
    }
    else if (cmd == "clear_decrypt_key" || cmd == "clear_key") {
        return cmd_clear_decrypt_key();
    }
    else {
        return "{\"status\":\"error\",\"message\":\"Unknown command: " + escape_json(cmd) + "\"}";
    }
}

std::string TcpServer::cmd_attach(const std::string& target) {
    if (target.empty()) {
        return "{\"status\":\"error\",\"message\":\"Missing target pid or package name\"}";
    }

    bool success = false;
    // Check if target is a numeric PID
    char* endptr = nullptr;
    long pid = strtol(target.c_str(), &endptr, 10);
    if (*endptr == '\0' && pid > 0) {
        success = mem_reader.attach((int)pid);
    } else {
        success = mem_reader.attach_by_name(target);
    }

    if (success) {
        std::ostringstream oss;
        oss << "{\"status\":\"ok\",\"attached\":true,\"pid\":" << mem_reader.get_pid()
            << ",\"name\":\"" << escape_json(mem_reader.get_process_name()) << "\"}";
        return oss.str();
    } else {
        return "{\"status\":\"error\",\"message\":\"Failed to attach to target: " + escape_json(target) + "\"}";
    }
}

std::string TcpServer::cmd_detach() {
    mem_reader.detach();
    return "{\"status\":\"ok\",\"attached\":false}";
}

std::string TcpServer::cmd_status() {
    std::ostringstream oss;
    oss << "{\"status\":\"ok\""
        << ",\"attached\":" << (mem_reader.is_attached() ? "true" : "false")
        << ",\"pid\":" << mem_reader.get_pid()
        << ",\"name\":\"" << escape_json(mem_reader.get_process_name()) << "\""
        << "}";
    return oss.str();
}

std::string TcpServer::cmd_list_processes() {
    auto procs = MemReader::list_processes();
    std::ostringstream oss;
    oss << "{\"status\":\"ok\",\"count\":" << procs.size() << ",\"processes\":[";
    for (size_t i = 0; i < procs.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{\"pid\":" << procs[i].pid
            << ",\"name\":\"" << escape_json(procs[i].name) << "\""
            << ",\"cmdline\":\"" << escape_json(procs[i].cmdline) << "\"}";
    }
    oss << "]}";
    return oss.str();
}

std::string TcpServer::cmd_get_modules() {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }

    auto modules = mem_reader.get_modules();
    std::ostringstream oss;
    oss << "{\"status\":\"ok\",\"count\":" << modules.size() << ",\"modules\":[";
    for (size_t i = 0; i < modules.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{\"name\":\"" << escape_json(modules[i].name) << "\""
            << ",\"path\":\"" << escape_json(modules[i].path) << "\""
            << ",\"base\":\"0x" << std::hex << modules[i].base_address << "\""
            << ",\"end\":\"0x" << std::hex << modules[i].end_address << "\""
            << ",\"size\":" << std::dec << modules[i].size
            << ",\"perms\":\"" << escape_json(modules[i].permissions) << "\"}";
    }
    oss << "]}";
    return oss.str();
}

std::string TcpServer::cmd_read(uintptr_t addr, size_t size) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }
    if (size == 0 || size > 1024 * 1024) { // Max 1MB per chunk
        return "{\"status\":\"error\",\"message\":\"Invalid read size (1B - 1MB)\"}";
    }

    std::vector<uint8_t> buffer(size);
    if (mem_reader.read(addr, buffer.data(), size)) {
        std::string hex = bytes_to_hex(buffer.data(), size);
        std::ostringstream oss;
        oss << "{\"status\":\"ok\",\"address\":\"0x" << std::hex << addr << "\""
            << ",\"size\":" << std::dec << size
            << ",\"hex\":\"" << hex << "\"}";
        return oss.str();
    } else {
        std::ostringstream oss;
        oss << "{\"status\":\"error\",\"message\":\"Failed to read memory at 0x" << std::hex << addr << "\"}";
        return oss.str();
    }
}

std::string TcpServer::cmd_write(uintptr_t addr, const std::string& hex_data) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }
    if (hex_data.empty() || hex_data.length() % 2 != 0) {
        return "{\"status\":\"error\",\"message\":\"Invalid hex data length\"}";
    }

    std::vector<uint8_t> bytes = hex_to_bytes(hex_data);
    if (mem_reader.write(addr, bytes.data(), bytes.size())) {
        std::ostringstream oss;
        oss << "{\"status\":\"ok\",\"address\":\"0x" << std::hex << addr << "\""
            << ",\"bytes_written\":" << bytes.size() << "}";
        return oss.str();
    } else {
        std::ostringstream oss;
        oss << "{\"status\":\"error\",\"message\":\"Failed to write memory at 0x" << std::hex << addr << "\"}";
        return oss.str();
    }
}

std::string TcpServer::cmd_write_typed(uintptr_t addr, const std::string& type, const std::string& val_str) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }
    if (val_str.empty()) {
        return "{\"status\":\"error\",\"message\":\"Missing value\"}";
    }

    std::string lt = type;
    std::transform(lt.begin(), lt.end(), lt.begin(), ::tolower);

    std::vector<uint8_t> bytes;
    if (lt == "float" || lt == "f32") {
        float v = strtof(val_str.c_str(), nullptr);
        bytes.resize(sizeof(float));
        memcpy(bytes.data(), &v, sizeof(float));
    } else if (lt == "double" || lt == "f64") {
        double v = strtod(val_str.c_str(), nullptr);
        bytes.resize(sizeof(double));
        memcpy(bytes.data(), &v, sizeof(double));
    } else if (lt == "int" || lt == "int32" || lt == "i32") {
        int32_t v = (int32_t)strtol(val_str.c_str(), nullptr, 0);
        bytes.resize(sizeof(int32_t));
        memcpy(bytes.data(), &v, sizeof(int32_t));
    } else if (lt == "uint" || lt == "uint32" || lt == "u32") {
        uint32_t v = (uint32_t)strtoul(val_str.c_str(), nullptr, 0);
        bytes.resize(sizeof(uint32_t));
        memcpy(bytes.data(), &v, sizeof(uint32_t));
    } else if (lt == "int64" || lt == "i64" || lt == "long" || lt == "ptr") {
        uint64_t v = parse_hex_or_dec(val_str);
        bytes.resize(sizeof(uint64_t));
        memcpy(bytes.data(), &v, sizeof(uint64_t));
    } else if (lt == "int16" || lt == "i16" || lt == "short") {
        int16_t v = (int16_t)strtol(val_str.c_str(), nullptr, 0);
        bytes.resize(sizeof(int16_t));
        memcpy(bytes.data(), &v, sizeof(int16_t));
    } else if (lt == "int8" || lt == "i8" || lt == "byte" || lt == "bool") {
        uint8_t v = (uint8_t)strtoul(val_str.c_str(), nullptr, 0);
        bytes.push_back(v);
    } else if (lt == "vec3" || lt == "vector3") {
        std::istringstream viss(val_str);
        float x = 0, y = 0, z = 0;
        viss >> x >> y >> z;
        bytes.resize(12);
        memcpy(bytes.data() + 0, &x, 4);
        memcpy(bytes.data() + 4, &y, 4);
        memcpy(bytes.data() + 8, &z, 4);
    } else if (lt == "string" || lt == "str") {
        bytes.assign(val_str.begin(), val_str.end());
        bytes.push_back(0); // null-terminated
    } else if (lt == "hex") {
        bytes = hex_to_bytes(val_str);
    } else {
        return "{\"status\":\"error\",\"message\":\"Unknown type '" + escape_json(type) + "'. Supported: float, double, int32, uint32, int64, int16, int8, bool, vec3, string, hex\"}";
    }

    if (bytes.empty()) {
        return "{\"status\":\"error\",\"message\":\"No bytes to write\"}";
    }

    if (mem_reader.write(addr, bytes.data(), bytes.size())) {
        std::ostringstream oss;
        oss << "{\"status\":\"ok\",\"address\":\"0x" << std::hex << addr << "\""
            << ",\"type\":\"" << escape_json(type) << "\""
            << ",\"value\":\"" << escape_json(val_str) << "\""
            << ",\"bytes_written\":" << std::dec << bytes.size()
            << ",\"hex_written\":\"" << bytes_to_hex(bytes.data(), bytes.size()) << "\"}";
        return oss.str();
    } else {
        std::ostringstream oss;
        oss << "{\"status\":\"error\",\"message\":\"Failed to write memory at 0x" << std::hex << addr << "\"}";
        return oss.str();
    }
}

std::string TcpServer::cmd_patch(uintptr_t addr, const std::string& hex_patch) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }
    if (hex_patch.empty() || hex_patch.length() % 2 != 0) {
        return "{\"status\":\"error\",\"message\":\"Invalid patch hex length\"}";
    }

    std::vector<uint8_t> patch_bytes = hex_to_bytes(hex_patch);
    std::vector<uint8_t> orig_bytes(patch_bytes.size(), 0);

    // Read original bytes for backup
    if (!mem_reader.read(addr, orig_bytes.data(), orig_bytes.size())) {
        return "{\"status\":\"error\",\"message\":\"Failed to read original bytes before patching at 0x" + std::to_string(addr) + "\"}";
    }

    if (!mem_reader.write(addr, patch_bytes.data(), patch_bytes.size())) {
        return "{\"status\":\"error\",\"message\":\"Failed to apply patch at 0x" + std::to_string(addr) + "\"}";
    }

    std::ostringstream oss;
    oss << "{\"status\":\"ok\",\"address\":\"0x" << std::hex << addr << "\""
        << ",\"patch_hex\":\"" << hex_patch << "\""
        << ",\"orig_hex\":\"" << bytes_to_hex(orig_bytes.data(), orig_bytes.size()) << "\""
        << ",\"bytes_written\":" << std::dec << patch_bytes.size()
        << ",\"message\":\"Patch applied. Use orig_hex to restore.\"}";
    return oss.str();
}

std::string TcpServer::cmd_restore(uintptr_t addr, const std::string& hex_orig) {
    return cmd_write(addr, hex_orig);
}

std::string TcpServer::cmd_read_ptr(uintptr_t base, const std::vector<uintptr_t>& offsets) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }

    std::vector<uintptr_t> chain;
    uintptr_t final_ptr = mem_reader.read_ptr_chain(base, offsets, &chain);

    std::ostringstream oss;
    oss << "{\"status\":\"ok\",\"base\":\"0x" << std::hex << base << "\""
        << ",\"result\":\"0x" << std::hex << final_ptr << "\""
        << ",\"chain\":[";
    for (size_t i = 0; i < chain.size(); i++) {
        if (i > 0) oss << ",";
        oss << "\"0x" << std::hex << chain[i] << "\"";
    }
    oss << "]}";
    return oss.str();
}

std::string TcpServer::cmd_read_string(uintptr_t addr, size_t max_len) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }

    std::string str = mem_reader.read_string(addr, max_len);
    std::ostringstream oss;
    oss << "{\"status\":\"ok\",\"address\":\"0x" << std::hex << addr << "\""
        << ",\"value\":\"" << escape_json(str) << "\"}";
    return oss.str();
}

std::string TcpServer::cmd_scan(uintptr_t start, uintptr_t end, const std::string& pattern) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }

    auto matches = mem_reader.pattern_scan(start, end, pattern);
    std::ostringstream oss;
    oss << "{\"status\":\"ok\",\"count\":" << matches.size() << ",\"matches\":[";
    for (size_t i = 0; i < matches.size(); i++) {
        if (i > 0) oss << ",";
        oss << "\"0x" << std::hex << matches[i] << "\"";
    }
    oss << "]}";
    return oss.str();
}

std::string TcpServer::cmd_scan_mod(const std::string& mod_name, const std::string& pattern) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }

    auto matches = mem_reader.pattern_scan_module(mod_name, pattern);
    std::ostringstream oss;
    oss << "{\"status\":\"ok\",\"module\":\"" << escape_json(mod_name) << "\""
        << ",\"count\":" << matches.size() << ",\"matches\":[";
    for (size_t i = 0; i < matches.size(); i++) {
        if (i > 0) oss << ",";
        oss << "\"0x" << std::hex << matches[i] << "\"";
    }
    oss << "]}";
    return oss.str();
}

std::string TcpServer::cmd_scan_all(const std::string& pattern) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }

    auto matches = mem_reader.pattern_scan_all(pattern);
    std::ostringstream oss;
    oss << "{\"status\":\"ok\""
        << ",\"pattern\":\"" << escape_json(pattern) << "\""
        << ",\"count\":" << matches.size() << ",\"matches\":[";
    for (size_t i = 0; i < matches.size(); i++) {
        if (i > 0) oss << ",";
        oss << "\"0x" << std::hex << matches[i] << "\"";
    }
    oss << "]}";
    return oss.str();
}

std::string TcpServer::cmd_ue4_roots() {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }

    auto roots = mem_reader.get_ue4_roots();
    std::ostringstream oss;
    oss << "{\"status\":\"ok\""
        << ",\"lib_base\":\"0x" << std::hex << roots.lib_base << "\""
        << ",\"fname_pool\":\"0x" << std::hex << roots.fname_pool << "\""
        << ",\"guobject_array\":\"0x" << std::hex << roots.guobject_array << "\""
        << ",\"gworld\":\"0x" << std::hex << roots.gworld << "\""
        << "}";
    return oss.str();
}

std::string TcpServer::cmd_fname(uint32_t index) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }

    std::string name = mem_reader.resolve_fname(index);
    std::ostringstream oss;
    oss << "{\"status\":\"ok\""
        << ",\"index\":" << std::dec << index
        << ",\"name\":\"" << escape_json(name) << "\""
        << "}";
    return oss.str();
}

std::string TcpServer::cmd_uobj(uint32_t index) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }

    uintptr_t uobj_ptr = mem_reader.get_uobject_ptr(index);
    std::string name = mem_reader.get_uobject_name(uobj_ptr);
    std::string class_name = mem_reader.get_uobject_class_name(uobj_ptr);

    std::ostringstream oss;
    oss << "{\"status\":\"ok\""
        << ",\"index\":" << std::dec << index
        << ",\"ptr\":\"0x" << std::hex << uobj_ptr << "\""
        << ",\"name\":\"" << escape_json(name) << "\""
        << ",\"class\":\"" << escape_json(class_name) << "\""
        << "}";
    return oss.str();
}

std::string TcpServer::cmd_actors(uintptr_t gworld, size_t limit) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }

    auto snapshot = mem_reader.get_world_snapshot(gworld, limit);
    auto& actors = snapshot.actors;
    if (actors.empty()) {
        auto fallback = mem_reader.scan_all_actors(limit);
        actors = std::move(fallback);
    }

    std::ostringstream oss;
    oss << "{\"status\":\"ok\""
        << ",\"camera\":{"
        << "\"valid\":" << (snapshot.camera.valid ? "true" : "false")
        << ",\"x\":" << std::fixed << std::setprecision(2) << snapshot.camera.x
        << ",\"y\":" << snapshot.camera.y
        << ",\"z\":" << snapshot.camera.z
        << ",\"pitch\":" << snapshot.camera.pitch
        << ",\"yaw\":" << snapshot.camera.yaw
        << ",\"roll\":" << snapshot.camera.roll
        << ",\"fov\":" << snapshot.camera.fov
        << ",\"local_pawn\":\"0x" << std::hex << snapshot.camera.local_pawn << "\""
        << ",\"local_team\":" << std::dec << snapshot.camera.local_team_id
        << ",\"local_pos\":{\"x\":" << std::fixed << std::setprecision(2) << snapshot.camera.local_x
        << ",\"y\":" << snapshot.camera.local_y
        << ",\"z\":" << snapshot.camera.local_z << "}"
        << "}"
        << ",\"count\":" << std::dec << actors.size()
        << ",\"actors\":[";

    for (size_t i = 0; i < actors.size(); i++) {
        if (i > 0) oss << ",";
        std::string type = "other";
        if (actors[i].is_local_player) type = "local_player";
        else if (actors[i].is_teammate) type = "teammate";
        else if (actors[i].is_bot) type = "bot";
        else if (actors[i].is_player) type = "enemy";
        else if (actors[i].is_item) type = "item";

        oss << "{\"ptr\":\"0x" << std::hex << actors[i].address << "\""
            << ",\"name_id\":" << std::dec << actors[i].name_id
            << ",\"name\":\"" << escape_json(actors[i].name) << "\""
            << ",\"class\":\"" << escape_json(actors[i].class_name) << "\""
            << ",\"type\":\"" << type << "\""
            << ",\"dist\":" << std::fixed << std::setprecision(1) << actors[i].distance
            << ",\"team\":" << std::dec << actors[i].team_id
            << ",\"player_name\":\"" << escape_json(actors[i].player_name) << "\""
            << ",\"is_local\":" << (actors[i].is_local_player ? "true" : "false")
            << ",\"is_teammate\":" << (actors[i].is_teammate ? "true" : "false")
            << ",\"is_bot\":" << (actors[i].is_bot ? "true" : "false")
            << ",\"is_player\":" << (actors[i].is_player ? "true" : "false")
            << ",\"is_item\":" << (actors[i].is_item ? "true" : "false")
            << ",\"root\":\"0x" << std::hex << actors[i].root_component << "\""
            << ",\"pos\":{\"x\":" << std::fixed << std::setprecision(2) << actors[i].x
            << ",\"y\":" << actors[i].y
            << ",\"z\":" << actors[i].z << "}"
            << ",\"head\":{\"x\":" << actors[i].head_x
            << ",\"y\":" << actors[i].head_y
            << ",\"z\":" << actors[i].head_z << "}"
            << "}";
    }
    oss << "]}";
    return oss.str();
}

std::string TcpServer::cmd_inspect_actor(uintptr_t actor_ptr) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }
    if (actor_ptr < 0x100000000ULL) {
        return "{\"status\":\"error\",\"message\":\"Invalid actor pointer address\"}";
    }

    UE4ActorDiagnostic diag = mem_reader.inspect_actor(actor_ptr);

    std::ostringstream oss;
    oss << "{\"status\":\"ok\""
        << ",\"actor_ptr\":\"0x" << std::hex << diag.actor_ptr << "\""
        << ",\"name\":\"" << escape_json(diag.name) << "\""
        << ",\"class\":\"" << escape_json(diag.class_name) << "\""
        << ",\"is_player\":" << (diag.is_player ? "true" : "false")
        << ",\"is_bot\":" << (diag.is_bot ? "true" : "false")
        << ",\"root_component\":{"
        << "\"ptr\":\"0x" << std::hex << diag.root_component << "\""
        << ",\"name\":\"" << escape_json(diag.root_comp_name) << "\""
        << ",\"location\":{\"x\":" << std::fixed << std::setprecision(2) << diag.root_location.x
        << ",\"y\":" << diag.root_location.y
        << ",\"z\":" << diag.root_location.z << "}"
        << ",\"rotation\":{\"pitch\":" << diag.root_rotation.pitch
        << ",\"yaw\":" << diag.root_rotation.yaw
        << ",\"roll\":" << diag.root_rotation.roll << "}"
        << "}"
        << ",\"mesh\":{"
        << "\"ptr\":\"0x" << std::hex << diag.mesh_component << "\""
        << ",\"name\":\"" << escape_json(diag.mesh_comp_name) << "\""
        << ",\"skeletal_mesh\":\"0x" << std::hex << diag.skeletal_mesh << "\""
        << ",\"skeletal_mesh_name\":\"" << escape_json(diag.skeletal_mesh_name) << "\""
        << ",\"bone_transforms_ptr\":\"0x" << std::hex << diag.bone_transforms_ptr << "\""
        << ",\"bone_count\":" << std::dec << diag.bone_count
        << "}"
        << ",\"player_state\":{"
        << "\"ptr\":\"0x" << std::hex << diag.player_state << "\""
        << ",\"name\":\"" << escape_json(diag.player_state_name) << "\""
        << ",\"player_name\":\"" << escape_json(diag.player_name) << "\""
        << ",\"team_id\":" << std::dec << diag.team_id
        << "}"
        << ",\"controller\":{"
        << "\"ptr\":\"0x" << std::hex << diag.controller << "\""
        << ",\"name\":\"" << escape_json(diag.controller_name) << "\""
        << "}"
        << ",\"components_count\":" << std::dec << diag.components.size()
        << ",\"components\":[";

    for (size_t i = 0; i < diag.components.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{\"offset\":\"0x" << std::hex << diag.components[i].offset << "\""
            << ",\"name\":\"" << escape_json(diag.components[i].name) << "\""
            << ",\"class\":\"" << escape_json(diag.components[i].class_name) << "\""
            << ",\"value\":\"" << escape_json(diag.components[i].value_str) << "\""
            << "}";
    }

    oss << "]}";
    return oss.str();
}

std::string TcpServer::cmd_dump_elf(const std::string& mod_name, const std::string& out_path) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }

    std::string err_msg;
    bool success = ElfFixer::dump_and_fix(mem_reader, mod_name, out_path, err_msg);

    if (success) {
        std::ostringstream oss;
        oss << "{\"status\":\"ok\""
            << ",\"module\":\"" << escape_json(mod_name) << "\""
            << ",\"output_path\":\"" << escape_json(out_path) << "\""
            << ",\"message\":\"ELF dumped and reconstructed successfully for IDA Pro!\""
            << "}";
        return oss.str();
    } else {
        std::ostringstream oss;
        oss << "{\"status\":\"error\",\"message\":\"" << escape_json(err_msg) << "\"}";
        return oss.str();
    }
}

std::string TcpServer::cmd_get_logs(size_t limit, int min_lvl) {
    auto logs = Logger::getInstance().get_logs(limit, (LogLevel)min_lvl);
    std::ostringstream oss;
    oss << "{\"status\":\"ok\",\"count\":" << logs.size() << ",\"logs\":[";
    for (size_t i = 0; i < logs.size(); i++) {
        if (i > 0) oss << ",";
        const char* lvl_name = "INFO";
        switch (logs[i].level) {
            case LogLevel::DEBUG_LVL: lvl_name = "DEBUG"; break;
            case LogLevel::INFO_LVL:  lvl_name = "INFO";  break;
            case LogLevel::WARN_LVL:  lvl_name = "WARN";  break;
            case LogLevel::ERROR_LVL: lvl_name = "ERROR"; break;
            case LogLevel::CRIT_LVL:  lvl_name = "CRITICAL"; break;
        }

        oss << "{\"time\":\"" << escape_json(logs[i].time_str) << "\""
            << ",\"level\":\"" << lvl_name << "\""
            << ",\"category\":\"" << escape_json(logs[i].category) << "\""
            << ",\"message\":\"" << escape_json(logs[i].message) << "\""
            << ",\"file\":\"" << escape_json(logs[i].file) << "\""
            << ",\"line\":" << logs[i].line
            << ",\"tid\":" << logs[i].tid
            << ",\"err_code\":" << logs[i].err_code
            << ",\"err_desc\":\"" << escape_json(logs[i].err_desc) << "\"}";
    }
    oss << "]}";
    return oss.str();
}

std::string TcpServer::cmd_clear_logs() {
    Logger::getInstance().clear();
    return "{\"status\":\"ok\",\"message\":\"Diagnostic logs cleared.\"}";
}

std::string TcpServer::cmd_compress(const std::string& in_path, const std::string& out_path, const std::string& format) {
    std::string err_msg;
    ArchiveType type = ArchiveType::ZIP;
    
    std::string fmt_lower = format;
    for (auto& c : fmt_lower) c = tolower(c);

    if (fmt_lower == "tar.gz" || fmt_lower == "tgz" || fmt_lower == "gz") type = ArchiveType::TAR_GZ;
    else if (fmt_lower == "tar.xz" || fmt_lower == "xz") type = ArchiveType::TAR_XZ;
    else if (fmt_lower == "tar") type = ArchiveType::TAR;
    else type = ArchiveType::ZIP;

    bool ok = Compressor::compress(in_path, out_path, type, err_msg);
    std::ostringstream oss;
    if (ok) {
        oss << "{\"status\":\"ok\""
            << ",\"input\":\"" << escape_json(in_path) << "\""
            << ",\"output\":\"" << escape_json(out_path) << "\""
            << ",\"format\":\"" << escape_json(format) << "\""
            << ",\"message\":\"Archive created successfully\""
            << "}";
    } else {
        oss << "{\"status\":\"error\",\"message\":\"" << escape_json(err_msg) << "\"}";
    }
    return oss.str();
}

std::string TcpServer::cmd_decompress(const std::string& archive_path, const std::string& out_dir) {
    std::string err_msg;
    bool ok = Compressor::decompress(archive_path, out_dir, err_msg);
    std::ostringstream oss;
    if (ok) {
        oss << "{\"status\":\"ok\""
            << ",\"archive\":\"" << escape_json(archive_path) << "\""
            << ",\"output_dir\":\"" << escape_json(out_dir) << "\""
            << ",\"message\":\"Archive extracted successfully\""
            << "}";
    } else {
        oss << "{\"status\":\"error\",\"message\":\"" << escape_json(err_msg) << "\"}";
    }
    return oss.str();
}

std::string TcpServer::cmd_fs_list(const std::string& dir_path) {
    std::string err_msg;
    auto entries = FileManager::list_directory(dir_path, err_msg);
    std::ostringstream oss;
    if (!err_msg.empty()) {
        oss << "{\"status\":\"error\",\"message\":\"" << escape_json(err_msg) << "\"}";
    } else {
        oss << "{\"status\":\"ok\",\"path\":\"" << escape_json(dir_path) << "\",\"count\":" << entries.size() << ",\"entries\":[";
        for (size_t i = 0; i < entries.size(); i++) {
            if (i > 0) oss << ",";
            oss << "{\"name\":\"" << escape_json(entries[i].name) << "\""
                << ",\"path\":\"" << escape_json(entries[i].path) << "\""
                << ",\"is_dir\":" << (entries[i].is_dir ? "true" : "false")
                << ",\"size\":" << entries[i].size
                << ",\"mod_time\":" << entries[i].mod_time
                << ",\"perms\":\"" << escape_json(entries[i].permissions) << "\"}";
        }
        oss << "]}";
    }
    return oss.str();
}

std::string TcpServer::cmd_fs_stat(const std::string& file_path) {
    auto info = FileManager::stat_file(file_path);
    std::ostringstream oss;
    if (!info.exists) {
        oss << "{\"status\":\"error\",\"message\":\"File not found: " + escape_json(file_path) + "\"}";
    } else {
        oss << "{\"status\":\"ok\",\"path\":\"" << escape_json(file_path) << "\""
            << ",\"is_dir\":" << (info.is_dir ? "true" : "false")
            << ",\"size\":" << info.size
            << ",\"mod_time\":" << info.mod_time
            << ",\"crc32\":\"0x" << std::hex << info.crc32 << "\"}";
    }
    return oss.str();
}

std::string TcpServer::cmd_fs_read_chunk(const std::string& file_path, uint64_t offset, size_t size) {
    std::string b64, err_msg;
    size_t bytes_read = 0;
    bool is_eof = false;
    bool ok = FileManager::read_chunk(file_path, offset, size, b64, bytes_read, is_eof, err_msg);
    std::ostringstream oss;
    if (ok) {
        oss << "{\"status\":\"ok\",\"path\":\"" << escape_json(file_path) << "\""
            << ",\"offset\":" << offset
            << ",\"bytes_read\":" << bytes_read
            << ",\"is_eof\":" << (is_eof ? "true" : "false")
            << ",\"data_b64\":\"" << b64 << "\"}";
    } else {
        oss << "{\"status\":\"error\",\"message\":\"" << escape_json(err_msg) << "\"}";
    }
    return oss.str();
}

std::string TcpServer::cmd_fs_write_chunk(const std::string& file_path, uint64_t offset, const std::string& b64_data, bool truncate) {
    std::string err_msg;
    size_t bytes_written = 0;
    bool ok = FileManager::write_chunk(file_path, offset, b64_data, truncate, bytes_written, err_msg);
    std::ostringstream oss;
    if (ok) {
        oss << "{\"status\":\"ok\",\"path\":\"" << escape_json(file_path) << "\""
            << ",\"offset\":" << offset
            << ",\"bytes_written\":" << bytes_written << "}";
    } else {
        oss << "{\"status\":\"error\",\"message\":\"" << escape_json(err_msg) << "\"}";
    }
    return oss.str();
}

std::string TcpServer::cmd_fs_delete(const std::string& path, bool recursive) {
    std::string err_msg;
    bool ok = FileManager::delete_item(path, recursive, err_msg);
    std::ostringstream oss;
    if (ok) {
        oss << "{\"status\":\"ok\",\"path\":\"" << escape_json(path) << "\",\"message\":\"Deleted successfully\"}";
    } else {
        oss << "{\"status\":\"error\",\"message\":\"" << escape_json(err_msg) << "\"}";
    }
    return oss.str();
}

std::string TcpServer::cmd_fs_rename(const std::string& old_path, const std::string& new_path) {
    std::string err_msg;
    bool ok = FileManager::rename_item(old_path, new_path, err_msg);
    std::ostringstream oss;
    if (ok) {
        oss << "{\"status\":\"ok\",\"old_path\":\"" << escape_json(old_path) << "\",\"new_path\":\"" << escape_json(new_path) << "\"}";
    } else {
        oss << "{\"status\":\"error\",\"message\":\"" << escape_json(err_msg) << "\"}";
    }
    return oss.str();
}

std::string TcpServer::cmd_fs_mkdir(const std::string& dir_path) {
    std::string err_msg;
    bool ok = FileManager::make_dir(dir_path, err_msg);
    std::ostringstream oss;
    if (ok) {
        oss << "{\"status\":\"ok\",\"path\":\"" << escape_json(dir_path) << "\",\"message\":\"Directory created\"}";
    } else {
        oss << "{\"status\":\"error\",\"message\":\"" << escape_json(err_msg) << "\"}";
    }
    return oss.str();
}

std::string TcpServer::cmd_update_server(const std::string& update_path, const std::string& target_path) {
    std::string err_msg;
    LOG_INFO("UPDATE", "Received hot-update request: candidate '%s' -> target '%s'",
             update_path.c_str(), target_path.c_str());

    // Port is no longer used (Unix socket). Pass 0 to AutoUpdater for compatibility.
    bool ok = AutoUpdater::apply_update(update_path, target_path, 0, err_msg);
    std::ostringstream oss;
    if (ok) {
        oss << "{\"status\":\"ok\""
            << ",\"update_path\":\"" << escape_json(update_path) << "\""
            << ",\"target_path\":\"" << escape_json(target_path) << "\""
            << ",\"message\":\"New binary validated successfully! Handoff initiated to replacement daemon.\"}";
        
        // Return JSON to client, and schedule clean shutdown of current server in 100ms
        // so client receives response before port is handed off to new binary
        LOG_INFO("UPDATE", "Update successful, preparing graceful shutdown of old server process");
    } else {
        oss << "{\"status\":\"error\""
            << ",\"update_path\":\"" << escape_json(update_path) << "\""
            << ",\"message\":\"Validation / Update failed: " << escape_json(err_msg) << ". Previous server process remains ACTIVE and safe.\"}";
    }
    return oss.str();
}

// ─── ARM64 Hook Capture Command Implementations ──────────────────────────────────

std::string TcpServer::cmd_hook_capture(uintptr_t func_addr, uintptr_t capture_buf, const std::string& label) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }
    if (func_addr == 0) {
        return "{\"status\":\"error\",\"message\":\"func_addr required (hex address of target function)\"}";
    }
    if (capture_buf == 0) {
        return "{\"status\":\"error\",\"message\":\"capture_buf required (writable address in target process for register dump)\"}";
    }

    bool ok = mem_reader.hook_capture(func_addr, capture_buf, label);
    std::ostringstream oss;
    if (ok) {
        oss << std::hex;
        oss << "{\"status\":\"ok\""
            << ",\"hook_addr\":\"0x" << func_addr << "\""
            << ",\"capture_buf\":\"0x" << capture_buf << "\""
            << ",\"label\":\"" << escape_json(label) << "\""
            << ",\"message\":\"BRK trampoline placed. Use hook_poll to capture registers when game thread hits the function.\""
            << ",\"capture_buf_layout\":{\"dirty_flag\":\"0x00\",\"X0\":\"0x08\",\"X1\":\"0x10\",\"X2\":\"0x18\",\"X3\":\"0x20\",\"X4\":\"0x28\",\"X5\":\"0x30\",\"X6\":\"0x38\",\"X7\":\"0x40\",\"call_count\":\"0x48\"}"
            << "}";
    } else {
        oss << std::hex;
        oss << "{\"status\":\"error\",\"message\":\"hook_capture failed at 0x" << func_addr
            << ". Check: (1) address in libUE4.so range, (2) memory is writable with root, (3) not already hooked.\"}";
    }
    return oss.str();
}

std::string TcpServer::cmd_hook_restore(uintptr_t func_addr) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }
    bool ok = mem_reader.hook_restore(func_addr);
    std::ostringstream oss;
    if (ok) {
        oss << "{\"status\":\"ok\",\"hook_addr\":\"0x" << std::hex << func_addr
            << "\",\"message\":\"Original bytes restored. Function unhooked.\"}";
    } else {
        oss << "{\"status\":\"error\",\"message\":\"No active hook at 0x" << std::hex << func_addr << "\"}";
    }
    return oss.str();
}

std::string TcpServer::cmd_hook_poll(uintptr_t func_addr) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached to any process\"}";
    }
    HookCaptureResult res = mem_reader.hook_poll(func_addr);
    std::ostringstream oss;
    oss << "{\"status\":\"ok\""
        << ",\"hook_addr\":\"0x" << std::hex << res.hook_addr << "\""
        << ",\"label\":\"" << escape_json(res.label) << "\""
        << ",\"valid\":" << (res.valid ? "true" : "false");
    if (res.valid) {
        oss << ",\"registers\":{";
        for (int i = 0; i < 8; i++) {
            if (i > 0) oss << ",";
            oss << "\"X" << std::dec << i << "\":\"" << res.x_hex[i] << "\"";
        }
        oss << "}";
        oss << ",\"note\":\"Hook auto-restored after capture. Re-hook if needed for next invocation.\"";
    } else {
        oss << ",\"note\":\"No new capture since last poll. Function may not have been called yet, or dirty_flag was already cleared.\"";
    }
    oss << "}";
    return oss.str();
}

// ─── VM::TransformEncrypt Decryption Command Implementations ─────────────────

// set_decrypt_key <algo> <hex_key>
// algo: XOR_FIXED | XOR_NONCE | AES_ECB
std::string TcpServer::cmd_set_decrypt_key(const std::string& algo, const std::string& hex_key) {
    if (algo.empty() || hex_key.empty()) {
        return "{\"status\":\"error\",\"message\":\"Usage: set_decrypt_key <XOR_FIXED|XOR_NONCE|AES_ECB> <hex_key_bytes>\"}";
    }

    auto bytes = hex_to_bytes(hex_key);
    if (bytes.empty() || bytes.size() > 256) {
        return "{\"status\":\"error\",\"message\":\"hex_key must be 2-512 hex chars (1-256 bytes)\"}";
    }

    VMDecryptKey key;
    key.valid   = true;
    key.key_len = (uint32_t)bytes.size();
    memcpy(key.key, bytes.data(), bytes.size());

    std::string algo_upper = algo;
    for (auto& c : algo_upper) c = toupper(c);
    if (algo_upper == "XOR_FIXED" || algo_upper == "XOR")  key.algo = VMDecryptKey::Algo::XOR_FIXED;
    else if (algo_upper == "XOR_NONCE")                    key.algo = VMDecryptKey::Algo::XOR_NONCE;
    else if (algo_upper == "AES_ECB" || algo_upper == "AES") key.algo = VMDecryptKey::Algo::AES_ECB;
    else {
        return "{\"status\":\"error\",\"message\":\"Unknown algo. Use: XOR_FIXED, XOR_NONCE, AES_ECB\"}";
    }

    mem_reader.set_decrypt_key(key);

    std::ostringstream oss;
    oss << "{\"status\":\"ok\""
        << ",\"algo\":\"" << escape_json(key.algo_name()) << "\""
        << ",\"key_len\":" << key.key_len
        << ",\"key_hex\":\"" << hex_key.substr(0, 32) << (hex_key.size() > 32 ? "..." : "") << "\""
        << ",\"message\":\"Decrypt key stored. Now bones/FTransforms will be decrypted in get_bone.\""
        << "}";
    return oss.str();
}

// kpa <cipher_hex_96chars> <tx> <ty> <tz>
// Known-plaintext attack: derive XOR key from encrypted FTransform + known world position
std::string TcpServer::cmd_known_plaintext(const std::string& cipher_hex, float tx, float ty, float tz) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached\"}";
    }
    auto bytes = hex_to_bytes(cipher_hex);
    if (bytes.size() < 48) {
        std::ostringstream err;
        err << "{\"status\":\"error\",\"message\":\"cipher_hex must be at least 96 hex chars (48 bytes). Got "
            << bytes.size() << " bytes.\"}";
        return err.str();
    }

    bool ok = mem_reader.known_plaintext_attack(bytes.data(), tx, ty, tz);
    if (ok) {
        const VMDecryptKey& k = mem_reader.get_decrypt_key();
        std::ostringstream oss;
        oss << "{\"status\":\"ok\""
            << ",\"result\":\"KEY_FOUND\""
            << ",\"algo\":\"" << escape_json(k.algo_name()) << "\""
            << ",\"key_len\":" << k.key_len
            << ",\"key_hex\":\"";
        for (uint32_t i = 0; i < k.key_len; i++) {
            char hb[3]; snprintf(hb, sizeof(hb), "%02x", k.key[i]);
            oss << hb;
        }
        oss << "\""
            << ",\"message\":\"Key recovered via known-plaintext attack. Bones will now decrypt.\""
            << "}";
        return oss.str();
    } else {
        return "{\"status\":\"error\",\"result\":\"KEY_NOT_FOUND\""
               ",\"message\":\"KPA failed sanity check. The encryption may not be simple XOR, "
               "or the known position (tx/ty/tz) doesn't correspond to this cipher block.\"}";
    }
}

// get_bone <skel_mesh_ptr_hex> <bone_idx> [bone_arr_off] [mesh_world_off]
std::string TcpServer::cmd_get_bone(uintptr_t skel_mesh_comp, int bone_idx,
                                     uintptr_t bone_arr_off, uintptr_t mesh_world_off) {
    if (!mem_reader.is_attached()) {
        return "{\"status\":\"error\",\"message\":\"Not attached\"}";
    }
    if (skel_mesh_comp == 0) {
        return "{\"status\":\"error\",\"message\":\"skel_mesh_comp required (ptr to USkeletalMeshComponent)\"}";
    }

    const VMDecryptKey& k = mem_reader.get_decrypt_key();
    UE4BonePos pos = mem_reader.get_bone_world_pos(skel_mesh_comp, bone_idx, bone_arr_off, mesh_world_off);

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "{\"status\":\"ok\""
        << ",\"skel_mesh\":\"0x" << std::hex << skel_mesh_comp << "\""
        << ",\"bone_idx\":" << std::dec << bone_idx
        << ",\"valid\":" << (pos.valid ? "true" : "false")
        << ",\"key_active\":" << (k.valid ? "true" : "false")
        << ",\"algo\":\"" << escape_json(k.algo_name()) << "\"";
    if (pos.valid) {
        oss << ",\"world_pos\":{\"x\":" << std::fixed << pos.wx
            << ",\"y\":" << pos.wy
            << ",\"z\":" << pos.wz << "}"
            << ",\"quat\":{\"x\":" << pos.qx << ",\"y\":" << pos.qy
            << ",\"z\":" << pos.qz << ",\"w\":" << pos.qw << "}";
    } else {
        oss << ",\"note\":\"Bone read failed — check: (1) key loaded, (2) skel ptr valid, "
               "(3) bone_idx in range, (4) BoneArray offset correct (default 0x4A0)\"";
    }
    oss << "}";
    return oss.str();
}

std::string TcpServer::cmd_decrypt_key_status() {
    const VMDecryptKey& k = mem_reader.get_decrypt_key();
    std::ostringstream oss;
    oss << "{\"status\":\"ok\""
        << ",\"key_valid\":" << (k.valid ? "true" : "false")
        << ",\"algo\":\"" << escape_json(k.algo_name()) << "\""
        << ",\"key_len\":" << k.key_len
        << ",\"key_ptr_in_game\":\"0x" << std::hex << k.key_ptr_in_game << "\""
        << ",\"key_hex\":\"";
    for (uint32_t i = 0; i < k.key_len && i < 32; i++) {
        char hb[3]; snprintf(hb, sizeof(hb), "%02x", k.key[i]);
        oss << hb;
    }
    if (k.key_len > 32) oss << "...";
    oss << "\"";
    if (!k.valid) {
        oss << ",\"note\":\"No active key. Run hook_capture + hook_poll to capture from game, "
               "then kpa or set_decrypt_key.\"";
    }
    oss << "}";
    return oss.str();
}

std::string TcpServer::cmd_clear_decrypt_key() {
    mem_reader.clear_decrypt_key();
    return "{\"status\":\"ok\",\"message\":\"Decrypt key cleared. Bones will return raw (encrypted) data.\"}";
}

std::string TcpServer::cmd_hook_list() {
    auto entries = mem_reader.hook_list_all();
    std::ostringstream oss;
    oss << "{\"status\":\"ok\",\"count\":" << entries.size() << ",\"hooks\":[";
    for (size_t i = 0; i < entries.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{\"hook_addr\":\"0x" << std::hex << entries[i].hook_addr << "\""
            << ",\"capture_buf\":\"0x" << std::hex << entries[i].capture_buf << "\""
            << ",\"label\":\"" << escape_json(entries[i].label) << "\""
            << ",\"orig_bytes\":\"";
        for (int j = 0; j < 16; j++) {
            char hb[3];
            snprintf(hb, sizeof(hb), "%02x", entries[i].orig_bytes[j]);
            oss << hb;
        }
        oss << "\"}";
    }
    oss << "]}";
    return oss.str();
}

std::string TcpServer::cmd_set_ue4_config(const std::string& key, const std::string& val) {
    if (key.empty()) {
        return "{\"status\":\"error\",\"message\":\"Missing key. Valid keys: persistent_level, actors, root_comp, mesh, camera, bone_array, comp_to_world, reset\"}";
    }

    UE4DynamicOffsets cfg = mem_reader.get_dynamic_offsets();
    std::string lk = key;
    std::transform(lk.begin(), lk.end(), lk.begin(), ::tolower);
    uint32_t num_val = (uint32_t)parse_hex_or_dec(val);

    if (lk == "reset") {
        mem_reader.reset_dynamic_offsets();
        return "{\"status\":\"ok\",\"message\":\"Dynamic offsets reset to defaults.\"}";
    } else if (lk == "persistent_level" || lk == "pl" || lk == "persistent_level_offset") {
        cfg.persistent_level_offset = num_val;
    } else if (lk == "actors" || lk == "actors_offset" || lk == "actors_array") {
        cfg.actors_offset = num_val;
    } else if (lk == "root_comp" || lk == "root_component" || lk == "root") {
        cfg.root_component_offset = num_val;
    } else if (lk == "mesh" || lk == "mesh_offset") {
        cfg.mesh_offset = num_val;
    } else if (lk == "camera" || lk == "camera_manager" || lk == "pov") {
        cfg.camera_manager_offset = num_val;
    } else if (lk == "bone_array" || lk == "bone" || lk == "bones") {
        cfg.bone_array_offset = num_val;
    } else if (lk == "comp_to_world" || lk == "translation" || lk == "pos") {
        cfg.component_to_world_off = num_val;
    } else if (lk == "bone_decrypt") {
        cfg.enable_bone_decrypt = (val == "1" || val == "true" || val == "yes");
    } else {
        return "{\"status\":\"error\",\"message\":\"Unknown key '" + escape_json(key) + "'. Valid keys: persistent_level, actors, root_comp, mesh, camera, bone_array, comp_to_world, reset\"}";
    }

    mem_reader.set_dynamic_offsets(cfg);
    return "{\"status\":\"ok\",\"message\":\"Offset updated.\",\"key\":\"" + escape_json(key) + "\",\"value\":\"0x" + bytes_to_hex((uint8_t*)&num_val, 4) + "\"}";
}

std::string TcpServer::cmd_get_ue4_config() {
    const UE4DynamicOffsets& cfg = mem_reader.get_dynamic_offsets();
    std::ostringstream oss;
    oss << "{\"status\":\"ok\""
        << ",\"persistent_level\":\"0x" << std::hex << cfg.persistent_level_offset << "\""
        << ",\"actors\":\"0x" << std::hex << cfg.actors_offset << "\""
        << ",\"root_component\":\"0x" << std::hex << cfg.root_component_offset << "\""
        << ",\"mesh\":\"0x" << std::hex << cfg.mesh_offset << "\""
        << ",\"camera_manager\":\"0x" << std::hex << cfg.camera_manager_offset << "\""
        << ",\"player_state\":\"0x" << std::hex << cfg.player_state_offset << "\""
        << ",\"bone_array\":\"0x" << std::hex << cfg.bone_array_offset << "\""
        << ",\"comp_to_world\":\"0x" << std::hex << cfg.component_to_world_off << "\""
        << ",\"bone_decrypt\":" << (cfg.enable_bone_decrypt ? "true" : "false")
        << "}";
    return oss.str();
}

std::string TcpServer::cmd_set_draw_config(const std::string& key, const std::string& val) {
    if (key.empty()) {
        return "{\"status\":\"error\",\"message\":\"Missing key. Valid keys: box, skeleton, snapline, name, distance, health, weapon, radar, fov_circle, ignore_bots, loot, loot_price, min_loot_price, fov_radius, reset\"}";
    }

    ESPDrawConfig cfg = mem_reader.get_draw_config();
    std::string lk = key;
    std::transform(lk.begin(), lk.end(), lk.begin(), ::tolower);
    bool bval = (val == "1" || val == "true" || val == "yes" || val == "on");

    if (lk == "reset") {
        mem_reader.reset_draw_config();
        return "{\"status\":\"ok\",\"message\":\"Draw config reset to defaults.\"}";
    } else if (lk == "box" || lk == "boxes" || lk == "2d_box" || lk == "3d_box") {
        cfg.box = bval;
    } else if (lk == "skeleton" || lk == "bones" || lk == "bone") {
        cfg.skeleton = bval;
    } else if (lk == "snapline" || lk == "line" || lk == "lines" || lk == "ray") {
        cfg.snapline = bval;
    } else if (lk == "name" || lk == "player_name") {
        cfg.name = bval;
    } else if (lk == "distance" || lk == "dist") {
        cfg.distance = bval;
    } else if (lk == "health" || lk == "hp" || lk == "armor") {
        cfg.health = bval;
    } else if (lk == "weapon" || lk == "gun" || lk == "ammo") {
        cfg.weapon = bval;
    } else if (lk == "radar") {
        cfg.radar = bval;
    } else if (lk == "fov" || lk == "fov_circle" || lk == "crosshair") {
        cfg.fov_circle = bval;
    } else if (lk == "ignore_bots" || lk == "players_only") {
        cfg.ignore_bots = bval;
    } else if (lk == "loot" || lk == "items" || lk == "supplies") {
        cfg.loot = bval;
    } else if (lk == "loot_price" || lk == "price") {
        cfg.loot_price = bval;
    } else if (lk == "min_loot_price" || lk == "min_price") {
        cfg.min_loot_price = strtof(val.c_str(), nullptr);
    } else if (lk == "fov_radius" || lk == "radius") {
        cfg.fov_radius = strtof(val.c_str(), nullptr);
    } else {
        return "{\"status\":\"error\",\"message\":\"Unknown draw key '" + escape_json(key) + "'. Valid keys: box, skeleton, snapline, name, distance, health, weapon, radar, fov_circle, ignore_bots, loot, loot_price, min_loot_price, fov_radius, reset\"}";
    }

    mem_reader.set_draw_config(cfg);
    return "{\"status\":\"ok\",\"message\":\"Draw setting updated.\",\"key\":\"" + escape_json(key) + "\",\"value\":\"" + escape_json(val) + "\"}";
}

std::string TcpServer::cmd_get_draw_config() {
    const ESPDrawConfig& cfg = mem_reader.get_draw_config();
    std::ostringstream oss;
    oss << "{\"status\":\"ok\""
        << ",\"box\":" << (cfg.box ? "true" : "false")
        << ",\"skeleton\":" << (cfg.skeleton ? "true" : "false")
        << ",\"snapline\":" << (cfg.snapline ? "true" : "false")
        << ",\"name\":" << (cfg.name ? "true" : "false")
        << ",\"distance\":" << (cfg.distance ? "true" : "false")
        << ",\"health\":" << (cfg.health ? "true" : "false")
        << ",\"weapon\":" << (cfg.weapon ? "true" : "false")
        << ",\"radar\":" << (cfg.radar ? "true" : "false")
        << ",\"fov_circle\":" << (cfg.fov_circle ? "true" : "false")
        << ",\"ignore_bots\":" << (cfg.ignore_bots ? "true" : "false")
        << ",\"loot\":" << (cfg.loot ? "true" : "false")
        << ",\"loot_price\":" << (cfg.loot_price ? "true" : "false")
        << ",\"min_loot_price\":" << cfg.min_loot_price
        << ",\"fov_radius\":" << cfg.fov_radius
        << "}";
    return oss.str();
}


