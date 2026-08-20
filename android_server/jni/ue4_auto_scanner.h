#ifndef UE4_AUTO_SCANNER_H
#define UE4_AUTO_SCANNER_H

#include <stdint.h>
#include <vector>
#include <string>
#include "protocol.h"

class MemReader;

struct UE4AutoScanResult {
    uintptr_t lib_base;
    uintptr_t lib_end;
    uintptr_t fname_pool;
    uintptr_t guobject_array;
    uintptr_t gworld;
    bool fname_pool_found;
    bool guobject_found;
    bool gworld_found;
};

class UE4AutoScanner {
public:
    static UE4AutoScanResult scan(MemReader& reader);

private:
    static uintptr_t decode_adrp(uintptr_t pc, uint32_t insn);
    static uint32_t decode_add_imm12(uint32_t insn);
    static uint32_t decode_ldr_imm(uint32_t insn);
    static uintptr_t resolve_adrp_add(uintptr_t pc, uint32_t adrp_insn, uint32_t add_or_ldr);

    struct PatternByte {
        uint8_t val;
        uint8_t mask;
    };

    static std::vector<PatternByte> parse_pattern(const char* hex);
    static std::vector<uintptr_t> find_pattern(const uint8_t* buf, size_t buf_sz, const std::vector<PatternByte>& pat, uintptr_t base_addr);
    static uintptr_t find_by_pattern_step(MemReader& reader, const std::vector<ModuleInfo>& regions, uintptr_t lib_base, uintptr_t lib_end, const std::vector<std::pair<std::string,int>>& patterns, bool dereference);
};

#endif // UE4_AUTO_SCANNER_H
