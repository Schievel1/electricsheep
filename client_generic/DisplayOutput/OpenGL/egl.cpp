#ifndef WIN32
#ifdef HAVE_WAYLAND

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <assert.h>
#include <iostream>
#include <string>

#include "Exception.h"
#include "Log.h"
#include "egl.h"

#include "egl.h"

namespace DisplayOutput {

#ifdef HAVE_LIBDECOR
  void
  frame_configure(struct libdecor_frame *frame,
		              struct libdecor_configuration *configuration,
		              void *user_data)
  {
    CWaylandGL *waylandGL = static_cast<CWaylandGL *>(user_data);
	  struct libdecor_state *state;
	  int width, height;

	  if (!libdecor_configuration_get_content_size(configuration, frame,
						                                     &width, &height)) {
		  width = waylandGL->floating_width;
		  height = waylandGL->floating_height;
	  }

	  waylandGL->content_width = width;
	  waylandGL->content_height = height;

	  wl_egl_window_resize(waylandGL->m_EGLWindow,
			                   waylandGL->content_width, waylandGL->content_height,
			                   0, 0);
    glViewport(0, 0, waylandGL->content_width, waylandGL->content_height);

	  state = libdecor_state_new(width, height);
	  libdecor_frame_commit(frame, state, configuration);
	  libdecor_state_free(state);

	  /* store floating dimensions */
	  if (libdecor_frame_is_floating(waylandGL->frame)) {
		  waylandGL->floating_width = width;
		  waylandGL->floating_height = height;
	  }

	  waylandGL->configured = true;
  }

  void
  frame_close(struct libdecor_frame *frame,
	            void *user_data)
  {
    CWaylandGL *waylandGL = static_cast<CWaylandGL *>(user_data);

	  waylandGL->m_bClosed = true;
  }

  void
  frame_commit(struct libdecor_frame *frame,
	             void *user_data)
  {
    CWaylandGL *waylandGL = static_cast<CWaylandGL *>(user_data);
    eglSwapBuffers(waylandGL->m_EGLDisplay, waylandGL->m_EGLSurface);
  }

  static struct libdecor_frame_interface frame_interface = {
	  frame_configure,
	  frame_close,
	  frame_commit,
  };

  static void
  libdecor_error(struct libdecor *context,
                 enum libdecor_error error,
                 const char *message)
  {
    fprintf(stderr, "Caught error (%d): %s\n", error, message);
    exit(EXIT_FAILURE);
  }

  static struct libdecor_interface libdecor_interface = {
    libdecor_error,
  };
#endif

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                             uint32_t serial) {
  xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

void registry_handle_global(void *data, struct wl_registry *registry,
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
    xdg_wm_base_add_listener(waylandGL->m_XdgWmBase, &xdg_wm_base_listener,
                             waylandGL);
  } else if (strcmp(interface, wl_output_interface.name) == 0) {
    waylandGL->m_Output = (struct wl_output *)wl_registry_bind(
        registry, name, &wl_output_interface, 1);
  } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
    waylandGL->m_Shell = (struct zwlr_layer_shell_v1 *)wl_registry_bind(
        registry, name, &zwlr_layer_shell_v1_interface, 1);
  } else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
    waylandGL->m_DecorationManager = (struct zxdg_decoration_manager_v1 *)wl_registry_bind(
        registry, name, &zxdg_decoration_manager_v1_interface, 1);
  }
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global, nullptr};

void xdg_toplevel_configure_handler(void *data,
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

void xdg_toplevel_close_handler(void *data, struct xdg_toplevel *xdg_toplevel) {
  fprintf(stderr, "close\n");
}

const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure_handler,
    .close = xdg_toplevel_close_handler};

void xdg_surface_configure_handler(void *data, struct xdg_surface *xdg_surface,
                                   uint32_t serial) {
  CWaylandGL *waylandGL = static_cast<CWaylandGL *>(data);
  xdg_surface_ack_configure(xdg_surface, serial);
  waylandGL->configured = true;
}

const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure_handler};

void zwlr_layer_surface_configure_handler(
    void *data, struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1,
    uint32_t serial, uint32_t width, uint32_t height) {
  zwlr_layer_surface_v1_ack_configure(zwlr_layer_surface_v1, serial);
}

const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = zwlr_layer_surface_configure_handler};

CWaylandGL::CWaylandGL() : CDisplayOutput() {
  // Constructor initialization
}

CWaylandGL::~CWaylandGL() {
  // Cleanup
#ifdef HAVE_LIBDECOR
  if (context) {
    libdecor_unref(context);
  }
#endif
  if (m_EGLDisplay != EGL_NO_DISPLAY) {
    eglMakeCurrent(m_EGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
    if (m_EGLContext != EGL_NO_CONTEXT) {
      eglDestroyContext(m_EGLDisplay, m_EGLContext);
    }
    if (m_EGLSurface != EGL_NO_SURFACE) {
      eglDestroySurface(m_EGLDisplay, m_EGLSurface);
    }
    eglTerminate(m_EGLDisplay);
  }
  if (m_EGLWindow) {
    wl_egl_window_destroy(m_EGLWindow);
  }
  if (m_Surface) {
    wl_surface_destroy(m_Surface);
  }
  if (m_ShellSurface) {
    wl_shell_surface_destroy(m_ShellSurface);
  }
  if (m_pDisplay) {
    wl_display_disconnect(m_pDisplay);
  }
}

bool CWaylandGL::Initialize(const uint32 _width, const uint32 _height,
                            const bool _bFullscreen) {
  m_Width = m_WidthFS = _width;
  m_Height = m_HeightFS =  _height;
  fprintf(stderr, "CWaylandGL\n");

  // Connect to the Wayland display
  m_pDisplay = wl_display_connect(NULL);
  assert(m_pDisplay);

  // Create a Wayland registry
  struct wl_registry *registry = wl_display_get_registry(m_pDisplay);
  assert(registry);

  // Add listeners to the registry to handle global events
  wl_registry_add_listener(registry, &registry_listener, this);
  // wl_display_dispatch(m_pDisplay);
  wl_display_roundtrip(m_pDisplay);

  assert(m_Compositor);
  assert(m_XdgWmBase);

  // Initialize EGL
  EGLint count, size, numConfigs;
  EGLConfig *configs;
  EGLint configAttribs[] = {EGL_SURFACE_TYPE,
                            EGL_WINDOW_BIT,
                            EGL_RED_SIZE,
                            8,
                            EGL_GREEN_SIZE,
                            8,
                            EGL_BLUE_SIZE,
                            8,
                            EGL_DEPTH_SIZE,
                            24,
                            EGL_ALPHA_SIZE,
                            8,
                            EGL_RENDERABLE_TYPE,
                            EGL_OPENGL_ES2_BIT,
                            EGL_NONE};
  EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
                             EGL_NONE};
  m_EGLDisplay = eglGetDisplay((EGLNativeDisplayType)m_pDisplay);
  assert(m_EGLDisplay);
  if (m_EGLDisplay == EGL_NO_DISPLAY) {
    fprintf(stderr, "Can't create egl display\n");
    return false;
  }

  if (!eglInitialize(m_EGLDisplay, NULL, NULL)) {
    fprintf(stderr, "eglInitialize failed with error: 0x%x\n", eglGetError());
    return false;
  }

  if (!eglBindAPI(EGL_OPENGL_API)) {
    fprintf(stderr, "eglBindAPI failed with error: 0x%x\n", eglGetError());
    return false;
  }

  if (!eglGetConfigs(m_EGLDisplay, NULL, 0, &count) || count < 1) {
    fprintf(stderr, "Failed to get configs\n");
    return false;
  }

  configs = (EGLConfig *)calloc(count, sizeof(EGLConfig));
  if (!configs) {
    fprintf(stderr, "Failed to allocate config list");
    return false;
  }

  if (!eglChooseConfig(m_EGLDisplay, configAttribs, configs, count,
                       &numConfigs) ||
      numConfigs == 0) {
    fprintf(stderr, "eglChooseConfig failed with error: 0x%x\n", eglGetError());
    return false;
  }
  m_EGLConfig = configs[0];
  m_EGLContext = eglCreateContext(m_EGLDisplay, m_EGLConfig, EGL_NO_CONTEXT,
                                  contextAttribs);
  if (m_EGLContext == EGL_NO_CONTEXT) {
    fprintf(stderr, "eglCreateContext failed with error: 0x%x\n", eglGetError());
    return false;
  }

  m_Surface = wl_compositor_create_surface(m_Compositor);
  assert(m_Surface);
  fprintf(stderr, "Created surface\n");

  m_EGLWindow = wl_egl_window_create(m_Surface, _width, _height);
  if (!m_EGLWindow) {
    fprintf(stderr, "wl_egl_window_create failed\n");
    return false;
  }

  EGLint surfaceAttribs[] = {EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE};
  m_EGLSurface =
    eglCreateWindowSurface(m_EGLDisplay, m_EGLConfig,
                           (EGLNativeWindowType)m_EGLWindow, surfaceAttribs);
  if (m_EGLSurface == EGL_NO_SURFACE) {
    fprintf(stderr, "eglCreateWindowSurface failed with error: 0x%x\n", eglGetError());
    return false;
  }

  if (!eglMakeCurrent(m_EGLDisplay, m_EGLSurface, m_EGLSurface, m_EGLContext)) {
    fprintf(stderr, "eglMakeCurrent failed with error: 0x%x\n", eglGetError());
    return false;
  }

  /* Ensure that buffer swaps for egl_surface are not synchronized
   * to the compositor, as this would result in blocking and round-robin
   * updates when there are multiple outputs */
  if (!eglSwapInterval(m_EGLDisplay, 0)) {
    fprintf(stderr, "Failed to set swap interval\n");
    return false;
  }


  const char *is_background = getenv("ELECTRICSHEEP_BACKGROUND");

  if (is_background) {
    m_Background = true;
  }


  if (!m_Background) { // normal window
    if (m_DecorationManager) { // compositor knows xdg-decoration, use xdg-shell and server side decor
      using_csd = false;
      fprintf(stderr, "Using xdg-shell and server side decor\n");
      m_XdgSurface = xdg_wm_base_get_xdg_surface(m_XdgWmBase, m_Surface);
      assert(m_XdgSurface);
      xdg_surface_add_listener(m_XdgSurface, &xdg_surface_listener, this);
      fprintf(stderr, "Created xdgsurface\n");
      m_XdgToplevel = xdg_surface_get_toplevel(m_XdgSurface);
      assert(m_XdgToplevel);
      xdg_toplevel_add_listener(m_XdgToplevel, &xdg_toplevel_listener, this);
      fprintf(stderr, "Created toplevel\n");
      m_Decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(m_DecorationManager, m_XdgToplevel);
      zxdg_toplevel_decoration_v1_set_mode(m_Decoration,
                                           ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
      xdg_surface_set_window_geometry(m_XdgSurface, 0, 0, _width, _height);
    }
#ifdef HAVE_LIBDECOR
    else { // compositor does not know xdg-decoration, use libdecor and client side decor
      fprintf(stderr, "Using libdecor\n");
      using_csd = true;
      configured = false;
      floating_width = m_WidthFS;
      floating_height = m_HeightFS;
      context = libdecor_new(m_pDisplay, &libdecor_interface);
      frame = libdecor_decorate(context, m_Surface, &frame_interface, this);
      libdecor_frame_map(frame);
      while (!configured) {
        if (libdecor_dispatch(context, 0) < 0) {
          fprintf(stderr, "Failed to dispatch libdecor\n");
          return false;
        }
      }
    }
#else
    else {
      fprintf(stderr, "Compositor does not support xdg-decoration\n");
      fprintf(stderr, "and electricsheep is compiled without libdecor support.\n");
      fprintf(stderr, "Still trying basic output without title bar.\n");
      m_XdgSurface = xdg_wm_base_get_xdg_surface(m_XdgWmBase, m_Surface);
      assert(m_XdgSurface);
      xdg_surface_add_listener(m_XdgSurface, &xdg_surface_listener, this);
      fprintf(stderr, "Created xdgsurface\n");
      m_XdgToplevel = xdg_surface_get_toplevel(m_XdgSurface);
      assert(m_XdgToplevel);
      xdg_toplevel_add_listener(m_XdgToplevel, &xdg_toplevel_listener, this);
      fprintf(stderr, "Created toplevel\n");
      xdg_surface_set_window_geometry(m_XdgSurface, 0, 0, _width, _height);
    }
#endif
  } else { // wl_layer_shell
    if (!m_Shell) {
      fprintf(stderr, "Compositor does not support layer shell\n");
      return false;
    }
    layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        m_Shell, m_Surface, m_Output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
        "wallpaper");
    zwlr_layer_surface_v1_set_size(layer_surface, 0, 0);
    zwlr_layer_surface_v1_set_anchor(layer_surface,
                                     ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
    zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, -1);
    zwlr_layer_surface_v1_add_listener(layer_surface, &layer_surface_listener,
                                       m_Output);
  }
  wl_surface_commit(m_Surface);
  wl_display_roundtrip(m_pDisplay);

  glViewport(0, 0, _width, _height);

  setFullScreen(_bFullscreen);

  free(configs);

  return true;
}

void CWaylandGL::Title(const std::string &_title) {
  // Set window title
  if (!m_Background)
  {
    if (!using_csd)
    {
      xdg_toplevel_set_title(m_XdgToplevel, _title.c_str());
      xdg_toplevel_set_app_id(m_XdgToplevel, _title.c_str());
    }
#ifdef HAVE_LIBDECOR
    else
    {
      libdecor_frame_set_title(frame, _title.c_str());
      libdecor_frame_set_app_id(frame, _title.c_str());
    }
#endif
  }
}

void CWaylandGL::setFullScreen(bool enabled) {
  // Fullscreen setup
  fprintf(stderr, "Setting fullscreen: %d\n", enabled);
  if (!m_Background) {
      if (enabled) {
#ifdef HAVE_LIBDECOR
        if (using_csd)
          libdecor_frame_set_fullscreen(frame, m_Output);
        else
#endif
          xdg_toplevel_set_fullscreen(m_XdgToplevel, m_Output);
      } else {
#ifdef HAVE_LIBDECOR
        if (using_csd)
          libdecor_frame_unset_fullscreen(frame)
        else
#endif
          xdg_toplevel_unset_fullscreen(m_XdgToplevel);
      }
}

void CWaylandGL::Update() {
  checkClientMessages();
}

void CWaylandGL::SwapBuffers() {
#ifdef HAVE_LIBDECOR
  if (using_csd) {
    if (libdecor_dispatch(context, 0) < 0) {
      fprintf(stderr, "libdecor dispatch failed");
    }
  }
#endif
  if (configured) {
    if (!eglSwapBuffers(m_EGLDisplay, m_EGLSurface)) {
      fprintf(stderr, "Swapping buffers failed\n");
    }
  }
}

  void CWaylandGL::checkClientMessages() {
    // Handle Wayland events
  }

} // namespace DisplayOutput

#endif
#endif
