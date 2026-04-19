#pragma once
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <optional>
#include <string_view>

namespace macho {
    struct SectionInfo {
        uintptr_t address;
        size_t size;
    };

    inline std::optional<SectionInfo> get_section(task_t task, uintptr_t base, std::string_view seg, std::string_view sect) {
        unsigned long size;
        uintptr_t addr = (uintptr_t)getsectiondata((const struct mach_header_64*)base, seg.data(), sect.data(), &size);
        if (addr) return SectionInfo{addr, size};
        return std::nullopt;
    }
}
