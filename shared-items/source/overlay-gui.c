/**
 * @file remote-webrtc-gui.c
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2021-12-08
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include <overlay-gui.h>


#include <virtual-key.h>
#include <capture-key.h>

#include <glib-2.0/glib.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <gst/video/gstvideosink.h>



struct _GUI
{
    /**
     * @brief 
     * reference to remote app
     */
    gpointer app;

#ifdef G_OS_WIN32
    /**
     * @brief 
     * win32 window for display video
     */
    HWND window;

    /**
     * @brief 
     * revious window style
     */
    LONG prev_style;

    /**
     * @brief 
     * previous rectangle size, use for setup fullscreen mode
     */
    RECT prev_rect;

    /**
     * @brief 
     * window retangle, size and position of window
     */
    RECT wr;

    /**
     * @brief 
     * IO channel for remote app
     */
    GIOChannel *msg_io_channel;
#endif

    /**
     * @brief 
     * gst video sink element
     */
    GstElement* sink_element;

    /**
     * @brief 
     * fullscreen mode on or off
     */
    gboolean fullscreen;


    /**
     * @brief 
     * 
     */
    gboolean disable_client_cursor;

    /**
     * @brief 
     * 
     */
    InputHandler* handler;

    /**
     * @brief 
     * 
     */
    ResetApplicationEvent reset;  
};


/**
 * @brief Set the up window object
 * 
 * @param gui 
 */
void                        set_up_window           (GUI* gui);

/**
 * @brief 
 * 
 */
void                        handle_fullscreen_hotkey ();

/**
 * @brief 
 * 
 */
void                        toggle_client_cursor                    ();

static GUI _gui = {0};

GUI*
init_remote_app_gui(gpointer app,   
                    ResetApplicationEvent reset)
{
    GUI* gui = &_gui;
    gui->handler =  init_input_capture_system(NULL,app);
    gui->app = app;
    gui->reset = reset;
#ifdef G_OS_WIN32
    gui->wr = (RECT) { 0, 0, 320, 240 };
    set_up_window(gui);
#endif
    return gui;
}

#ifdef G_OS_WIN32
#include <windows.h>
#include <WinUser.h>
#include <libloaderapi.h>
#include <Xinput.h>
#include <hidusage.h>



/**
 * @brief 
 * window procedure function
 * @param hWnd 
 * @param message 
 * @param wParam 
 * @param lParam 
 * @return LRESULT 
 */
static LRESULT CALLBACK       window_proc             (HWND hWnd, 
                                                      UINT message, 
                                                      WPARAM wParam, 
                                                      LPARAM lParam);

/**
 * @brief 
 * 
 * @param source 
 * @param condition 
 * @param data 
 * @return gboolean 
 */
static gboolean
msg_cb (GIOChannel * source, 
        GIOCondition condition, 
        gpointer data)
{
  MSG msg;

  if (!PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    return G_SOURCE_CONTINUE;

  TranslateMessage (&msg);
  DispatchMessage (&msg);

  return G_SOURCE_CONTINUE;
}

/**
 * @brief Set the up window object
 * setup window 
 * @param gui 
 * @return HWND 
 */
static void
set_up_window(GUI* gui)
{
    HINSTANCE hinstance = GetModuleHandle(NULL);
    WNDCLASSEX wc = { 0, };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW; //// ????
    wc.lpfnWndProc = (WNDPROC)window_proc;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursor(NULL, IDC_NO);
    wc.lpszClassName = "GstWIN32VideoOverlay";
    RegisterClassEx(&wc);

    AdjustWindowRect (&(gui->wr), WS_OVERLAPPEDWINDOW, FALSE);
    gui->window = CreateWindowEx(0, wc.lpszClassName,
                          "Thinkremote",
                          WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          gui->wr.right  - gui->wr.left, 
                          gui->wr.bottom - gui->wr.top, 
                          (HWND)NULL, (HMENU)NULL,
                          hinstance, NULL);
    gui->msg_io_channel = g_io_channel_win32_new_messages (0);
    g_io_add_watch (gui->msg_io_channel, G_IO_IN, msg_cb, NULL);

	{
		RAWINPUTDEVICE devices[1];
		devices[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
		devices[0].usUsage = HID_USAGE_GENERIC_KEYBOARD;
		devices[0].dwFlags = RIDEV_NOLEGACY | RIDEV_NOHOTKEYS;
		devices[0].hwndTarget = gui->window;

		BOOL registered = RegisterRawInputDevices(devices, ARRAYSIZE(devices),
			sizeof(RAWINPUTDEVICE));
		if(registered == FALSE)
		{
			g_printerr("Registering keyboard and/or mouse as Raw Input devices failed!");
		}
	}
}


/**
 * @brief 
 * handle message from gbus of gstreamer pipeline
 * @param bus 
 * @param msg 
 * @param user_data 
 * @return gboolean 
 */
static gboolean
bus_msg (GstBus * bus, 
        GstMessage * msg, 
        gpointer user_data)
{
  GstElement *pipeline = GST_ELEMENT (user_data);
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ASYNC_DONE:
      break;
  }

  return TRUE;
}




gpointer
setup_video_overlay(GUI* gui,
                    GstElement* videosink, 
                    GstElement* pipeline)
{
    /* prepare the pipeline */
    gst_object_ref_sink (videosink);
    gst_bus_add_watch (GST_ELEMENT_BUS (pipeline), bus_msg, pipeline);

    ShowWindow (gui->window, SW_SHOW);

    gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (videosink),
        (guintptr) gui->window);

}




/**
 * @brief Get the monitor size object
 * 
 * @param rect 
 * @param hwnd 
 * @return gboolean 
 */
static gboolean
get_monitor_size(RECT *rect, 
                 HWND *hwnd)
{
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEX monitor_info;
    DEVMODE dev_mode;

    monitor_info.cbSize = sizeof(monitor_info);
    if (!GetMonitorInfo(monitor, (LPMONITORINFO)&monitor_info))
    {
        return FALSE;
    }

    dev_mode.dmSize = sizeof(dev_mode);
    dev_mode.dmDriverExtra = sizeof(POINTL);
    dev_mode.dmFields = DM_POSITION;
    if (!EnumDisplaySettings(monitor_info.szDevice, ENUM_CURRENT_SETTINGS, &dev_mode))
    {
        return FALSE;
    }

    SetRect(rect, 0, 0, dev_mode.dmPelsWidth, dev_mode.dmPelsHeight);

    return TRUE;
}








void 
switch_fullscreen_mode(GUI* gui)
{
    long _prev_style;

    gui->fullscreen = !gui->fullscreen;

    if (!gui->fullscreen)
    {
        /* Restore the window's attributes and size */
        SetWindowLong(gui->window, GWL_STYLE, gui->prev_style);

        SetWindowPos(gui->window, HWND_NOTOPMOST,
                    gui->prev_rect.left,
                    gui->prev_rect.top,
                    1556 - gui->prev_rect.left,
                    884 - gui->prev_rect.top, SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(gui->window, SW_NORMAL);
    }
    else
    {
        RECT fullscreen_rect;

        /* show window before change style */
        ShowWindow(gui->window, SW_SHOW);

        /* Save the old window rect so we can restore it when exiting
        * fullscreen mode */
        GetWindowRect(gui->window, &(gui->prev_rect));
        gui->prev_style = GetWindowLong(gui->window, GWL_STYLE);

        if (!get_monitor_size(&fullscreen_rect, gui->window))
        {
          g_warning("Couldn't get monitor size");

          gui->fullscreen = !gui->fullscreen;
          return;
        }

        /* Make the window borderless so that the client area can fill the screen */
        _prev_style = gui->prev_style;
        SetWindowLong(gui->window, GWL_STYLE,
                      _prev_style &
                          ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU |
                            WS_THICKFRAME));
        gui->prev_style = _prev_style;
        SetWindowPos(gui->window, HWND_NOTOPMOST,
                    fullscreen_rect.left,
                    fullscreen_rect.top,
                    fullscreen_rect.right,
                    fullscreen_rect.bottom, SWP_FRAMECHANGED | SWP_NOACTIVATE);
        ShowWindow(gui->window, SW_MAXIMIZE);
    }
}



/**
 * @brief 
 * 
 * @param app 
 * @param x 
 * @param y 
 * @param width 
 * @param height 
 * @return gboolean 
 */
static gboolean
adjust_video_position(GUI* gui, 
                      gint x, 
                      gint y, 
                      gint width, 
                      gint height)
{
    gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(gui->sink_element), 
                                            x, 
                                            y, 
                                            width, 
                                            height);
}





/**
 * @brief 
 * detect if a key is pressed
 * @param key 
 * @return gboolean 
 */
static gboolean
_keydown(int *key)
{
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}


/**
 * @brief 
 * center mouse to center of the screee
 * @param gui 
 * @return RECT new position of the screen 
 */
POINT
center_mouse_position(GUI* gui)
{
    POINT pt;
    RECT rect;
    GetWindowRect(gui->window,&rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    pt.x = width /2;
    pt.y = height /2;
    ClientToScreen(gui->window, &pt);
    SetCursorPos(pt.x,pt.y);
    ScreenToClient(gui->window,&pt);
    return pt; 
}



/**
 * @brief 
 * 
 */
void
handle_fullscreen_hotkey()
{
    GUI* gui = &_gui;
    switch_fullscreen_mode(gui);
    toggle_client_cursor(gui);

    HIDHandleFunction function = !gui->disable_client_cursor ? NULL : NULL;
    set_hid_handle_function(function);

    /**
     * @brief 
     * reset mouse and keyboard to prevent key stuck
     */
    if(gui->disable_client_cursor)
    {
        reset_keyboard();
        reset_mouse();
    }
}


/**
 * @brief 
 * 
 * @return gboolean TRUE if the input signal should be disabled
 */
gboolean
handle_user_shortcut()
{
    GUI* gui = &_gui;
    if (_keydown(VK_SHIFT))
    {
        if (_keydown(VK_MENU))
        {
            if (_keydown(VK_CONTROL))
            {
                /**
                 * @brief 
                 * handle mouse lock key
                 */
                if (_keydown(F_KEY))
                {
                    handle_fullscreen_hotkey();
                    return TRUE;
                }

                /**
                 * @brief 
                 * handle reset video stream 
                 */
                else if (_keydown(W_KEY))
                {
                    gui->reset(gui->app);
                    return TRUE;
                }

                /**
                 * @brief 
                 * handle reset video stream 
                 */
                else if (_keydown(VK_OEM_PLUS))
                {
                    return TRUE;
                }


                /**
                 * @brief 
                 * handle reset video stream 
                 */
                else if (_keydown(VK_OEM_MINUS))
                {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

/**
 * @brief 
 * window proc responsible for handling message from window HWND
 * @param hWnd 
 * @param message 
 * @param wParam 
 * @param lParam 
 * @return LRESULT 
 */
static LRESULT CALLBACK
window_proc(HWND hWnd, 
            UINT message, 
            WPARAM wParam, 
            LPARAM lParam)
{
    GUI* gui = &_gui;

    if (message == WM_DESTROY) 
        g_assert_nonnull(NULL);

    if (message == WM_INPUT)
    {
        if(!handle_user_shortcut())
            handle_message_window_proc(hWnd, message, wParam, lParam );
    }
    else if (message == WM_MOUSEMOVE    ||
             message == WM_LBUTTONDOWN	||
             message == WM_LBUTTONUP	||
             message == WM_MBUTTONDOWN	||
             message == WM_MBUTTONUP	||
             message == WM_RBUTTONDOWN	||
             message == WM_RBUTTONUP	||
             message == WM_XBUTTONDOWN	||
             message == WM_XBUTTONUP	)
    {
        if(!gui->disable_client_cursor)
            goto end;

        gint x = LOWORD(lParam);
		gint y = HIWORD(lParam);
        POINT pt = center_mouse_position(gui);
        handle_window_mouse_relative(message, x-pt.x, y-pt.y);
    }
    else if (message == WM_MOUSEWHEEL)
    {		
		gboolean up = (GET_WHEEL_DELTA_WPARAM(wParam) > 0);
        handle_window_wheel(up);
    }

end:
    return DefWindowProc(hWnd, message, wParam, lParam);
}



/**
 * @brief 
 * 
 * @param gui 
 */
static void 
adjust_window(GUI* gui)
{
    AdjustWindowRect(&(gui->wr), WS_OVERLAPPEDWINDOW, FALSE);
}



void
toggle_client_cursor(GUI* gui)
{
    gui->disable_client_cursor = !gui->disable_client_cursor;
    ShowCursor(!gui->disable_client_cursor);
}



void
gui_terminate(GUI* gui)
{
    DestroyWindow(gui->window);
}


#else
void
gui_terminate(GUI* gui)
{
}

gpointer
setup_video_overlay(GstElement* videosink, 
                    RemoteApp* app)
{
    GUI* gui = remote_app_get_gui(app);
    Pipeline* pipeline = remote_app_get_pipeline(app);
    GstElement* pipe_element = pipeline_get_pipeline_element(pipeline);

    /* prepare the pipeline */
    gst_object_ref_sink (videosink);



}
#endif