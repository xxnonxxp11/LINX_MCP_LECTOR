#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <string>
#include <vector>

enum class ArchiveType {
    ZIP,
    TAR,
    TAR_GZ,
    TAR_XZ
};

class Compressor {
public:
    static bool compress(const std::string& input_path, const std::string& output_archive, ArchiveType type, std::string& err_msg);
    static bool decompress(const std::string& archive_path, const std::string& output_dir, std::string& err_msg);
    static bool create_zip(const std::string& input_path, const std::string& output_zip, std::string& err_msg);
    static bool create_tar(const std::string& input_path, const std::string& output_tar, bool gzip, std::string& err_msg);
};

#endif // COMPRESSOR_H
