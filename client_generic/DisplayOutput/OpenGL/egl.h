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

#ifndef LINUX_GNU
#include "GLee.h"
#else
#include <GLee.h>
#endif
#include "DisplayOutput.h"

namespace	DisplayOutput
{

class CWaylandGL : public CDisplayOutput
{
    wl_display       *m_pDisplay = nullptr;
    wl_compositor    *m_Compositor = nullptr;
    wl_surface       *m_Surface = nullptr;
    wl_shell         *m_Shell = nullptr;
    zxdg_shell_v6    *m_XdgShell = nullptr;
    zxdg_surface_v6    *m_XdgSurface = nullptr;
    zxdg_toplevel_v6 *m_XdgToplevel = nullptr;
    wl_shell_surface *m_ShellSurface = nullptr;
    wl_egl_window    *m_EGLWindow = nullptr;
    wl_output        *m_Output = nullptr;
    // test code
    wl_region* region = nullptr;

    EGLDisplay m_EGLDisplay = nullptr;
    EGLContext m_EGLContext = nullptr;
    EGLSurface m_EGLSurface = nullptr;
    EGLConfig  m_EGLConfig = nullptr;

    bool              m_FullScreen;

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

            // Add a setter for setting m_Compositor
            void SetCompositor(wl_compositor *compositor) { m_Compositor = compositor; }
            friend void registry_handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
};

typedef	CWaylandGL	CDisplayGL;

}

#endif
#endif