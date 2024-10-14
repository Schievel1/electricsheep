#ifndef	WIN32
#ifdef HAVE_WAYLAND

#include <string>
#include <iostream>
#include <assert.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "egl.h"
#include "Log.h"
#include "Exception.h"

#include "egl.h"

namespace DisplayOutput {

  void registry_handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    CWaylandGL *waylandGL = static_cast<CWaylandGL*>(data);

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
      waylandGL->m_Compositor = (struct wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "zxdg_shell_v6") == 0) {
      waylandGL->m_XdgShell = (struct zxdg_shell_v6*)wl_registry_bind(registry, name, &zxdg_shell_v6_interface, 1);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
      waylandGL->m_Output = (struct wl_output*)wl_registry_bind(registry, name, &wl_output_interface, 1);
    }
}

  static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    nullptr
  };

  void xdg_toplevel_configure_handler
(
 void *data,
 struct zxdg_toplevel_v6 *xdg_toplevel,
 int32_t width,
 int32_t height,
 struct wl_array *states
 ) {
    printf("configure: %dx%d\n", width, height);
  }

  void xdg_toplevel_close_handler
(
 void *data,
 struct zxdg_toplevel_v6 *xdg_toplevel
 ) {
    printf("close\n");
  }

  const struct zxdg_toplevel_v6_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure_handler,
    .close = xdg_toplevel_close_handler
  };

  void xdg_surface_configure_handler
(
 void *data,
 struct zxdg_surface_v6 *xdg_surface,
 uint32_t serial
 ) {
    zxdg_surface_v6_ack_configure(xdg_surface, serial);
  }

  const struct zxdg_surface_v6_listener xdg_surface_listener = {
    .configure = xdg_surface_configure_handler
  };

  CWaylandGL::CWaylandGL() : CDisplayOutput() {
    // Constructor initialization
  }

  CWaylandGL::~CWaylandGL() {
    // Cleanup
    if (m_EGLDisplay != EGL_NO_DISPLAY) {
      eglMakeCurrent(m_EGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
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

  bool CWaylandGL::Initialize(const uint32 _width, const uint32 _height, const bool _bFullscreen) {
    m_Width = _width;
    m_Height = _height;

    // Connect to the Wayland display
    m_pDisplay = wl_display_connect(NULL);
    assert(m_pDisplay);

    // Create a Wayland registry
    struct wl_registry *registry = wl_display_get_registry(m_pDisplay);
    assert(registry);

    // Add listeners to the registry to handle global events
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_dispatch(m_pDisplay);
    wl_display_roundtrip(m_pDisplay);

    assert(m_Compositor);
    assert(m_XdgShell);

    m_Surface = wl_compositor_create_surface(m_Compositor);
    assert(m_Surface);
	  fprintf(stderr, "Created surface\n");

    m_XdgSurface = zxdg_shell_v6_get_xdg_surface(m_XdgShell, m_Surface);
    assert(m_XdgSurface);
    zxdg_surface_v6_add_listener(m_XdgSurface, &xdg_surface_listener, NULL);
	  fprintf(stderr, "Created shellsurface\n");

    m_XdgToplevel = zxdg_surface_v6_get_toplevel(m_XdgSurface);
    assert(m_XdgToplevel);
    zxdg_toplevel_v6_add_listener(m_XdgToplevel, &xdg_toplevel_listener, NULL);
    fprintf(stderr, "Created toplevel\n");

    wl_surface_commit(m_Surface);
    wl_display_roundtrip(m_pDisplay);
    wl_surface_commit(m_Surface);

    // Initialize EGL
    EGLint major, minor, count, size, numConfigs;
    EGLConfig *configs;
    EGLint configAttribs[] = {
		  EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		  EGL_RED_SIZE, 8,
		  EGL_GREEN_SIZE, 8,
		  EGL_BLUE_SIZE, 8,
      EGL_DEPTH_SIZE, 24,
      EGL_ALPHA_SIZE, 8,
		  EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		  EGL_NONE
	  };
    EGLint contextAttribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2, // OpenGL ES 2.0
      EGL_NONE
    };
    m_EGLDisplay = eglGetDisplay((EGLNativeDisplayType)m_pDisplay);
    assert(m_EGLDisplay);
    if (m_EGLDisplay == EGL_NO_DISPLAY) {
	    fprintf(stderr, "Can't create egl display\n");
      return false;
    } else {
	    fprintf(stderr, "Created egl display\n");
    }

    if (!eglInitialize(m_EGLDisplay, &major, &minor)) {
      printf("eglInitialize failed with error: 0x%x\n", eglGetError());
      return false;
    }
    printf("EGL major: %d, minor %d\n", major, minor);

    // if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    //   printf("eglBindAPI failed with error: 0x%x\n", eglGetError());
    //   return false;
    // }

    if (!eglGetConfigs(m_EGLDisplay, NULL, 0, &count) || count < 1) {
      fprintf(stderr, "Failed to get configs\n");
      return false;
    }
    printf("EGL has %d configs\n", count);

    configs = (EGLConfig*)calloc(count, sizeof(EGLConfig));
    if (!configs) {
      fprintf(stderr, "Failed to allocate config list");
      return false;
    }

    if (!eglChooseConfig(m_EGLDisplay, configAttribs, configs, count, &numConfigs) || numConfigs == 0) {
      printf("eglChooseConfig failed with error: 0x%x\n", eglGetError());
      return false;
    }

    printf("EGL has %d matching configs\n", numConfigs);
    for (int i = 0; i < numConfigs; i++) {
	    eglGetConfigAttrib(m_EGLDisplay,
			                   configs[i], EGL_BUFFER_SIZE, &size);
	    printf("Buffer size for config %d is %d\n", i, size);
	    eglGetConfigAttrib(m_EGLDisplay,
			                   configs[i], EGL_RED_SIZE, &size);
	    printf("Red size for config %d is %d\n", i, size);
    }

	  m_EGLConfig = configs[0];

    m_EGLContext = eglCreateContext(m_EGLDisplay, m_EGLConfig, EGL_NO_CONTEXT, contextAttribs);
    if (m_EGLContext == EGL_NO_CONTEXT) {
      printf("eglCreateContext failed with error: 0x%x\n", eglGetError());
      return false;
    }


    /* second roundtrip: receive names from the outputs */
    // wl_display_roundtrip(m_pDisplay);
    /* after this roundtrip, should have received a configure event */
    // wl_display_roundtrip(m_pDisplay);


    // Create an EGL window surface
    EGLint surfaceAttribs[] = {
      EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
      EGL_NONE
    };

    m_EGLWindow = wl_egl_window_create(m_Surface, _width, _height);
    if (!m_EGLWindow) {
      printf("wl_egl_window_create failed\n");
      return false;
    }

    m_EGLSurface = eglCreateWindowSurface(m_EGLDisplay, m_EGLConfig, (EGLNativeWindowType)m_EGLWindow, surfaceAttribs);
    if (m_EGLSurface == EGL_NO_SURFACE) {
      printf("eglCreateWindowSurface failed with error: 0x%x\n", eglGetError());
      return false;
    }

    if (!eglMakeCurrent(m_EGLDisplay, m_EGLSurface, m_EGLSurface, m_EGLContext)) {
      printf("eglMakeCurrent failed with error: 0x%x\n", eglGetError());
      return false;
    }

    /* Ensure that buffer swaps for egl_surface are not synchronized
     * to the compositor, as this would result in blocking and round-robin
     * updates when there are multiple outputs */
    if (!eglSwapInterval(m_EGLDisplay, 0)) {
      fprintf(stderr, "Failed to set swap interval\n");
      return false;
    }

    // glViewport(0, 0, _width, _height);

      glClearColor(1.0, 1.0, 0.0, 1.0);
      glClear(GL_COLOR_BUFFER_BIT);
      glFlush();

    if (eglSwapBuffers(m_EGLDisplay, m_EGLSurface)) {
      fprintf(stderr, "Swapped buffers\n");
    } else {
      fprintf(stderr, "Swapped buffers failed\n");
    }

    free(configs);

    return true;
  }

  void CWaylandGL::Title(const std::string &_title) {
    // Set window title
    zxdg_toplevel_v6_set_title(m_XdgToplevel, _title.c_str());
  }

  void CWaylandGL::setFullScreen(bool enabled) {
    // Fullscreen setup
  }

  void CWaylandGL::Update() {
    // printf("Update...\n");
    checkClientMessages();
  }

  void CWaylandGL::SwapBuffers() {
    // static bool color_toggle = false;
    // printf("Swapping buffers\n");
    // if (color_toggle) {
    //   glClearColor(1.0, 0.0, 1.0, 1.0);
    //   color_toggle = false;
    // } else {
    //   glClearColor(0.0, 0.0, 1.0, 1.0);
    //   color_toggle = true;
    // }
    // glClear(GL_COLOR_BUFFER_BIT);
    // glFlush();
    if (eglSwapBuffers(m_EGLDisplay, m_EGLSurface)) {
      // fprintf(stderr, "Swapped buffers\n");
    } else {
      fprintf(stderr, "Swapped buffers failed\n");
    }
    if (eglGetError() != EGL_SUCCESS) {
      printf("Failed to swap buffers");
    }
    // exit(0); // DEBUG
  }

  void CWaylandGL::checkClientMessages() {
    // Handle Wayland events
  }

} // namespace DisplayOutput

#endif
#endif
