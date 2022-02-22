/**
 * @file capture-key.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-02-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __CAPTURE_KEY_H__
#define __CAPTURE_KEY_H__

#include <glib-2.0/glib.h>

/**
 * @brief 
 * hid handler is a datastructure that wrap around handling of 
 */
typedef struct          _InputHandler                         InputHandler;

/**
 * @brief 
 * Hid Input is a datastructure that wrap around all parameter 
 * that captured by remoteapp related to human interface device
 */
typedef struct			_HidInput								HidInput;


/**
 * @brief 
 * 
 */
typedef void            (*HIDHandleFunction)                    (gchar* message,
                                                                 gpointer data);

/**
 * @brief 
 * 
 * @param mouse_code 
 * @param delta_X 
 * @param delta_Y 
 */
void                handle_window_mouse_relative    (gint mouse_code,
                                                    gint delta_X,
                                                    gint delta_Y);

/**
 * @brief 
 * 
 * @param isup 
 * @param app 
 */
void                handle_window_wheel             (gint isup);


/**
 * @brief 
 * 
 * @return InputHandler* 
 */
InputHandler*       init_input_capture_system       (HIDHandleFunction function,
                                                     gpointer data);

/**
 * @brief 
 * 
 */
void                reset_mouse                     ();

/**
 * @brief 
 * 
 */
void                reset_keyboard                  ();

/**
 * @brief 
 * 
 * @return InputHandler* 
 */
void                set_hid_handle_function         (HIDHandleFunction function);

#ifdef G_OS_WIN32
#include <Windows.h>
/**
 * @brief 
 * 
 * @param hwnd 
 * @param message 
 * @param wParam 
 * @param lParam 
 */
void                handle_message_window_proc      (HWND hwnd, 
                                                    UINT message, 
                                                    WPARAM wParam, 
                                                    LPARAM lParam);

#endif
#endif