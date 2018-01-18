#pragma once

#include <glib.h>
#include <wayland-client.h>

namespace WLClient {

class Backend {
public:
    static void connect(int);
    static void invalidate();
    static Backend* singleton();

    struct wl_display* display() const;
    struct wl_compositor* compositor() const;

private:
    static const struct wl_registry_listener s_registryListener;

    Backend(int);
    ~Backend();

    struct wl_display* m_display;
    struct wl_registry* m_registry;
    struct wl_compositor* m_compositor;

    GSource* m_source;
};

} // namespace WLClient
