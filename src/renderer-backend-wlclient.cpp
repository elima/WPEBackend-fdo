#include "renderer-backend-wlclient.h"

#include <cstdio>
#include <cstring>

namespace WLClient {

struct Source {
    static GSourceFuncs s_sourceFuncs;

    GSource source;
    GPollFD pfd;
    struct wl_display* display;
};

GSourceFuncs Source::s_sourceFuncs = {
    // prepare
    [](GSource* base, gint* timeout) -> gboolean
    {
        auto& source = *reinterpret_cast<Source*>(base);
        *timeout = -1;
        wl_display_dispatch_pending(source.display);
        wl_display_flush(source.display);
        return FALSE;
    },
    // check
    [](GSource* base) -> gboolean
    {
        auto& source = *reinterpret_cast<Source*>(base);
        return !!source.pfd.revents;
    },
    // dispatch
    [](GSource* base, GSourceFunc, gpointer) -> gboolean
    {
        auto& source = *reinterpret_cast<Source*>(base);

        if (source.pfd.revents & G_IO_IN)
            wl_display_dispatch(source.display);

        if (source.pfd.revents & (G_IO_ERR | G_IO_HUP))
            return FALSE;

        source.pfd.revents = 0;
        return TRUE;
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

static Backend* s_singleton { nullptr };

void Backend::connect(int hostFd)
{
    if (s_singleton)
        return;

    s_singleton = new Backend(hostFd);
}

void Backend::invalidate()
{
    if (s_singleton) {
        delete s_singleton;
        s_singleton = nullptr;
    }
}

Backend* Backend::singleton()
{
    return s_singleton;
}

Backend::Backend(int hostFd)
{
    m_display = wl_display_connect_to_fd(hostFd);
    fprintf(stderr, "EGL: Backend(%d), m_display %p, err %d\n",
        hostFd, m_display, wl_display_get_error(m_display));

    m_source = g_source_new(&Source::s_sourceFuncs, sizeof(Source));
    auto& source = *reinterpret_cast<Source*>(m_source);
    source.pfd.fd = wl_display_get_fd(m_display);
    source.pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
    source.pfd.revents = 0;
    source.display = m_display;

    g_source_add_poll(m_source, &source.pfd);
    g_source_set_name(m_source, "WPEBackend-shm::Backend");
    g_source_set_priority(m_source, -70);
    g_source_set_can_recurse(m_source, TRUE);
    g_source_attach(m_source, g_main_context_get_thread_default());

    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &s_registryListener, this);
    wl_display_roundtrip(m_display);
}

Backend::~Backend()
{
    if (m_source) {
        g_source_destroy(m_source);
        g_source_unref(m_source);
    }
}

struct wl_display* Backend::display() const
{
    return m_display;
}

struct wl_compositor* Backend::compositor() const
{
    return m_compositor;
}

const struct wl_registry_listener Backend::s_registryListener = {
    // global
    [](void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t)
    {
        auto& backend = *reinterpret_cast<Backend*>(data);

        if (!std::strcmp(interface, "wl_compositor"))
            backend.m_compositor = static_cast<struct wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 1));
    },
    // global_remove
    [](void*, struct wl_registry*, uint32_t) { },
};

} // WLClient
