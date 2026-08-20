#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <vector>
#include <stdint.h>

struct FileEntryInfo {
    std::string name;
    std::string path;
    bool is_dir;
    uint64_t size;
    uint64_t mod_time;
    std::string permissions;
};

struct FileStatInfo {
    bool exists;
    bool is_dir;
    uint64_t size;
    uint64_t mod_time;
    std::string permissions;
    uint32_t crc32;
};

class FileManager {
public:
    static std::vector<FileEntryInfo> list_directory(const std::string& dir_path, std::string& err_msg);
    static FileStatInfo stat_file(const std::string& file_path);
    static bool read_chunk(const std::string& file_path, uint64_t offset, size_t size, std::string& out_base64, size_t& bytes_read, bool& is_eof, std::string& err_msg);
    static bool write_chunk(const std::string& file_path, uint64_t offset, const std::string& data_base64, bool truncate_first, size_t& bytes_written, std::string& err_msg);
    static bool delete_item(const std::string& path, bool recursive, std::string& err_msg);
    static bool rename_item(const std::string& old_path, const std::string& new_path, std::string& err_msg);
    static bool make_dir(const std::string& dir_path, std::string& err_msg);
    static uint32_t calculate_crc32(const std::string& file_path);

    // Base64 Helpers
    static std::string base64_encode(const unsigned char* data, size_t len);
    static std::vector<uint8_t> base64_decode(const std::string& in);
};

#endif // FILE_MANAGER_H
