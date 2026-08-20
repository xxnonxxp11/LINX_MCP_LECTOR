#include "elf_fixer.h"
#include "mem_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <map>

struct SectionBuilder {
    std::string name;
    Elf64_Word type;
    Elf64_Xword flags;
    Elf64_Addr addr;
    Elf64_Off offset;
    Elf64_Xword size;
    Elf64_Word link;
    Elf64_Word info;
    Elf64_Xword addralign;
    Elf64_Xword entsize;
};

bool ElfFixer::dump_and_fix(MemReader& reader, const std::string& module_name, const std::string& output_file, std::string& err_msg) {
    auto modules = reader.get_modules();
    ModuleInfo target_mod;
    bool found = false;

    for (const auto& mod : modules) {
        if (mod.name == module_name || mod.name.find(module_name) != std::string::npos || mod.path.find(module_name) != std::string::npos) {
            target_mod = mod;
            found = true;
            break;
        }
    }

    if (!found) {
        err_msg = "Module " + module_name + " not found in process memory maps.";
        return false;
    }

    uintptr_t base_addr = target_mod.base_address;

    // Step 1: Read and validate ELF Header
    Elf64_Ehdr ehdr;
    if (!reader.read(base_addr, &ehdr, sizeof(ehdr))) {
        err_msg = "Failed to read ELF Header at base address.";
        return false;
    }

    if (ehdr.e_ident[0] != 0x7F || ehdr.e_ident[1] != 'E' || ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') {
        err_msg = "Invalid ELF magic header.";
        return false;
    }

    if (ehdr.e_ident[4] != 2) { // 2 = 64-bit ELF
        err_msg = "Only 64-bit (AArch64) ELF supported.";
        return false;
    }

    // Step 2: Read Program Headers
    if (ehdr.e_phnum == 0 || ehdr.e_phoff == 0) {
        err_msg = "No program headers found.";
        return false;
    }

    std::vector<Elf64_Phdr> phdrs(ehdr.e_phnum);
    if (!reader.read(base_addr + ehdr.e_phoff, phdrs.data(), ehdr.e_phnum * sizeof(Elf64_Phdr))) {
        err_msg = "Failed to read Program Headers.";
        return false;
    }

    // Step 3: Calculate full image size from PT_LOAD segments
    uint64_t max_memsz = 0;
    Elf64_Phdr* dynamic_phdr = nullptr;

    for (auto& ph : phdrs) {
        if (ph.p_type == PT_LOAD) {
            uint64_t end = ph.p_vaddr + ph.p_memsz;
            if (end > max_memsz) max_memsz = end;
        } else if (ph.p_type == PT_DYNAMIC) {
            dynamic_phdr = &ph;
        }
    }

    if (max_memsz == 0) {
        max_memsz = target_mod.size;
    }

    // Safety clamp (max 512 MB)
    if (max_memsz > 512 * 1024 * 1024) {
        max_memsz = 512 * 1024 * 1024;
    }

    // Step 4: Dump entire memory image into buffer
    std::vector<uint8_t> dump_buf(max_memsz, 0);

    const size_t CHUNK_SIZE = 64 * 1024; // 64 KB per chunk
    for (uint64_t offset = 0; offset < max_memsz; offset += CHUNK_SIZE) {
        size_t to_read = std::min((size_t)(max_memsz - offset), CHUNK_SIZE);
        reader.read(base_addr + offset, dump_buf.data() + offset, to_read);
    }

    // Step 5: Fix Program Headers in memory buffer
    Elf64_Ehdr* out_ehdr = (Elf64_Ehdr*)dump_buf.data();
    Elf64_Phdr* out_phdrs = (Elf64_Phdr*)(dump_buf.data() + out_ehdr->e_phoff);

    for (int i = 0; i < out_ehdr->e_phnum; i++) {
        out_phdrs[i].p_offset = out_phdrs[i].p_vaddr;
        out_phdrs[i].p_filesz = out_phdrs[i].p_memsz;
    }

    // Step 6: Parse PT_DYNAMIC to reconstruct Sections (.dynsym, .dynstr, .hash, .rel.dyn, .rel.plt, etc.)
    std::vector<SectionBuilder> sections;
    
    // Section 0: NULL Section
    SectionBuilder null_sec = {};
    sections.push_back(null_sec);

    std::string shstrtab = "\0"; // .shstrtab string pool
    auto add_shstr = [&](const std::string& name) -> Elf64_Word {
        Elf64_Word offset = (Elf64_Word)shstrtab.length();
        shstrtab += name;
        shstrtab.push_back('\0');
        return offset;
    };

    uintptr_t dynamic_addr = 0;
    size_t dynamic_sz = 0;

    if (dynamic_phdr) {
        dynamic_addr = dynamic_phdr->p_vaddr;
        dynamic_sz = dynamic_phdr->p_memsz;
    }

    uintptr_t dt_strtab = 0;
    uint64_t dt_strsz = 0;
    uintptr_t dt_symtab = 0;
    uint64_t dt_syment = sizeof(uint64_t) * 3;
    uintptr_t dt_hash = 0;
    uintptr_t dt_gnu_hash = 0;
    uintptr_t dt_rela = 0;
    uint64_t dt_relasz = 0;
    uintptr_t dt_jmprel = 0;
    uint64_t dt_pltrelsz = 0;
    uintptr_t dt_init_array = 0;
    uint64_t dt_init_arraysz = 0;
    uintptr_t dt_fini_array = 0;
    uint64_t dt_fini_arraysz = 0;

    if (dynamic_addr > 0 && dynamic_addr + dynamic_sz <= dump_buf.size()) {
        Elf64_Dyn* dyn = (Elf64_Dyn*)(dump_buf.data() + dynamic_addr);
        for (size_t i = 0; i < dynamic_sz / sizeof(Elf64_Dyn); i++) {
            if (dyn[i].d_tag == DT_NULL) break;

            switch (dyn[i].d_tag) {
                case DT_STRTAB: dt_strtab = dyn[i].d_un.d_ptr >= base_addr ? dyn[i].d_un.d_ptr - base_addr : dyn[i].d_un.d_ptr; break;
                case DT_STRSZ: dt_strsz = dyn[i].d_un.d_val; break;
                case DT_SYMTAB: dt_symtab = dyn[i].d_un.d_ptr >= base_addr ? dyn[i].d_un.d_ptr - base_addr : dyn[i].d_un.d_ptr; break;
                case DT_SYMENT: dt_syment = dyn[i].d_un.d_val; break;
                case DT_HASH: dt_hash = dyn[i].d_un.d_ptr >= base_addr ? dyn[i].d_un.d_ptr - base_addr : dyn[i].d_un.d_ptr; break;
                case DT_GNU_HASH: dt_gnu_hash = dyn[i].d_un.d_ptr >= base_addr ? dyn[i].d_un.d_ptr - base_addr : dyn[i].d_un.d_ptr; break;
                case DT_RELA: dt_rela = dyn[i].d_un.d_ptr >= base_addr ? dyn[i].d_un.d_ptr - base_addr : dyn[i].d_un.d_ptr; break;
                case DT_RELASZ: dt_relasz = dyn[i].d_un.d_val; break;
                case DT_JMPREL: dt_jmprel = dyn[i].d_un.d_ptr >= base_addr ? dyn[i].d_un.d_ptr - base_addr : dyn[i].d_un.d_ptr; break;
                case DT_PLTRELSZ: dt_pltrelsz = dyn[i].d_un.d_val; break;
                case DT_INIT_ARRAY: dt_init_array = dyn[i].d_un.d_ptr >= base_addr ? dyn[i].d_un.d_ptr - base_addr : dyn[i].d_un.d_ptr; break;
                case DT_INIT_ARRAYSZ: dt_init_arraysz = dyn[i].d_un.d_val; break;
                case DT_FINI_ARRAY: dt_fini_array = dyn[i].d_un.d_ptr >= base_addr ? dyn[i].d_un.d_ptr - base_addr : dyn[i].d_un.d_ptr; break;
                case DT_FINI_ARRAYSZ: dt_fini_arraysz = dyn[i].d_un.d_val; break;
            }
        }
    }

    // Fix Relative Relocations in memory dump (subtract base address from relocations)
    if (dt_rela > 0 && dt_relasz > 0 && dt_rela + dt_relasz <= dump_buf.size()) {
        Elf64_Rela* rela = (Elf64_Rela*)(dump_buf.data() + dt_rela);
        size_t count = dt_relasz / sizeof(Elf64_Rela);
        for (size_t i = 0; i < count; i++) {
            if ((rela[i].r_info & 0xFFFFFFFF) == R_AARCH64_RELATIVE) {
                uintptr_t target_offset = rela[i].r_offset >= base_addr ? rela[i].r_offset - base_addr : rela[i].r_offset;
                if (target_offset + 8 <= dump_buf.size()) {
                    uint64_t* val_ptr = (uint64_t*)(dump_buf.data() + target_offset);
                    if (*val_ptr >= base_addr) {
                        *val_ptr -= base_addr; // Re-normalize pointer offset for IDA Pro
                    }
                }
            }
        }
    }

    // Build standard sections for IDA Pro
    if (dt_strtab > 0 && dt_strsz > 0) {
        SectionBuilder s;
        s.name = ".dynstr";
        s.type = SHT_STRTAB;
        s.flags = 0x2; // SHF_ALLOC
        s.addr = dt_strtab;
        s.offset = dt_strtab;
        s.size = dt_strsz;
        s.link = 0; s.info = 0; s.addralign = 1; s.entsize = 0;
        sections.push_back(s);
    }

    if (dt_symtab > 0) {
        SectionBuilder s;
        s.name = ".dynsym";
        s.type = SHT_DYNSYM;
        s.flags = 0x2; // SHF_ALLOC
        s.addr = dt_symtab;
        s.offset = dt_symtab;
        s.size = (dt_strtab > dt_symtab) ? (dt_strtab - dt_symtab) : 0x10000;
        s.link = 1; s.info = 1; s.addralign = 8; s.entsize = 24;
        sections.push_back(s);
    }

    if (dt_hash > 0) {
        SectionBuilder s;
        s.name = ".hash";
        s.type = SHT_HASH;
        s.flags = 0x2;
        s.addr = dt_hash;
        s.offset = dt_hash;
        s.size = 0x1000;
        s.link = 2; s.info = 0; s.addralign = 8; s.entsize = 4;
        sections.push_back(s);
    }

    if (dt_gnu_hash > 0) {
        SectionBuilder s;
        s.name = ".gnu.hash";
        s.type = SHT_GNU_HASH;
        s.flags = 0x2;
        s.addr = dt_gnu_hash;
        s.offset = dt_gnu_hash;
        s.size = 0x1000;
        s.link = 2; s.info = 0; s.addralign = 8; s.entsize = 0;
        sections.push_back(s);
    }

    if (dt_rela > 0 && dt_relasz > 0) {
        SectionBuilder s;
        s.name = ".rela.dyn";
        s.type = SHT_RELA;
        s.flags = 0x2;
        s.addr = dt_rela;
        s.offset = dt_rela;
        s.size = dt_relasz;
        s.link = 2; s.info = 0; s.addralign = 8; s.entsize = sizeof(Elf64_Rela);
        sections.push_back(s);
    }

    if (dt_jmprel > 0 && dt_pltrelsz > 0) {
        SectionBuilder s;
        s.name = ".rela.plt";
        s.type = SHT_RELA;
        s.flags = 0x2;
        s.addr = dt_jmprel;
        s.offset = dt_jmprel;
        s.size = dt_pltrelsz;
        s.link = 2; s.info = 0; s.addralign = 8; s.entsize = sizeof(Elf64_Rela);
        sections.push_back(s);
    }

    if (dt_init_array > 0 && dt_init_arraysz > 0) {
        SectionBuilder s;
        s.name = ".init_array";
        s.type = SHT_INIT_ARRAY;
        s.flags = 0x3; // SHF_WRITE | SHF_ALLOC
        s.addr = dt_init_array;
        s.offset = dt_init_array;
        s.size = dt_init_arraysz;
        s.link = 0; s.info = 0; s.addralign = 8; s.entsize = 8;
        sections.push_back(s);
    }

    if (dt_fini_array > 0 && dt_fini_arraysz > 0) {
        SectionBuilder s;
        s.name = ".fini_array";
        s.type = SHT_FINI_ARRAY;
        s.flags = 0x3;
        s.addr = dt_fini_array;
        s.offset = dt_fini_array;
        s.size = dt_fini_arraysz;
        s.link = 0; s.info = 0; s.addralign = 8; s.entsize = 8;
        sections.push_back(s);
    }

    if (dynamic_addr > 0 && dynamic_sz > 0) {
        SectionBuilder s;
        s.name = ".dynamic";
        s.type = SHT_DYNAMIC;
        s.flags = 0x3;
        s.addr = dynamic_addr;
        s.offset = dynamic_addr;
        s.size = dynamic_sz;
        s.link = 1; s.info = 0; s.addralign = 8; s.entsize = sizeof(Elf64_Dyn);
        sections.push_back(s);
    }

    // Append .shstrtab section
    SectionBuilder shstr_sec;
    shstr_sec.name = ".shstrtab";
    shstr_sec.type = SHT_STRTAB;
    shstr_sec.flags = 0;
    shstr_sec.addr = 0;
    shstr_sec.offset = dump_buf.size();
    shstr_sec.size = shstrtab.length();
    shstr_sec.link = 0; shstr_sec.info = 0; shstr_sec.addralign = 1; shstr_sec.entsize = 0;
    sections.push_back(shstr_sec);

    Elf64_Half shstrndx = (Elf64_Half)(sections.size() - 1);

    // Build raw Section Headers
    std::vector<Elf64_Shdr> shdrs(sections.size());
    for (size_t i = 0; i < sections.size(); i++) {
        shdrs[i].sh_name = (i == 0) ? 0 : add_shstr(sections[i].name);
        shdrs[i].sh_type = sections[i].type;
        shdrs[i].sh_flags = sections[i].flags;
        shdrs[i].sh_addr = sections[i].addr;
        shdrs[i].sh_offset = sections[i].offset;
        shdrs[i].sh_size = sections[i].size;
        shdrs[i].sh_link = sections[i].link;
        shdrs[i].sh_info = sections[i].info;
        shdrs[i].sh_addralign = sections[i].addralign;
        shdrs[i].sh_entsize = sections[i].entsize;
    }

    // Update shstr_sec offset & size now that all names are populated
    shdrs[shstrndx].sh_offset = dump_buf.size();
    shdrs[shstrndx].sh_size = shstrtab.length();

    // Append .shstrtab data
    dump_buf.insert(dump_buf.end(), shstrtab.begin(), shstrtab.end());

    // Align to 8 bytes for Section Header Table
    while (dump_buf.size() % 8 != 0) dump_buf.push_back(0);

    uint64_t shoff = dump_buf.size();
    uint8_t* shdrs_bytes = (uint8_t*)shdrs.data();
    dump_buf.insert(dump_buf.end(), shdrs_bytes, shdrs_bytes + (shdrs.size() * sizeof(Elf64_Shdr)));

    // Step 7: Update ELF Header with Section table offsets
    out_ehdr = (Elf64_Ehdr*)dump_buf.data();
    out_ehdr->e_shoff = shoff;
    out_ehdr->e_shentsize = sizeof(Elf64_Shdr);
    out_ehdr->e_shnum = (Elf64_Half)shdrs.size();
    out_ehdr->e_shstrndx = shstrndx;

    // Step 8: Save fixed ELF file
    FILE* out_file = fopen(output_file.c_str(), "wb");
    if (!out_file) {
        err_msg = "Failed to open output file for writing: " + output_file;
        return false;
    }

    fwrite(dump_buf.data(), 1, dump_buf.size(), out_file);
    fclose(out_file);

    return true;
}
