import ctypes
from ctypes import wintypes, windll
from win32gui import GetDesktopWindow, GetWindowRect, GetDC
from win32con import *
import win32gui
import win32api
import win32con
import win32print
import numpy as np
from bitmap import *
import win32ui
from PIL import Image
from structures import *

# Define necessary Windows API functions
user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32


class EnumHwndsPrintData:
    def __init__(self, hDc, hDcScreen):
        self.hDc = hDc
        self.hDcScreen = hDcScreen

def is_window_visible(hWnd):
    """Check if the window is visible."""
    return win32gui.IsWindowVisible(hWnd)

def paint_window(hWnd, hDc, hDcScreen):
    """Paint the window using the provided device contexts."""
    # Get the window's rectangle
    rect_tuple  = win32gui.GetWindowRect(hWnd)
    rect = RECT(rect_tuple[0], rect_tuple[1], rect_tuple[2], rect_tuple[3])

    # Create compatible DC and bitmap
    hDcWindow = win32gui.CreateCompatibleDC(hDc)
    width = rect.right - rect.left
    height = rect.bottom - rect.top
    hBmpWindow = win32gui.CreateCompatibleBitmap(hDc, width, height)

    # Select the bitmap into the DC
    win32gui.SelectObject(hDcWindow, hBmpWindow)

    # Capture the window into the compatible bitmap
    ret = False
    if windll.user32.PrintWindow(hWnd, hDcWindow, 0):
        # Copy the captured bitmap to the screen DC using BitBlt
        win32gui.BitBlt(hDcScreen,
                        rect.left,
                        rect.top,
                        width,
                        height,
                        hDcWindow,
                        0,
                        0,
                        win32con.SRCCOPY)
        ret = True

    # Clean up resources
    win32gui.DeleteObject(hBmpWindow)
    win32gui.DeleteDC(hDcWindow)

    return ret

def enum_hwnds_print(hWnd, lParam):
    """Callback function for each window in the enum."""
    data = lParam
    if not is_window_visible(hWnd):
        return True

    paint_window(hWnd, data.hDc, data.hDcScreen)
    return True

def enum_windows_top_to_down(owner, proc, param):
    """Enumerate windows from top to bottom and call the given callback for each."""
    current_window = win32gui.GetTopWindow(owner)
    if current_window == 0:
        return
    if (current_window := win32gui.GetWindow(current_window, win32con.GW_HWNDLAST)) is None:
        return

    while proc(current_window, param):
        try:
            if current_window is None or current_window == 0:
                break
            current_window = win32gui.GetWindow(current_window, win32con.GW_HWNDPREV)
        except Exception as e:
            print(e)

HBITMAP = ctypes.c_void_p

def get_bitmap_bits(hbitmap):
    # Select the HBITMAP into the memory DC
    bitmap = win32ui.CreateBitmapFromHandle(hbitmap)

    # Get bitmap info
    bmpinfo = bitmap.GetInfo()
    bmpstr = bitmap.GetBitmapBits(True)

    # Convert to PIL Image
    image = Image.frombuffer(
        'RGB',
        (bmpinfo['bmWidth'], bmpinfo['bmHeight']),
        bmpstr, 'raw', 'BGRX', 0, 1
    )

    # Save image
    # image.save("1.BMP")
    return image

def _get_desk_pixels(server_width, server_height):
    # Get desktop window handle
    hWndDesktop = GetDesktopWindow()

    # Get the dimensions of the desktop window
    rect = RECT()
    rect_tuple = GetWindowRect(hWndDesktop)
    rect = RECT(rect_tuple[0], rect_tuple[1], rect_tuple[2], rect_tuple[3])
    #GetWindowRect(hWndDesktop, ctypes.byref(rect))

    # Get device context for the screen
    hDc = GetDC(0)

    # Create a compatible device context and bitmap
    hDcScreen = gdi32.CreateCompatibleDC(hDc)
    hBmpScreen = gdi32.CreateCompatibleBitmap(hDc, rect.right - rect.left, rect.bottom - rect.top)

    # Select the bitmap into the device context
    gdi32.SelectObject(hDcScreen, hBmpScreen)

    # Clean up: Release the device context after usage
    data = EnumHwndsPrintData(hDc, hDcScreen)
    enum_windows_top_to_down(None, enum_hwnds_print, data)

    screen_width = rect.right
    screen_height = rect.bottom

    if server_width > screen_width:
        server_width = screen_width
    if server_height > screen_height:
        server_height = screen_height
    hBmpScreenResized = win32gui.CreateCompatibleBitmap(hDc, server_width, server_height)
    hDcScreenResized = win32gui.CreateCompatibleDC(hDc)
    
    win32gui.SelectObject(hDcScreenResized, hBmpScreenResized)
    win32gui.SetStretchBltMode(hDcScreenResized, win32con.HALFTONE)
    win32gui.StretchBlt(hDcScreenResized, 0, 0, server_width, server_height,
                        hDcScreen, 0, 0, screen_width, screen_height, win32con.SRCCOPY)
    image = get_bitmap_bits(hBmpScreenResized)

    win32gui.DeleteObject(hBmpScreenResized)
    win32gui.DeleteObject(hBmpScreen)
    win32gui.DeleteDC(hDcScreenResized)
    gdi32.DeleteDC(hDcScreen)
    user32.ReleaseDC(0, hDc)

    return image
