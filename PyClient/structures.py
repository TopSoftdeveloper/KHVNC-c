from enum import IntEnum
import ctypes
from ctypes import wintypes

class Connection(IntEnum):
    desktop = 0
    input = 1

WM_USER = 0x0400  # Assuming WM_USER is 0x0400

class WmStartApp(IntEnum):
    startExplorer = WM_USER + 1
    startRun = startExplorer + 1
    startChrome = startRun + 1
    startEdge = startChrome + 1
    startBrave = startEdge + 1
    startFirefox = startBrave + 1
    startIexplore = startFirefox + 1
    startPowershell = startIexplore + 1
    startItau = startPowershell + 1
    monitorItau = startItau + 1

class POINT(ctypes.Structure):
    _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]

# Define the RECT structure
class RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_int),
                ("top", ctypes.c_int),
                ("right", ctypes.c_int),
                ("bottom", ctypes.c_int)]

class WINDOWPLACEMENT(ctypes.Structure):
    _fields_ = [
        ('length', wintypes.UINT),
        ('flags', wintypes.UINT),
        ('showCmd', wintypes.UINT),
        ('ptMinPosition', wintypes.POINT),
        ('ptMaxPosition', wintypes.POINT),
        ('rcNormalPosition', RECT)
    ]
