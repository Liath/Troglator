#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>

#include <windows.h>

struct intf_sys_t
{
    HWND window;
    HANDLE ready;
    vlc_thread_t thread;
};

/* Must be same as in src/win32/specific.c */
typedef struct
{
    int argc;
    int enqueue;
    char data[];
} vlc_ipc_data_t;

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

//Heavy lifting
static LRESULT CALLBACK WMCOPYWNDPROC(HWND hwnd, UINT uMsg,
                                      WPARAM wParam, LPARAM lParam)
{
    if( uMsg == WM_QUIT  )
    {
        PostQuitMessage( 0 );
    }
    else if( uMsg == WM_COPYDATA )
    {
        /*We'll get here*/
    }
    return DefWindowProc( hwnd, uMsg, wParam, lParam );
}

//Window thread pump
static void *HelperThread(void *data)
{
    intf_thread_t *intf = data;
    intf_sys_t *sys = intf->p_sys;

    HWND ipcwindow = CreateWindow(L"Winamp v1.x", L"(track number). (artist) - (song name) - Winamp", 0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);

    SetWindowLongPtr(ipcwindow, GWLP_WNDPROC, (LRESULT)WMCOPYWNDPROC);
    SetWindowLongPtr(ipcwindow, GWLP_USERDATA, (uintptr_t)data);

    sys->window = ipcwindow;
    /* Signal the creation of the thread and events queue */
    SetEvent(sys->ready);

    MSG message;
    while (GetMessage(&message, NULL, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return NULL;
}

//Creates Window Thread
static int Open(vlc_object_t *p_this)
{
    intf_thread_t   *p_intf = (intf_thread_t *)p_this;
    intf_sys_t      *p_sys  = malloc( sizeof( *p_sys ) );
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_intf->p_sys = p_sys;

    /* Run the helper thread */
    p_sys->ready = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (vlc_clone(&p_sys->thread, HelperThread, p_intf, VLC_THREAD_PRIORITY_LOW))
    {
        free(p_sys);
        msg_Err(p_intf, "one instance mode DISABLED "
                 "(IPC helper thread couldn't be created)");
        return VLC_ENOMEM;
    }

    WaitForSingleObject(p_sys->ready, INFINITE);
    CloseHandle(p_sys->ready);

    return VLC_SUCCESS;
}

//Creates Window Thread
static void Close(vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t *)obj;
    intf_sys_t *sys = intf->p_sys;

    SendMessage(sys->window, WM_QUIT, 0, 0);
    vlc_join(sys->thread, NULL);
    free(sys);
}

// Module descriptor
vlc_module_begin()
    set_category(CAT_INTERFACE)
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_shortname("Troglator")
    set_description("Translate WinAmpian Trog to VLC Trog")
    set_capability("interface", 0)
    set_callbacks(Open, Close)
vlc_module_end ()
