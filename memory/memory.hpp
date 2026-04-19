#pragma once
#include <mach/mach.h>
#include <string>
#include <vector>

namespace memory {
    // Injected-friendly memory reading (direct access)
    template <typename T>
    inline bool read_value(task_t task, uintptr_t address, T& out) {
        if (address < 0x10000) return false;
        try {
            out = *reinterpret_cast<T*>(address);
            return true;
        } catch (...) { return false; }
    }

    inline bool read_bytes(task_t task, uintptr_t address, void* buffer, size_t size) {
        if (address < 0x10000) return false;
        try {
            memcpy(buffer, reinterpret_cast<void*>(address), size);
            return true;
        } catch (...) { return false; }
    }

    inline bool read_cstring(task_t task, uintptr_t address, std::string& out, size_t max_size) {
        if (address < 0x10000) return false;
        try {
            const char* str = reinterpret_cast<const char*>(address);
            out = std::string(str).substr(0, max_size);
            return true;
        } catch (...) { return false; }
    }
}
