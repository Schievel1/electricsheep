#ifndef EGL_VIDEO_OUTPUT_H
#define EGL_VIDEO_OUTPUT_H

#ifndef WIN32
#ifdef HAVE_WAYLAND

// #ifdef _DisplayGL_H_
// #error "DisplayGL.h included before egl.h!"
// #endif

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-decoration.h"
#include "xdg-shell.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#ifndef LINUX_GNU
#include "GLee.h"
#else
#include <GLee.h>
#endif
#include "DisplayOutput.h"

#ifdef HAVE_LIBDECOR
#include <libdecor.h>
#endif

namespace DisplayOutput {

class CWaylandGL : public CDisplayOutput {
  bool using_csd = false;
  bool configured = false;
  // wayland
  wl_display *m_pDisplay = nullptr;
  wl_compositor *m_Compositor = nullptr;
  wl_surface *m_Surface = nullptr;
  wl_output *m_Output = nullptr;
  // xdg
  xdg_wm_base *m_XdgWmBase = nullptr;
  xdg_surface *m_XdgSurface = nullptr;
  xdg_toplevel *m_XdgToplevel = nullptr;
  zxdg_decoration_manager_v1 *m_DecorationManager = nullptr;
  zxdg_toplevel_decoration_v1 *m_Decoration = nullptr;
  // wlr layer shell
  zwlr_layer_shell_v1 *m_WlrLayerShell = nullptr;
  zwlr_layer_surface_v1 *layer_surface = nullptr;
  // egl
  wl_egl_window *m_EGLWindow = nullptr;
  EGLDisplay m_EGLDisplay = nullptr;
  EGLContext m_EGLContext = nullptr;
  EGLSurface m_EGLSurface = nullptr;
  EGLConfig m_EGLConfig = nullptr;

#ifdef HAVE_LIBDECOR
  libdecor *m_LibdecorContext;
  libdecor_frame *m_LibdecorFrame;
  int m_LibdecorContentWidth;
  int m_LibdecorContentHeight;
  int m_LibdecorFloatingWidth;
  int m_LibdecorFloatingHeight;
  bool open = false;
#endif

  bool m_FullScreen;
  bool m_Background = false;

  uint32 m_WidthFS;
  uint32 m_HeightFS;

  void setFullScreen(bool enabled);
  void checkClientMessages();

// wayland handling stuff starts here
#ifdef HAVE_LIBDECOR
  static void frame_configure(struct libdecor_frame *frame,
                              struct libdecor_configuration *configuration,
                              void *user_data) {
    CWaylandGL *waylandGL = static_cast<CWaylandGL *>(user_data);
    struct libdecor_state *state;
    int width, height;

    if (!libdecor_configuration_get_content_size(configuration, frame, &width,
                                                 &height)) {
      width = waylandGL->m_LibdecorFloatingWidth;
      height = waylandGL->m_LibdecorFloatingHeight;
    }

    waylandGL->m_LibdecorContentWidth = width;
    waylandGL->m_LibdecorContentHeight = height;

    wl_egl_window_resize(waylandGL->m_EGLWindow,
                         waylandGL->m_LibdecorContentWidth,
                         waylandGL->m_LibdecorContentHeight, 0, 0);
    glViewport(0, 0, waylandGL->m_LibdecorContentWidth,
               waylandGL->m_LibdecorContentHeight);

    state = libdecor_state_new(width, height);
    libdecor_frame_commit(frame, state, configuration);
    libdecor_state_free(state);

    /* store floating dimensions */
    if (libdecor_frame_is_floating(waylandGL->m_LibdecorFrame)) {
      waylandGL->m_LibdecorFloatingWidth = width;
      waylandGL->m_LibdecorFloatingHeight = height;
    }

    waylandGL->configured = true;
  }

  static void frame_close(struct libdecor_frame *frame, void *user_data) {
    CWaylandGL *waylandGL = static_cast<CWaylandGL *>(user_data);

    waylandGL->m_bClosed = true;
  }

  static void frame_commit(struct libdecor_frame *frame, void *user_data) {
    CWaylandGL *waylandGL = static_cast<CWaylandGL *>(user_data);
    eglSwapBuffers(waylandGL->m_EGLDisplay, waylandGL->m_EGLSurface);
  }

  struct libdecor_frame_interface frame_interface = {
      frame_configure,
      frame_close,
      frame_commit,
  };

  static void libdecor_error(struct libdecor *context,
                             enum libdecor_error error, const char *message) {
    fprintf(stderr, "Caught error (%d): %s\n", error, message);
    exit(EXIT_FAILURE);
  }

  struct libdecor_interface libdecor_interface = {
      libdecor_error,
  };
#endif

  static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                               uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
  }

  const struct xdg_wm_base_listener xdg_wm_base_listener = {
      .ping = xdg_wm_base_ping,
  };

  static void xdg_toplevel_configure_handler(void *data,
                                             struct xdg_toplevel *xdg_toplevel,
                                             int32_t width, int32_t height,
                                             struct wl_array *states) {
    fprintf(stderr, "configure: %dx%d\n", width, height);
    CWaylandGL *waylandGL = static_cast<CWaylandGL *>(data);
    if (width == 0 || height == 0) {
      /* Compositor is deferring to us */
      return;
    }
    waylandGL->m_WidthFS = width;
    waylandGL->m_HeightFS = height;
    glViewport(0, 0, width, height);
  }

  static void xdg_toplevel_close_handler(void *data,
                                         struct xdg_toplevel *xdg_toplevel) {
    CWaylandGL *waylandGL = static_cast<CWaylandGL *>(data);
    waylandGL->m_bClosed = true;
  }

  const struct xdg_toplevel_listener xdg_toplevel_listener = {
      .configure = xdg_toplevel_configure_handler,
      .close = xdg_toplevel_close_handler};

  static void xdg_surface_configure_handler(void *data,
                                            struct xdg_surface *xdg_surface,
                                            uint32_t serial) {
    CWaylandGL *waylandGL = static_cast<CWaylandGL *>(data);
    xdg_surface_ack_configure(xdg_surface, serial);
    waylandGL->configured = true;
  }

  const struct xdg_surface_listener xdg_surface_listener = {
      .configure = xdg_surface_configure_handler};

  static void zwlr_layer_surface_configure_handler(
      void *data, struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1,
      uint32_t serial, uint32_t width, uint32_t height) {
    glViewport(0, 0, width, height);
    zwlr_layer_surface_v1_ack_configure(zwlr_layer_surface_v1, serial);
  }

  const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
      .configure = zwlr_layer_surface_configure_handler};

  static void registry_handle_global(void *data, struct wl_registry *registry,
                                     uint32_t name, const char *interface,
                                     uint32_t version) {
    CWaylandGL *waylandGL = static_cast<CWaylandGL *>(data);
    // fprintf(stderr, "Got a registry event for %s id %d\n", interface, name);
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
      waylandGL->m_Compositor = (struct wl_compositor *)wl_registry_bind(
          registry, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
      waylandGL->m_XdgWmBase = (struct xdg_wm_base *)wl_registry_bind(
          registry, name, &xdg_wm_base_interface, 1);
      xdg_wm_base_add_listener(waylandGL->m_XdgWmBase,
                               &waylandGL->xdg_wm_base_listener, waylandGL);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
      waylandGL->m_Output = (struct wl_output *)wl_registry_bind(
          registry, name, &wl_output_interface, 1);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
      waylandGL->m_WlrLayerShell =
          (struct zwlr_layer_shell_v1 *)wl_registry_bind(
              registry, name, &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) ==
               0) {
      waylandGL->m_DecorationManager =
          (struct zxdg_decoration_manager_v1 *)wl_registry_bind(
              registry, name, &zxdg_decoration_manager_v1_interface, 1);
    }
  }
  const struct wl_registry_listener registry_listener = {registry_handle_global,
                                                         nullptr};
  // wayland handling stuff ends here

public:
  CWaylandGL();
  virtual ~CWaylandGL();

  static const char *Description() { return "Wayland OpenGL display"; };

  virtual bool Initialize(const uint32 _width, const uint32 _height,
                          const bool _bFullscreen);
  virtual void Title(const std::string &_title);
  virtual void Update();

  // void request_next_frame();
  void SwapBuffers();
};

#if defined(HAVE_WAYLAND) && !defined(HAVE_X11)
/* See player.cpp
- If we have wayland and X11, we decide at runtime to load CWaylandGL in
player.cpp
- If we have wayland and no X11, typedef CWaylandGL to CDisplayGL and egl.h gets
included in DisplayGL.h
- If we have X11 and no wayland, typedef CDisplayGL to CWaylandGL and glx.h gets
included in DisplayGL.h
*/
typedef CWaylandGL CDisplayGL;
#endif

} // namespace DisplayOutput

#endif
#endif
#endif
