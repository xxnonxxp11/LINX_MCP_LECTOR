#include "file_manager.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>

// ─── Base64 Implementation ──────────────────────────────────────────────────

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string FileManager::base64_encode(const unsigned char* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t b = (data[i] << 16) | ((i + 1 < len ? data[i + 1] : 0) << 8) | (i + 2 < len ? data[i + 2] : 0);
        out.push_back(b64_table[(b >> 18) & 0x3F]);
        out.push_back(b64_table[(b >> 12) & 0x3F]);
        out.push_back(i + 1 < len ? b64_table[(b >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < len ? b64_table[b & 0x3F] : '=');
    }
    return out;
}

std::vector<uint8_t> FileManager::base64_decode(const std::string& in) {
    std::vector<uint8_t> out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[(unsigned char)b64_table[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(uint8_t((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

// ─── CRC32 Table ────────────────────────────────────────────────────────────

static uint32_t crc32_for_byte(uint32_t r) {
    for (int j = 0; j < 8; ++j)
        r = (r & 1 ? 0 : (uint32_t)0xEDB88320L) ^ r >> 1;
    return r ^ (uint32_t)0xFF000000L;
}

static uint32_t table[256];
static bool have_table = false;

uint32_t FileManager::calculate_crc32(const std::string& file_path) {
    if (!have_table) {
        for (size_t i = 0; i < 256; ++i) table[i] = crc32_for_byte(i);
        have_table = true;
    }

    FILE* fp = fopen(file_path.c_str(), "rb");
    if (!fp) return 0;

    uint32_t crc = 0;
    unsigned char buf[16384];
    size_t len = 0;
    while ((len = fread(buf, 1, sizeof(buf), fp)) > 0) {
        for (size_t i = 0; i < len; ++i) {
            crc = table[(uint8_t)crc ^ buf[i]] ^ crc >> 8;
        }
    }
    fclose(fp);
    return crc;
}

// ─── Directory & File Operations ────────────────────────────────────────────

std::vector<FileEntryInfo> FileManager::list_directory(const std::string& dir_path, std::string& err_msg) {
    std::vector<FileEntryInfo> entries;
    DIR* d = opendir(dir_path.c_str());
    if (!d) {
        err_msg = "Cannot open directory: " + dir_path;
        LOG_ERROR("FS", "%s", err_msg.c_str());
        return entries;
    }

    struct dirent* dir;
    while ((dir = readdir(d)) != nullptr) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

        std::string full_path = dir_path;
        if (!full_path.empty() && full_path.back() != '/') full_path += '/';
        full_path += dir->d_name;

        struct stat st;
        if (stat(full_path.c_str(), &st) == 0) {
            FileEntryInfo info;
            info.name = dir->d_name;
            info.path = full_path;
            info.is_dir = S_ISDIR(st.st_mode);
            info.size = info.is_dir ? 0 : (uint64_t)st.st_size;
            info.mod_time = (uint64_t)st.st_mtime;

            char perms[10] = "---------";
            if (st.st_mode & S_IRUSR) perms[0] = 'r';
            if (st.st_mode & S_IWUSR) perms[1] = 'w';
            if (st.st_mode & S_IXUSR) perms[2] = 'x';
            if (st.st_mode & S_IRGRP) perms[3] = 'r';
            if (st.st_mode & S_IWGRP) perms[4] = 'w';
            if (st.st_mode & S_IXGRP) perms[5] = 'x';
            if (st.st_mode & S_IROTH) perms[6] = 'r';
            if (st.st_mode & S_IWOTH) perms[7] = 'w';
            if (st.st_mode & S_IXOTH) perms[8] = 'x';
            info.permissions = perms;

            entries.push_back(info);
        }
    }
    closedir(d);

    // Sort: directories first, then alphabetically
    std::sort(entries.begin(), entries.end(), [](const FileEntryInfo& a, const FileEntryInfo& b) {
        if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
        return a.name < b.name;
    });

    LOG_INFO("FS", "Listed directory %s (%zu items)", dir_path.c_str(), entries.size());
    return entries;
}

FileStatInfo FileManager::stat_file(const std::string& file_path) {
    FileStatInfo info = {};
    struct stat st;
    if (stat(file_path.c_str(), &st) == 0) {
        info.exists = true;
        info.is_dir = S_ISDIR(st.st_mode);
        info.size = (uint64_t)st.st_size;
        info.mod_time = (uint64_t)st.st_mtime;
        if (!info.is_dir && info.size < 50 * 1024 * 1024) {
            info.crc32 = calculate_crc32(file_path);
        }
    }
    return info;
}

bool FileManager::read_chunk(const std::string& file_path, uint64_t offset, size_t size, std::string& out_base64, size_t& bytes_read, bool& is_eof, std::string& err_msg) {
    FILE* fp = fopen(file_path.c_str(), "rb");
    if (!fp) {
        err_msg = "Cannot open file for reading: " + file_path;
        LOG_ERROR("FS_READ", "%s", err_msg.c_str());
        return false;
    }

    if (fseeko(fp, (off_t)offset, SEEK_SET) != 0) {
        err_msg = "Failed to seek to offset " + std::to_string(offset);
        LOG_ERROR("FS_READ", "%s", err_msg.c_str());
        fclose(fp);
        return false;
    }

    std::vector<unsigned char> buf(size);
    bytes_read = fread(buf.data(), 1, size, fp);
    is_eof = feof(fp) != 0 || bytes_read < size;

    fclose(fp);

    out_base64 = base64_encode(buf.data(), bytes_read);
    return true;
}

bool FileManager::write_chunk(const std::string& file_path, uint64_t offset, const std::string& data_base64, bool truncate_first, size_t& bytes_written, std::string& err_msg) {
    auto data = base64_decode(data_base64);
    if (data.empty() && !data_base64.empty()) {
        err_msg = "Invalid base64 payload";
        LOG_ERROR("FS_WRITE", "%s", err_msg.c_str());
        return false;
    }

    // Automatically create parent directory if missing
    size_t last_slash = file_path.rfind('/');
    if (last_slash != std::string::npos) {
        std::string parent = file_path.substr(0, last_slash);
        std::string dummy;
        make_dir(parent, dummy);
    }

    const char* mode = truncate_first ? "wb" : (offset == 0 ? "wb" : "r+b");
    FILE* fp = fopen(file_path.c_str(), mode);
    if (!fp && !truncate_first) {
        fp = fopen(file_path.c_str(), "wb");
    }

    if (!fp) {
        err_msg = "Cannot open file for writing: " + file_path;
        LOG_ERROR("FS_WRITE", "%s", err_msg.c_str());
        return false;
    }

    if (offset > 0) {
        fseeko(fp, (off_t)offset, SEEK_SET);
    }

    bytes_written = data.empty() ? 0 : fwrite(data.data(), 1, data.size(), fp);
    fclose(fp);

    if (bytes_written != data.size()) {
        err_msg = "Incomplete write: wrote " + std::to_string(bytes_written) + " of " + std::to_string(data.size());
        LOG_ERROR("FS_WRITE", "%s", err_msg.c_str());
        return false;
    }

    return true;
}

bool FileManager::delete_item(const std::string& path, bool recursive, std::string& err_msg) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        err_msg = "Target path does not exist: " + path;
        return false;
    }

    if (S_ISDIR(st.st_mode)) {
        if (recursive) {
            std::string cmd = "rm -rf \"" + path + "\" 2>&1";
            int ret = system(cmd.c_str());
            if (ret != 0) {
                err_msg = "Recursive delete failed";
                return false;
            }
        } else {
            if (rmdir(path.c_str()) != 0) {
                err_msg = "Failed to remove directory (may not be empty)";
                return false;
            }
        }
    } else {
        if (unlink(path.c_str()) != 0) {
            err_msg = "Failed to unlink file: " + path;
            return false;
        }
    }
    LOG_INFO("FS", "Deleted item: %s", path.c_str());
    return true;
}

bool FileManager::rename_item(const std::string& old_path, const std::string& new_path, std::string& err_msg) {
    if (rename(old_path.c_str(), new_path.c_str()) != 0) {
        err_msg = "Failed to rename " + old_path + " to " + new_path;
        LOG_ERROR("FS", "%s", err_msg.c_str());
        return false;
    }
    LOG_INFO("FS", "Renamed %s -> %s", old_path.c_str(), new_path.c_str());
    return true;
}

bool FileManager::make_dir(const std::string& dir_path, std::string& err_msg) {
    std::string cmd = "mkdir -p \"" + dir_path + "\"";
    int ret = system(cmd.c_str());
    if (ret != 0) {
        err_msg = "Failed to create directory: " + dir_path;
        LOG_ERROR("FS", "%s", err_msg.c_str());
        return false;
    }
    LOG_INFO("FS", "Created directory: %s", dir_path.c_str());
    return true;
}
