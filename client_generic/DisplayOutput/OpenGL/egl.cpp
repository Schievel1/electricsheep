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
      waylandGL->SetCompositor((struct wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 1));
    } else if (strcmp(interface, wl_shell_interface.name) == 0) {
      waylandGL->m_Shell = (struct wl_shell*)wl_registry_bind(registry, name, &wl_shell_interface, 1); // Correct binding
    }
}

  static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    nullptr
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
    wl_display_roundtrip(m_pDisplay);

    // Ensure that the surface is created correctly
    m_Surface = wl_compositor_create_surface(m_Compositor);
    assert(m_Surface);

    // Initialize EGL
    m_EGLDisplay = eglGetDisplay((EGLNativeDisplayType)m_pDisplay);
    assert(m_EGLDisplay);

    eglInitialize(m_EGLDisplay, NULL, NULL);

    EGLint configAttribs[] = {
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_DEPTH_SIZE, 24,
      EGL_STENCIL_SIZE, 8,
      EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(m_EGLDisplay, configAttribs, &config, 1, &numConfigs);
    assert(numConfigs > 0);

    m_EGLContext = eglCreateContext(m_EGLDisplay, config, EGL_NO_CONTEXT, NULL);
    assert(m_EGLContext);

    m_EGLWindow = wl_egl_window_create(m_Surface, _width, _height);
    assert(m_EGLWindow);

    m_EGLSurface = eglCreateWindowSurface(m_EGLDisplay, config, (EGLNativeWindowType)m_EGLWindow, NULL);
    assert(m_EGLSurface);

    eglMakeCurrent(m_EGLDisplay, m_EGLSurface, m_EGLSurface, m_EGLContext);

    // Set OpenGL viewport
    glViewport(0, 0, _width, _height);

    return true;
  }

  void CWaylandGL::Title(const std::string &_title) {
    // Set window title
  }

  void CWaylandGL::setFullScreen(bool enabled) {
    // Fullscreen setup
  }

  void CWaylandGL::Update() {
    checkClientMessages();
  }

  void CWaylandGL::SwapBuffers() {
    eglSwapBuffers(m_EGLDisplay, m_EGLSurface);
  }

  void CWaylandGL::checkClientMessages() {
    // Handle Wayland events
  }

} // namespace DisplayOutput

#endif
#endif
