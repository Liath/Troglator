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
    if(uMsg == WM_QUIT)
    {
        PostQuitMessage( 0 );
    }
    else if(uMsg == WM_USER || uMsg == WM_COMMAND || WM_COPYDATA) //WM_USER=0x400
    {
        intf_thread_t *intf = (intf_thread_t *)(uintptr_t)
            GetWindowLongPtr( hwnd, GWLP_USERDATA );

        if (uMsg == WM_USER) {
            int cid  = (int)lParam;
            int data = (int)wParam;
            msg_Dbg(intf, "Troglator: Received WM_USER ID:%i DATA:%i", cid, data);
            switch(cid) {
                case 0:    //0x00 - Get Version - No idea what's acceptable for an answer
                    return 0x20FF;
                case 100:  //0x64 - Start Playback
                case 102:  //0x66 - ^
                    //return t/f - 0/1
                case 104:  //0x68 - Playback Status - (1 - Playing, 3 - Paused, Else - Stopped)
                    return 1;
                case 105:  //0x69 - Get Track Position - If Data is 0 return in ms, 1 in sec
                    
                case 106:  //0x6A - Seek To - (Data as position in ms)
                    
                case 120:  //0x78 - Write playlist to current directory, we're not gonna implement this if we can help it
                    return 0;
                case 121:  //0x79 - Move to song # in playlist
                    
                case 122:  //0x7A - Set volume to Data as 0-255
                    
                case 123:  //0x7B - Set panning as 0-255 (left/right) - Not sure if possible in vlc

                case 124:  //0x7C - Return length of playlist in terms of tracks

                case 125:  //0x7D - Get List Position in terms of tracks
                    return 0;
                case 126:  //0x7F - 
                    return 0;
                case 211:  //0xD3 - Get File - Theorhetically would return a pointer to the current file, it's flagged as in-process only so ignoring for now.
                    return "C:\\NO - U.mp3"; //It occurs to me that strings are passed around pretty will nilly.
            }
        } else if (uMsg == WM_COMMAND) {
            int cid = (int)wParam;
            msg_Dbg(intf, "Troglator: Received WM_COMMAND ID:%i", cid);
            return 0;
        } else { //WM_COPYDATA
        //    COPYDATASTRUCT *pwm_data = (COPYDATASTRUCT*)lParam;
        //    msg_Dbg(intf, "Troglator: Received WM_COPYDATA");
        //    return 0;
        }
    }
    return DefWindowProc( hwnd, uMsg, wParam, lParam );
}

//Window thread pump
static void *HelperThread(void *data)
{
    intf_thread_t *intf = data;
    intf_sys_t *sys = intf->p_sys;
    msg_Dbg(intf, "Troglator: Translation thread created.");

    WNDCLASS wc;
    wc.style = 0;
    wc.lpfnWndProc = WMCOPYWNDPROC;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(RGB(0,0,0));
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"Winamp v1.x";
    RegisterClass(&wc);

    HWND ipcwindow = CreateWindow(wc.lpszClassName, L"1. I like to watch - Testing - Winamp", 0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);

    SetWindowLongPtr(ipcwindow, GWLP_WNDPROC, (LRESULT)WMCOPYWNDPROC);
    SetWindowLongPtr(ipcwindow, GWLP_USERDATA, (uintptr_t)data);

    sys->window = ipcwindow;
    /* Signal the creation of the thread and events queue */
    SetEvent(sys->ready);
    msg_Dbg(intf, "Troglator: Starting message pump.");
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
    msg_Dbg(p_intf, "Troglator: Initiating translation thread.");
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
    set_capability("interface", 10)
    set_callbacks(Open, Close)
vlc_module_end ()
