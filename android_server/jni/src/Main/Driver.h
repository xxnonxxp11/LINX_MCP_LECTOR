#pragma once
#include <sys/uio.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <iostream>

// ─── ARM64 Syscall Numbers ───────────────────────────────────
#if defined(__aarch64__)
#  define SYS_vm_readv  270
#  define SYS_vm_writev 271
#elif defined(__arm__)
#  define SYS_vm_readv  376
#  define SYS_vm_writev 377
#else
#  define SYS_vm_readv  SYS_process_vm_readv
#  define SYS_vm_writev SYS_process_vm_writev
#endif

static inline ssize_t _vm_readv(pid_t pid,
                                 const struct iovec* lv, unsigned long lc,
                                 const struct iovec* rv, unsigned long rc,
                                 unsigned long flags) {
    return syscall(SYS_vm_readv, pid, lv, lc, rv, rc, flags);
}

static inline ssize_t _vm_writev(pid_t pid,
                                  const struct iovec* lv, unsigned long lc,
                                  const struct iovec* rv, unsigned long rc,
                                  unsigned long flags) {
    return syscall(SYS_vm_writev, pid, lv, lc, rv, rc, flags);
}

enum class ReadMethod { PROC_MEM, SYSCALL, LIBC_VM, NONE };

class c_mem_driver {
private:
    pid_t      m_pid  = 0;
    int        m_fd   = -1;
    ReadMethod m_meth = ReadMethod::NONE;

    bool _read_procmem(uintptr_t addr, void* buf, size_t sz) const {
        if (m_fd < 0) return false;
        if (lseek64(m_fd, (off64_t)addr, SEEK_SET) < 0) return false;
        ssize_t r = ::read(m_fd, buf, sz);
        return r == (ssize_t)sz;
    }

    bool _read_syscall(uintptr_t addr, void* buf, size_t sz) const {
        struct iovec lv = { buf,         sz };
        struct iovec rv = { (void*)addr, sz };
        return _vm_readv(m_pid, &lv, 1, &rv, 1, 0) == (ssize_t)sz;
    }

    bool _read_libc(uintptr_t addr, void* buf, size_t sz) const {
        struct iovec lv = { buf,         sz };
        struct iovec rv = { (void*)addr, sz };
        return ::_vm_readv(m_pid, &lv, 1, &rv, 1, 0) == (ssize_t)sz;
    }

    ReadMethod _detect() {
        if (m_fd >= 0) {
            if (lseek64(m_fd, 0, SEEK_SET) >= 0 || errno != EBADF)
                return ReadMethod::PROC_MEM;
        }

        uint8_t probe[8]{};
        struct iovec lv = { probe, 8 };
        struct iovec rv = { (void*)0x1000UL, 8 };

        errno = 0;
        _vm_readv(m_pid, &lv, 1, &rv, 1, 0);
        if (errno != EPERM && errno != ENOSYS) return ReadMethod::SYSCALL;

        errno = 0;
        ::_vm_readv(m_pid, &lv, 1, &rv, 1, 0);
        if (errno != EPERM && errno != ENOSYS) return ReadMethod::LIBC_VM;

        return ReadMethod::NONE;
    }

public:
    c_mem_driver() = default;
    ~c_mem_driver() { if (m_fd >= 0) close(m_fd); }

    bool init(pid_t pid) {
        if (m_fd >= 0) {
            close(m_fd);
            m_fd = -1;
        }
        m_pid = pid;
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/mem", pid);
        m_fd   = open(path, O_RDONLY | O_LARGEFILE);
        m_meth = _detect();
        return m_meth != ReadMethod::NONE;
    }

    bool read(uintptr_t addr, void* buf, size_t sz) const {
        if (!addr || !buf || !sz) return false;
        switch (m_meth) {
            case ReadMethod::SYSCALL:
                if (_read_syscall(addr, buf, sz)) return true;
                [[fallthrough]];
            case ReadMethod::LIBC_VM:
                if (_read_libc(addr, buf, sz))    return true;
                [[fallthrough]];
            case ReadMethod::PROC_MEM:
                return _read_procmem(addr, buf, sz);
            default:
                return false;
        }
    }

    template<typename T>
    T read(uintptr_t addr) const {
        T val{};
        read(addr, &val, sizeof(T));
        return val;
    }

    bool write(uintptr_t addr, const void* buf, size_t sz) const {
        struct iovec lv = { const_cast<void*>(buf), sz };
        struct iovec rv = { (void*)addr, sz };
        return _vm_writev(m_pid, &lv, 1, &rv, 1, 0) == (ssize_t)sz;
    }

    template<typename T>
    bool write(uintptr_t addr, T val) const {
        return write(addr, &val, sizeof(T));
    }

    pid_t pid() const { return m_pid; }
    bool  ok()  const { return m_meth != ReadMethod::NONE; }
};

inline pid_t find_target_pid(const char* pkg) {
    DIR* d = opendir("/proc");
    if (!d) return 0;
    struct dirent* e;
    pid_t result = 0;
    while ((e = readdir(d))) {
        pid_t pid = (pid_t)atoi(e->d_name);
        if (pid <= 0) continue;

        // 1. Match package name in cmdline
        char p[64]; snprintf(p, sizeof(p), "/proc/%d/cmdline", pid);
        FILE* f = fopen(p, "r"); if (!f) continue;
        char buf[512]{}; fread(buf, 1, sizeof(buf)-1, f); fclose(f);
        if (!strstr(buf, pkg)) continue;

        // 2. Verify libUE4.so is mapped in maps
        char maps_p[64]; snprintf(maps_p, sizeof(maps_p), "/proc/%d/maps", pid);
        FILE* mf = fopen(maps_p, "r");
        if (!mf) continue;
        char line[512];
        bool has_ue4 = false;
        while (fgets(line, sizeof(line), mf)) {
            if (strstr(line, "libUE4.so")) { has_ue4 = true; break; }
        }
        fclose(mf);
        if (!has_ue4) continue;

        // 3. Confirm /proc/pid/mem is openable
        char mem_p[64]; snprintf(mem_p, sizeof(mem_p), "/proc/%d/mem", pid);
        int fd_test = open(mem_p, O_RDONLY | O_LARGEFILE);
        if (fd_test < 0) continue;
        close(fd_test);

        result = pid;
        break;
    }
    closedir(d);
    return result;
}

inline uintptr_t get_target_module_base(pid_t pid, const char* mod) {
    char p[64]; snprintf(p, sizeof(p), "/proc/%d/maps", pid);
    FILE* f = fopen(p, "r"); if (!f) return 0;
    char line[512]; uintptr_t base = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, mod)) {
            base = (uintptr_t)strtoull(line, nullptr, 16);
            break;
        }
    }
    fclose(f);
    return base;
}

class c_driver {
private:
    c_mem_driver m_mem;
    pid_t        m_pid = 0;

public:
    c_driver() = default;
    ~c_driver() = default;

    void initialize(pid_t pid) {
        this->m_pid = pid;
        m_mem.init(pid);
        printf("[✓] Driver Nativo (Kernel 4.14.199 /proc/mem + syscall) inicializado para PID %d\n", pid);
    }

    bool read(uintptr_t addr, void* buffer, size_t size) {
        return m_mem.read(addr, buffer, size);
    }

    template <typename T>
    T read(uintptr_t addr) {
        return m_mem.read<T>(addr);
    }

    bool write(uintptr_t addr, const void* buffer, size_t size) {
        return m_mem.write(addr, buffer, size);
    }

    template <typename T>
    bool write(uintptr_t addr, T value) {
        return m_mem.write<T>(addr, value);
    }

    pid_t get_name_pid(const char* name) {
        pid_t p = find_target_pid(name);
        if (p <= 0) {
            // Fallback to standard pidof if needed
            FILE* fp;
            char cmd[0x100] = "pidof ";
            strcat(cmd, name);
            fp = popen(cmd, "r");
            if (fp) {
                fscanf(fp, "%d", &p);
                pclose(fp);
            }
        }
        return p;
    }

    uintptr_t get_module_base(const char* name) {
        return get_target_module_base(this->m_pid, name);
    }
};

static c_driver* driver = new c_driver();

typedef char PACKAGENAME;
extern pid_t pid;