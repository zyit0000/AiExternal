#include "dumper.hpp"
#include <thread>
#include <chrono>

extern "C" __attribute__((visibility("default"))) void __attribute__((constructor)) InitDumper() {
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        uintptr_t image_base = 0;
        for (uint32_t i = 0; i < _dyld_image_count(); i++) {
            const char* name = _dyld_get_image_name(i);
            if (name && (strstr(name, "RobloxPlayer") || strstr(name, "RobloxPlayerBeta"))) {
                image_base = (uintptr_t)_dyld_get_image_header(i);
                break;
            }
        }

        if (!image_base) return;

        task_t self = mach_task_self();
        dumper::DumperContext context(self);
        
        auto dm = dumper::find_datamodel(self, image_base);
        if (dm) {
            std::println("Found DataModel: {:#x}", *dm);
            // Here you could call find_studio_offsets if you have the live instances
        }

        context.print_found_offsets();
    }).detach();
}
