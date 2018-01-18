#include "interfaces.h"

#include "renderer-backend-wlclient.h"
#include <cstring>
#include <gio/gio.h>

#include <cstdio>

namespace {

class Backend {
public:
    Backend(int hostFd)
    {
        WLClient::Backend::connect(hostFd);
        auto* wlBackend = WLClient::Backend::singleton();
        if (wlBackend)
            m_nativeDisplay = static_cast<EGLNativeDisplayType>(wlBackend->display());
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

    EGLNativeDisplayType nativeDisplay() const { return m_nativeDisplay; }

private:
    EGLNativeDisplayType m_nativeDisplay { nullptr };
};

class Target {
public:
    Target(struct wpe_renderer_backend_egl_target* target, int hostFd)
        : m_target(target)
    {
        m_socket = g_socket_new_from_fd(hostFd, nullptr);
        if (m_socket)
            g_socket_set_blocking(m_socket, FALSE);
    }

    ~Target()
    {
        if (m_socket)
            g_object_unref(m_socket);
        if (m_source) {
            g_source_destroy(m_source);
            g_source_unref(m_source);
        }
    }

    void initialize(Backend& backend, uint32_t width, uint32_t height)
    {
        m_source = g_source_new(&s_sourceFuncs, sizeof(GSource));
        g_source_set_priority(m_source, -70);
        g_source_set_name(m_source, "WPEBackend-fdo::Target");
        g_source_set_callback(m_source, [](gpointer userData) -> gboolean {
            auto* target = static_cast<Target*>(userData);
            wpe_renderer_backend_egl_target_dispatch_frame_complete(target->m_target);
            return G_SOURCE_CONTINUE;
        }, this, nullptr);
        g_source_attach(m_source, g_main_context_get_thread_default());

        m_surface = wl_compositor_create_surface(backend.compositor());
        m_window = wl_egl_window_create(m_surface, width, height);
        wl_display_roundtrip(backend.display());

        uint32_t message[] = { 0x42, wl_proxy_get_id(reinterpret_cast<struct wl_proxy*>(m_surface)) };
        if (m_socket)
            g_socket_send(m_socket, reinterpret_cast<gchar*>(message), 2 * sizeof(uint32_t),
                nullptr, nullptr);
    }

    void requestFrame()
    {
        struct wl_callback* callback = wl_surface_frame(m_surface);
        wl_callback_add_listener(callback, &s_callbackListener, this);
    }

    void dispatchFrameComplete()
    {
        g_source_set_ready_time(m_source, 0);
    }

    struct wl_egl_window* window() const { return m_window; }

private:
    static const struct wl_callback_listener s_callbackListener;
    static GSourceFuncs s_sourceFuncs;

    struct wpe_renderer_backend_egl_target* m_target { nullptr };
    GSource* m_source { nullptr };
    GSocket* m_socket { nullptr };

    struct wl_surface* m_surface { nullptr };
    struct wl_egl_window* m_window { nullptr };
};

const struct wl_callback_listener Target::s_callbackListener = {
    // done
    [](void* data, struct wl_callback* callback, uint32_t time)
    {
        static_cast<Target*>(data)->dispatchFrameComplete();

        wl_callback_destroy(callback);
    },
};

GSourceFuncs Target::s_sourceFuncs = {
    nullptr, // prepare
    nullptr, // check
    // dispatch
    [](GSource* source, GSourceFunc callback, gpointer userData) -> gboolean
    {
        if (g_source_get_ready_time(source) == -1)
            return G_SOURCE_CONTINUE;
        g_source_set_ready_time(source, -1);
        return callback(userData);
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

} // namespace

struct wpe_renderer_backend_egl_interface fdo_renderer_backend_egl = {
    // create
    [](int host_fd) -> void*
    {
        return new Backend(host_fd);
    },
    // destroy
    [](void* data)
    {
        auto* backend = reinterpret_cast<Backend*>(data);
        delete backend;
    },
    // get_native_display
    [](void* data) -> EGLNativeDisplayType
    {
        auto& backend = *reinterpret_cast<Backend*>(data);
        return backend.nativeDisplay();
    },
};

struct wpe_renderer_backend_egl_target_interface fdo_renderer_backend_egl_target = {
    // create
    [](struct wpe_renderer_backend_egl_target* target, int host_fd) -> void*
    {
        return new Target(target, host_fd);
    },
    // destroy
    [](void* data)
    {
        auto* target = reinterpret_cast<Target*>(data);
        delete target;
    },
    // initialize
    [](void* data, void* backend_data, uint32_t width, uint32_t height)
    {
        auto& target = *reinterpret_cast<Target*>(data);
        auto& backend = *reinterpret_cast<Backend*>(backend_data);
        target.initialize(backend, width, height);
    },
    // get_native_window
    [](void* data) -> EGLNativeWindowType
    {
        auto& target = *reinterpret_cast<Target*>(data);
        return target.window();
    },
    // resize
    [](void* data, uint32_t width, uint32_t height)
    {
    },
    // frame_will_render
    [](void* data)
    {
        auto& target = *reinterpret_cast<Target*>(data);
        target.requestFrame();
    },
    // frame_rendered
    [](void* data)
    {
    },
};
