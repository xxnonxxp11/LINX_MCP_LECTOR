#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <string>
#include <vector>

struct ModuleInfo {
    std::string name;
    std::string path;
    uintptr_t base_address;
    uintptr_t end_address;
    size_t size;
    std::string permissions;
};

struct ProcessInfo {
    int pid;
    std::string name;
    std::string cmdline;
};

#endif // PROTOCOL_H
