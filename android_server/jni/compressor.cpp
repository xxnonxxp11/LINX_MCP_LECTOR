#include "compressor.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>

bool Compressor::create_zip(const std::string& input_path, const std::string& output_zip, std::string& err_msg) {
    return compress(input_path, output_zip, ArchiveType::ZIP, err_msg);
}

bool Compressor::create_tar(const std::string& input_path, const std::string& output_tar, bool gzip, std::string& err_msg) {
    return compress(input_path, output_tar, gzip ? ArchiveType::TAR_GZ : ArchiveType::TAR, err_msg);
}

bool Compressor::compress(const std::string& input_path, const std::string& output_archive, ArchiveType type, std::string& err_msg) {
    struct stat st;
    if (stat(input_path.c_str(), &st) != 0) {
        err_msg = "Input path does not exist: " + input_path;
        LOG_ERROR("COMPRESS", "%s", err_msg.c_str());
        return false;
    }

    std::string cmd;
    switch (type) {
        case ArchiveType::ZIP:
            // Prefer zip if available, fallback to toybox zip or tar
            cmd = "zip -r -q \"" + output_archive + "\" \"" + input_path + "\" 2>&1";
            break;
        case ArchiveType::TAR:
            cmd = "tar -cf \"" + output_archive + "\" -C \"$(dirname \"" + input_path + "\")\" \"$(basename \"" + input_path + "\")\" 2>&1";
            break;
        case ArchiveType::TAR_GZ:
            cmd = "tar -czf \"" + output_archive + "\" -C \"$(dirname \"" + input_path + "\")\" \"$(basename \"" + input_path + "\")\" 2>&1";
            break;
        case ArchiveType::TAR_XZ:
            cmd = "tar -cJf \"" + output_archive + "\" -C \"$(dirname \"" + input_path + "\")\" \"$(basename \"" + input_path + "\")\" 2>&1";
            break;
    }

    LOG_INFO("COMPRESS", "Executing command: %s", cmd.c_str());

    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) {
        err_msg = "Failed to spawn compression process";
        LOG_ERROR("COMPRESS", "%s", err_msg.c_str());
        return false;
    }

    char buf[512];
    std::string output;
    while (fgets(buf, sizeof(buf), fp)) {
        output += buf;
    }
    int status = pclose(fp);

    if (status != 0) {
        // If zip utility wasn't found on Android, fallback to tar.gz
        if (type == ArchiveType::ZIP && output.find("not found") != std::string::npos) {
            LOG_WARN("COMPRESS", "zip binary not found on device, attempting fallback with tar -czf...");
            std::string fallback_tar = output_archive + ".tar.gz";
            return compress(input_path, fallback_tar, ArchiveType::TAR_GZ, err_msg);
        }

        err_msg = "Compression failed (status " + std::to_string(status) + "): " + output;
        LOG_ERROR("COMPRESS", "%s", err_msg.c_str());
        return false;
    }

    if (stat(output_archive.c_str(), &st) == 0) {
        LOG_INFO("COMPRESS", "Archive created successfully: %s (Size: %zu bytes)", output_archive.c_str(), (size_t)st.st_size);
        return true;
    } else {
        err_msg = "Archive was not created: " + output;
        LOG_ERROR("COMPRESS", "%s", err_msg.c_str());
        return false;
    }
}

bool Compressor::decompress(const std::string& archive_path, const std::string& output_dir, std::string& err_msg) {
    struct stat st;
    if (stat(archive_path.c_str(), &st) != 0) {
        err_msg = "Archive file not found: " + archive_path;
        LOG_ERROR("DECOMPRESS", "%s", err_msg.c_str());
        return false;
    }

    std::string cmd;
    if (archive_path.rfind(".zip") != std::string::npos) {
        cmd = "unzip -q -o \"" + archive_path + "\" -d \"" + output_dir + "\" 2>&1";
    } else if (archive_path.rfind(".tar.gz") != std::string::npos || archive_path.rfind(".tgz") != std::string::npos) {
        cmd = "tar -xzf \"" + archive_path + "\" -C \"" + output_dir + "\" 2>&1";
    } else if (archive_path.rfind(".tar.xz") != std::string::npos) {
        cmd = "tar -xJf \"" + archive_path + "\" -C \"" + output_dir + "\" 2>&1";
    } else if (archive_path.rfind(".tar") != std::string::npos) {
        cmd = "tar -xf \"" + archive_path + "\" -C \"" + output_dir + "\" 2>&1";
    } else {
        cmd = "tar -xf \"" + archive_path + "\" -C \"" + output_dir + "\" 2>&1";
    }

    LOG_INFO("DECOMPRESS", "Executing command: %s", cmd.c_str());

    // Ensure output dir exists
    std::string mkdir_cmd = "mkdir -p \"" + output_dir + "\"";
    system(mkdir_cmd.c_str());

    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) {
        err_msg = "Failed to spawn decompression process";
        LOG_ERROR("DECOMPRESS", "%s", err_msg.c_str());
        return false;
    }

    char buf[512];
    std::string output;
    while (fgets(buf, sizeof(buf), fp)) {
        output += buf;
    }
    int status = pclose(fp);

    if (status != 0) {
        err_msg = "Decompression failed (status " + std::to_string(status) + "): " + output;
        LOG_ERROR("DECOMPRESS", "%s", err_msg.c_str());
        return false;
    }

    LOG_INFO("DECOMPRESS", "Extracted archive %s to %s", archive_path.c_str(), output_dir.c_str());
    return true;
}
