//cd E:\Projects\vlc\win32\vlc-2.2.0-git
//E:
//vlc -vvv --control trog -I qt
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_aout.h>

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
    else if(uMsg == WM_USER)
    {
        intf_thread_t *p_intf = (intf_thread_t *)(uintptr_t)
            GetWindowLongPtr( hwnd, GWLP_USERDATA );
        playlist_t *p_playlist = pl_Get( p_intf );
        input_thread_t *p_input = playlist_CurrentInput( p_playlist );
        // Skipping vout messages for now. Functionality before fluff

        int cid  = (int)lParam;
        int data = (int)wParam;
        msg_Dbg(p_intf, "Troglator: Received WM_USER ID:%i DATA:%i", cid, data);
        switch(cid) {
            case 0:    //0x00 - Get Version - No idea what's acceptable for an answer
                return 0x20FF;
            case 100:  //0x64 - Toggle Playback
            case 102:  //0x66 - ^
                if( p_input )
                {
                    playlist_Pause( p_playlist );
                }
                else {
                    playlist_Play( p_playlist );
                }
                //I'm not sure if our return matters
                return 1;
            case 104:  //0x68 - Playback Status - (1 - Playing, 3 - Paused, Else - Stopped)
                if( !p_input ) return 2;
                int state = var_GetInteger( p_input, "state" );
                if (state == PAUSE_S) return 3;
                if (state == PLAYING_S) return 1;
                return 0;
            case 105:  //0x69 - Get Track Position - If Data is 0 return in ms, 1 in sec
                if( !p_input )
                { 
                    return 0x570917; //Just staahhhpppp
                } else {
                    if (data == 0) {
                        return (int)var_GetTime(p_input, "time");
                    } else {
                        return (int)(var_GetTime(p_input, "time") + 500ULL)/1000ULL; //Per libvlc_internal->from_mtime
                    }
                }
            case 106:  //0x6A - Seek To - (Data as position in ms) 
                //TODO: Check if time is possible to seek to.
                if( p_input == NULL || !var_GetBool( p_input, "can-seek" )) return 0x570917; //Staaahhhhppp iiiittt
                var_SetTime( p_input, "time-offset", data);
                return 1;
            case 120:  //0x78 - Write playlist to current directory, we're not gonna implement this if we can help it, it's kinda ridiculous
                return (uintptr_t)"C:\\Playlist output is lame, reimplement";
            case 121:  //0x79 - Move to song # in playlist
                //Not sure I need to implement this with 0x78 nuked
                return 0x570917;
            case 122:  //0x7A - Set volume to Data as 0-255
                       //AOUT_VOLUME_DEFAULT leads me to belive that VLC also expresses volume as 0-256. No clude if it does, we're gonna assume so and see what happens. Throw science at the wall and see what sticks.
                playlist_VolumeSet(p_playlist, (float)data);
                return 1;
            case 123:  //0x7B - Set panning as 0-255 (left/right) - Not sure if possible in vlc
                return 0x570917; //I doubt this feature is used often and I don't see a way to implement it
            case 124:  //0x7C - Return length of playlist in terms of tracks
                return playlist_CurrentSize(p_playlist);
            case 125:  //0x7D - Get List Position in terms of tracks
                return p_playlist->i_current_index; //No idea if this is right or not, it may explode
                                                    //If it doesn't, maybe ItemIndex from playlist/item.c will work
            case 126:  //0x7F - Get Rate data = (0 -> samplerate (eg 44100hz), 1 -> bitrate (eg 256kbs), 2 -> number of channels (2 - stereo))
                //TODO: WA v5+ apparently has responses here for video files. Investigate.
                ///This is how they check for input types in ncurses.c
                ///for (int i = 0; i < item->i_es ; i++) {
                ///  i_audio += (item->es[i]->i_cat == AUDIO_ES);
                ///  i_video += (item->es[i]->i_cat == VIDEO_ES);
                ///}
                if (!p_input)  
                    return 0;
                input_item_t *p_item = input_GetItem(p_input);
                vlc_mutex_lock(&p_item->lock);
                input_stats_t *p_stats;
                int ret = 0;
                switch(data) {
                    case 0:
                        //TODO: Find out if there are very many ES, or what ES are for that matter
                        msg_Dbg(p_intf, "Troglator: there are this many ES: %i", p_item->i_es);
                        for (int i = 0; i < p_item->i_es ; i++) {
                            if(p_item->es[i]->audio.i_rate) {
                                ret = p_item->es[i]->audio.i_rate;
                                break;
                            }
                        }
                        break;
                    case 1:
                        p_stats = p_item->p_stats;
                        vlc_mutex_lock(&p_stats->lock);
                        ret = p_stats->f_input_bitrate*8000;
                        vlc_mutex_unlock(&p_stats->lock);
                        break;
                    case 2:
                        for (int i = 0; i < p_item->i_es ; i++) {
                            if(p_item->es[i]->audio.i_channels) {
                                ret = p_item->es[i]->audio.i_channels;
                                break;
                            }
                        }
                        break;
                }
                vlc_mutex_unlock(&p_item->lock);
                return ret;
            case 127: //No Eq access afaik
            case 128: //ditto
            case 129: //This is irrelevant to VLC
            case 135: //Why would you want to restart VLC?
            case 200: //I don't care about skins
            case 201: //ditto
                return -1;    
            case 202: //Visualizations are cool, might implement.
                return 0x570917;
            case 211:  //0xD3 - Get File - Theorhetically would return a pointer to the current file, it's flagged as in-process only so ignoring for now.
                return (uintptr_t)"C:\\NO - U.mp3"; //It occurs to me that strings are passed around pretty will nilly.
        }
    } else if (uMsg == WM_COMMAND) {
        int cid = (int)wParam;
        intf_thread_t *p_intf = (intf_thread_t *)(uintptr_t)
            GetWindowLongPtr( hwnd, GWLP_USERDATA );
        msg_Dbg(p_intf, "Troglator: Received WM_COMMAND ID:%i", cid);
        return 0;
    } else if (uMsg == WM_COPYDATA) {
        /*COPYDATASTRUCT *pwm_data = (COPYDATASTRUCT*)lParam;
        intf_thread_t *p_intf = (intf_thread_t *)(uintptr_t)
            GetWindowLongPtr( hwnd, GWLP_USERDATA );
        msg_Dbg(p_intf, "Troglator: Received WM_COPYDATA");
        return 0;*/
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

static int ItemChange( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *param )
{
    VLC_UNUSED(psz_var); VLC_UNUSED(oldval); VLC_UNUSED(newval);
    char *psz_title;
    char *psz_trackNumber;
    char *psz_artist;
    char  psz_tmp[256];
    input_thread_t *p_input = playlist_CurrentInput( (playlist_t*)p_this );
    intf_thread_t  *p_intf  = param;
    intf_sys_t     *p_sys   = p_intf->p_sys;

    if( !p_input ) return VLC_SUCCESS;
    if( p_input->b_dead )
    {
        /* Not playing anything ... */
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    
    input_item_t *p_input_item = input_GetItem( p_input );
    psz_title = input_item_GetTitleFbName(p_input_item);
    psz_trackNumber = input_item_GetTrackNumber(p_input_item);
    psz_artist = input_item_GetArtist(p_input_item);
    snprintf(psz_tmp, 256, "%s. %s - %s - Winamp", psz_trackNumber, psz_artist, psz_title);

    SetWindowTextA(p_sys->window, psz_tmp);

    free(psz_title);
    free(psz_trackNumber);
    free(psz_artist);
    vlc_object_release(p_input);
    return VLC_SUCCESS;
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

    var_AddCallback( pl_Get( p_intf ), "activity", ItemChange, p_intf );

    WaitForSingleObject(p_sys->ready, INFINITE);
    CloseHandle(p_sys->ready);

    return VLC_SUCCESS;
}

//Creates Window Thread
static void Close(vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t *)obj;
    intf_sys_t *sys = intf->p_sys;

    var_DelCallback( pl_Get( intf ), "activity", ItemChange, intf );

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
