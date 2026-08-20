#include "ue4_auto_scanner.h"
#include "mem_reader.h"
#include <stdio.h>
#include <string.h>
#include <algorithm>

// ─── ARM64 ADRP / ADD / LDR Decoders ────────────────────────────────────────

uintptr_t UE4AutoScanner::decode_adrp(uintptr_t pc, uint32_t insn) {
    if ((insn & 0x9F000000) != 0x90000000) return 0;
    int64_t immlo = (insn >> 29) & 0x3;
    int64_t immhi = (insn >> 5)  & 0x7FFFF;
    int64_t imm   = (immhi << 2) | immlo;
    if (imm & (1LL << 20)) imm |= ~((1LL << 21) - 1);
    imm <<= 12;
    return (uintptr_t)((int64_t)(pc & ~0xFFFULL) + imm);
}

uint32_t UE4AutoScanner::decode_add_imm12(uint32_t insn) {
    if ((insn & 0xFF000000) != 0x91000000) return ~0u;
    uint32_t imm12 = (insn >> 10) & 0xFFF;
    uint32_t shift = (insn >> 22) & 0x3;
    return imm12 << (shift == 1 ? 12 : 0);
}

uint32_t UE4AutoScanner::decode_ldr_imm(uint32_t insn) {
    if ((insn & 0xFFC00000) != 0xF9400000) return ~0u;
    return ((insn >> 10) & 0xFFF) * 8;
}

uintptr_t UE4AutoScanner::resolve_adrp_add(uintptr_t pc, uint32_t adrp_insn, uint32_t add_or_ldr) {
    uintptr_t page = decode_adrp(pc, adrp_insn);
    if (!page) return 0;
    uint32_t off = decode_add_imm12(add_or_ldr);
    if (off == ~0u) off = decode_ldr_imm(add_or_ldr);
    if (off == ~0u) return 0;
    return page + off;
}

std::vector<UE4AutoScanner::PatternByte> UE4AutoScanner::parse_pattern(const char* hex) {
    std::vector<PatternByte> out;
    while (*hex) {
        while (*hex == ' ') hex++;
        if (!*hex) break;
        if (hex[0] == '?') {
            out.push_back({0, 0});
            hex++; if (*hex == '?') hex++;
        } else {
            auto hv = [](char c) -> uint8_t {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return 0;
            };
            uint8_t v = (hv(hex[0]) << 4) | hv(hex[1]);
            out.push_back({v, 0xFF});
            hex += 2;
        }
    }
    return out;
}

std::vector<uintptr_t> UE4AutoScanner::find_pattern(const uint8_t* buf, size_t buf_sz, const std::vector<PatternByte>& pat, uintptr_t base_addr) {
    std::vector<uintptr_t> hits;
    if (pat.empty() || buf_sz < pat.size()) return hits;
    size_t end = buf_sz - pat.size();
    for (size_t i = 0; i <= end; i++) {
        bool ok = true;
        for (size_t j = 0; j < pat.size(); j++) {
            if (pat[j].mask && (buf[i+j] & pat[j].mask) != pat[j].val) {
                ok = false; break;
            }
        }
        if (ok) hits.push_back(base_addr + i);
    }
    return hits;
}

uintptr_t UE4AutoScanner::find_by_pattern_step(
    MemReader& reader,
    const std::vector<ModuleInfo>& regions,
    uintptr_t lib_base,
    uintptr_t lib_end,
    const std::vector<std::pair<std::string,int>>& patterns,
    bool dereference)
{
    for (const auto& r : regions) {
        if (r.permissions.find('x') == std::string::npos) continue;
        size_t sz = r.size;
        if (sz < 16) continue;

        const size_t CHUNK = 8ULL * 1024 * 1024; // 8MB chunks
        for (size_t chunk_off = 0; chunk_off < sz; chunk_off += CHUNK) {
            size_t to_read = (chunk_off + CHUNK > sz) ? sz - chunk_off : CHUNK;
            std::vector<uint8_t> code(to_read, 0);

            if (!reader.read(r.base_address + chunk_off, code.data(), to_read)) {
                continue;
            }

            uintptr_t chunk_base = r.base_address + chunk_off;

            for (const auto& [pat_str, step] : patterns) {
                auto pat = parse_pattern(pat_str.c_str());
                if (pat.empty()) continue;

                auto hits = find_pattern(code.data(), to_read, pat, chunk_base);
                if (hits.empty()) continue;

                for (uintptr_t hit : hits) {
                    uintptr_t adrp_addr = (uintptr_t)((int64_t)hit + (int64_t)step);
                    if (adrp_addr < chunk_base || adrp_addr >= chunk_base + to_read - 8)
                        continue;

                    size_t adrp_off = adrp_addr - chunk_base;
                    uint32_t insn_a, insn_b;
                    memcpy(&insn_a, code.data() + adrp_off,     4);
                    memcpy(&insn_b, code.data() + adrp_off + 4, 4);

                    uintptr_t resolved = resolve_adrp_add(adrp_addr, insn_a, insn_b);
                    if (!resolved) continue;

                    if (resolved < lib_base || resolved >= lib_end + 0x10000000ULL) continue;

                    if (dereference) {
                        uintptr_t val = reader.read_val<uintptr_t>(resolved);
                        if (!val) continue;
                        return resolved;
                    }
                    return resolved;
                }
            }
        }
    }
    return 0;
}

// ─── Pattern Sets ───────────────────────────────────────────────────────────

static const std::vector<std::pair<std::string,int>> FNAME_PATTERNS = {
    {"F4 4F 01 A9 FD 7B 02 A9 FD 83 00 91 ?? ?? ?? ?? ?? ?? ?? ?? A8 02 ?? 39", 0x18},
    {"F4 4F 01 A9 FD 7B 02 A9 FD 83 00 91 ?? ?? ?? ?? A8 02 ?? 39",             0x24},
    {"fd 7b 01 a9 fd 43 00 91 ?? ?? ?? ?? 89 ?? ?? 39 f3 03 08 aa c9 00 00 37 ?? ?? ?? ?? ?? ?? ?? 91", 0x18},
    {"f8 c8 ?? ?? 39 c8 00 00 37 ?? ?? ?? ?? ?? ?? ?? 91",                       9},
    {"02 ?? 91 C8 00 00 37 ?? ?? ?? ?? ?? ?? ?? 91",                             7},
    {"39 C8 00 00 37 ?? ?? ?? ?? ?? ?? ?? 91 ?? ?? ?? 97 ?? 00 80 52 ?? ?? ?? 39", 5},
    {"C8 00 00 37 ?? ?? ?? ?? ?? ?? ?? 91 ?? ?? ?? 97 ?? 00 80 52",              4},
    {"C8 00 00 37 ?? ?? ?? ?? ?? ?? ?? 91 ?? ?? ?? 97",                          4},
};

static const std::vector<std::pair<std::string,int>> GUOA_PATTERNS = {
    {"91 E1 03 ?? AA E0 03 08 AA E2 03 1F 2A",                                  -7},
    {"B4 21 0C 40 B9 ?? ?? ?? ?? ?? ?? ?? 91",                                   5},
    {"96 df 02 17 ?? ?? ?? ?? 54 ?? ?? ?? ?? ?? ?? ?? 91 e1 03 13 aa",           9},
    {"f4 03 01 2a ?? 00 00 34 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 00 54 ?? 00 00 14 ?? ?? ?? ?? ?? ?? ?? 91", 0x18},
    {"69 3e 40 b9 1f 01 09 6b ?? ?? ?? 54 e1 03 13 aa ?? ?? ?? ?? f4 4f ?? a9 ?? ?? ?? ?? ?? ?? ?? 91", 0x18},
};

static const std::vector<std::pair<std::string,int>> GWORLD_PATTERNS = {
    {"08 3D 40 F9 00 01 3F D6 E8 03 13 AA ?? ?? ?? F9",  0x18},
    {"FD ?? ?? A9 28 ?? ?? F9 F3 ?? ?? F8 C0 03 5F D6",  -0x1c},
    {"00 01 3F D6 E8 03 13 AA 60 ?? 00 F9 ?? ?? ?? A9",  14},
};

UE4AutoScanResult UE4AutoScanner::scan(MemReader& reader) {
    UE4AutoScanResult res = {};
    auto modules = reader.get_modules();

    std::vector<ModuleInfo> ue4_regions;
    for (const auto& mod : modules) {
        if (mod.name.find("libUE4.so") != std::string::npos || mod.path.find("libUE4.so") != std::string::npos) {
            ue4_regions.push_back(mod);
            if (res.lib_base == 0 || mod.base_address < res.lib_base) {
                res.lib_base = mod.base_address;
            }
            if (mod.end_address > res.lib_end) {
                res.lib_end = mod.end_address;
            }
        }
    }

    if (res.lib_base == 0) return res;

    // Scan FNamePool
    res.fname_pool = find_by_pattern_step(reader, ue4_regions, res.lib_base, res.lib_end, FNAME_PATTERNS, true);
    if (res.fname_pool) res.fname_pool_found = true;

    // Scan GUObjectArray
    res.guobject_array = find_by_pattern_step(reader, ue4_regions, res.lib_base, res.lib_end, GUOA_PATTERNS, true);
    if (res.guobject_array) res.guobject_found = true;

    // Scan GWorld
    res.gworld = find_by_pattern_step(reader, ue4_regions, res.lib_base, res.lib_end, GWORLD_PATTERNS, true);
    if (res.gworld) res.gworld_found = true;

    return res;
}
