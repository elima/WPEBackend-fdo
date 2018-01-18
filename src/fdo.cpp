#include <wpe/loader.h>

#include <cstdio>
#include <cstring>

#include "interfaces.h"

extern "C" {

__attribute__((visibility("default")))
struct wpe_loader_interface _wpe_loader_interface = {
    [](const char* object_name) -> void* {
        if (!std::strcmp(object_name, "_wpe_renderer_host_interface"))
            return &fdo_renderer_host;

        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_interface"))
            return &fdo_renderer_backend_egl;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_target_interface"))
            return &fdo_renderer_backend_egl_target;

        if (!std::strcmp(object_name, "_wpe_renderer_backend_vulkan_interface"))
            return &fdo_renderer_backend_vulkan;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_vulkan_target_interface"))
            return &fdo_renderer_backend_vulkan_target;

        return nullptr;
    },
};

}
