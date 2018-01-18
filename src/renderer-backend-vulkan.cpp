#define VK_USE_PLATFORM_WAYLAND_KHR
#include "interfaces.h"

#include "renderer-backend-wlclient.h"
#include <cstring>
#include <gio/gio.h>
#include <glib.h>

#include <cstdio>

namespace {

class Backend {
public:
    Backend(int hostFd)
    {
        WLClient::Backend::connect(hostFd);
        auto* wlBackend = WLClient::Backend::singleton();
        if (wlBackend) {
            VkApplicationInfo applicationInfo {
                VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr,
                "WPEBackend-fdo", 1, 
                nullptr, 0,
                VK_MAKE_VERSION(1, 0, 2),
            };
            const char* extensionNames[2] = {
                VK_KHR_SURFACE_EXTENSION_NAME,
                VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
            };
            VkInstanceCreateInfo instanceCreateInfo {
                VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr,
                0, &applicationInfo,
                0, nullptr,
                2, extensionNames,
            };
            VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &m_vkInstance);
        }
    }

    ~Backend()
    {
        WLClient::Backend::invalidate();
    }

    struct wl_display* display() const
    {
        auto* wlBackend = WLClient::Backend::singleton();
        if (!wlBackend)
            return nullptr;
        return wlBackend->display();
    }

    struct wl_compositor* compositor() const
    {
        auto* wlBackend = WLClient::Backend::singleton();
        if (!wlBackend)
            return nullptr;
        return wlBackend->compositor();
    }

    VkInstance vkInstance() const { return m_vkInstance; }

private:
    VkInstance m_vkInstance { nullptr };
};

class Target {
public:
    Target(int hostFd)
    {
        m_socket = g_socket_new_from_fd(hostFd, nullptr);
        if (m_socket)
            g_socket_set_blocking(m_socket, FALSE);
    }

    ~Target()
    {
        if (m_socket)
            g_object_unref(m_socket);
    }

    void initialize(Backend& backend, uint32_t width, uint32_t height)
    {
        fprintf(stderr, "Target::initialize() (%u,%u)\n", width, height);

        m_wlSurface = wl_compositor_create_surface(backend.compositor());
        fprintf(stderr, "\tcreated surface %p id %u\n",
            m_wlSurface, wl_proxy_get_id((struct wl_proxy*)m_wlSurface));

        VkInstance instance = backend.vkInstance();
        PFN_vkCreateWaylandSurfaceKHR createWaylandSurfaceKHR =
            (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR");
        fprintf(stderr, "\t createWaylandSurfaceKHR %p\n", createWaylandSurfaceKHR);

        VkWaylandSurfaceCreateInfoKHR surfaceCreateInfoKHR {
            VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR, nullptr,
            0, backend.display(), m_wlSurface,
        };
        VkResult result = createWaylandSurfaceKHR(instance, &surfaceCreateInfoKHR, nullptr, &m_vkSurface);
        fprintf(stderr, "\tresult %d wsiSurface %p\n", result, m_vkSurface);
        wl_display_roundtrip(backend.display());

        uint32_t message[] = { 0x42, wl_proxy_get_id(reinterpret_cast<struct wl_proxy*>(m_wlSurface)) };
        if (m_socket)
            g_socket_send(m_socket, reinterpret_cast<gchar*>(message), 2 * sizeof(uint32_t),
                nullptr, nullptr);
    }

    VkSurfaceKHR vkSurface() const { return m_vkSurface; }

private:
    GSocket* m_socket;

    struct wl_surface* m_wlSurface;
    VkSurfaceKHR m_vkSurface;
};

} // namespace

struct wpe_renderer_backend_vulkan_interface fdo_renderer_backend_vulkan = {
    // create
    [](int host_fd) -> void*
    {
        fprintf(stderr, "fdo_renderer_backend_vulkan::create()\n");
        return new Backend(host_fd);
    },
    // destroy
    [](void* data)
    {
        fprintf(stderr, "fdo_renderer_backend_vulkan::destroy()\n");
        auto* backend = reinterpret_cast<Backend*>(data);
        delete backend;
    },
    // get_instance
    [](void* data) -> VkInstance
    {
        fprintf(stderr, "fdo_renderer_backend_vulkan::get_instance()\n");
        auto& backend = *reinterpret_cast<Backend*>(data);
        return backend.vkInstance();
    },
};

struct wpe_renderer_backend_vulkan_target_interface fdo_renderer_backend_vulkan_target = {
    // create
    [](struct wpe_renderer_backend_vulkan_target*, int host_fd) -> void*
    {
        fprintf(stderr, "fdo_renderer_backend_vulkan_target::create()\n");
        return new Target(host_fd);
    },
    // destroy
    [](void* data)
    {
        fprintf(stderr, "fdo_renderer_backend_vulkan_target::destroy()\n");
        auto* target = reinterpret_cast<Target*>(data);
        delete target;
    },
    // initialize
    [](void* data, void* backend_data, uint32_t width, uint32_t height)
    {
        fprintf(stderr, "fdo_renderer_backend_vulkan_target::initialize()\n");
        auto& target = *reinterpret_cast<Target*>(data);
        auto& backend = *reinterpret_cast<Backend*>(backend_data);
        target.initialize(backend, width, height);
    },
    // get_surface
    [](void* data) -> VkSurfaceKHR
    {
        fprintf(stderr, "fdo_renderer_backend_vulkan_target::get_surface()\n");
        auto& target = *reinterpret_cast<Target*>(data);
        return target.vkSurface();
    },
};
