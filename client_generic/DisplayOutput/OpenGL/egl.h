#ifndef EGL_VIDEO_OUTPUT_H
#define EGL_VIDEO_OUTPUT_H

#ifndef WIN32

#ifdef _DisplayGL_H_
#error "DisplayGL.h included before egl.h!"
#endif

#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <unistd.h>
#include "xdg-shell.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-decoration.h"

#ifndef LINUX_GNU
#include "GLee.h"
#else
#include <GLee.h>
#endif
#include "DisplayOutput.h"

#ifdef HAVE_LIBDECOR
#include <libdecor.h>
#endif

namespace	DisplayOutput
{

class CWaylandGL : public CDisplayOutput
{
    wl_display       *m_pDisplay = nullptr;
    wl_compositor    *m_Compositor = nullptr;
    wl_surface       *m_Surface = nullptr;
    zwlr_layer_shell_v1 *m_Shell = nullptr;
    xdg_wm_base      *m_XdgWmBase = nullptr;
    xdg_surface    *m_XdgSurface = nullptr;
    xdg_toplevel *m_XdgToplevel = nullptr;
    wl_shell_surface *m_ShellSurface = nullptr;
    zwlr_layer_surface_v1 *layer_surface = nullptr;
    wl_egl_window    *m_EGLWindow = nullptr;
    wl_output        *m_Output = nullptr;
    zxdg_decoration_manager_v1 *m_DecorationManager = nullptr;
    zxdg_toplevel_decoration_v1 *m_Decoration = nullptr;
    bool using_csd = false;
    bool configured = false;
#ifdef HAVE_LIBDECOR
    libdecor *context;
    libdecor_frame *frame;
    int content_width;
    int content_height;
    int floating_width;
    int floating_height;
    bool open = false;
#endif

    EGLDisplay m_EGLDisplay = nullptr;
    EGLContext m_EGLContext = nullptr;
    EGLSurface m_EGLSurface = nullptr;
    EGLConfig  m_EGLConfig = nullptr;

    bool              m_FullScreen;
    bool              m_Background = false;

    uint32	m_WidthFS;
    uint32	m_HeightFS;

    void    setFullScreen( bool enabled );
    void    checkClientMessages();
    //bool    checkResizeEvent( ResizeEvent &event);

    public:
            CWaylandGL();
            virtual ~CWaylandGL();

			static const char *Description()	{	return "Wayland OpenGL display";	};

            virtual bool Initialize( const uint32 _width, const uint32 _height, const bool _bFullscreen );
			virtual void Title( const std::string &_title );
			virtual void Update();

            void    request_next_frame();
			void SwapBuffers();

            friend void registry_handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
            friend void xdg_toplevel_configure_handler(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states);
            friend void xdg_surface_configure_handler(void *data, struct xdg_surface *xdg_surface, uint32_t serial);
    friend void frame_configure(struct libdecor_frame *frame, struct libdecor_configuration *configuration, void *user_data);
    friend void frame_close(struct libdecor_frame *frame, void *user_data);
    friend void frame_destroy(struct libdecor_frame *frame, void *user_data);
    friend void frame_commit(struct libdecor_frame *frame, void *user_data);


};

typedef	CWaylandGL	CDisplayGL;

}

#endif
#endif
