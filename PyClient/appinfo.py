import winreg
import os

def get_installation_apps_list():
    software_info = []
    reg_paths = [
        (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"),
        (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall"),
        (winreg.HKEY_CURRENT_USER, r"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"),
    ]

    for hive, path in reg_paths:
        try:
            reg_key = winreg.OpenKey(hive, path)
            for i in range(winreg.QueryInfoKey(reg_key)[0]):
                try:
                    subkey_name = winreg.EnumKey(reg_key, i)
                    subkey = winreg.OpenKey(reg_key, subkey_name)

                    # Get values with fallback if missing
                    display_name = ""
                    install_source = ""
                    install_location = ""

                    try:
                        display_name, _ = winreg.QueryValueEx(subkey, "DisplayName")
                    except FileNotFoundError:
                        pass

                    try:
                        install_source, _ = winreg.QueryValueEx(subkey, "InstallSource")
                    except FileNotFoundError:
                        pass

                    try:
                        install_location, _ = winreg.QueryValueEx(subkey, "InstallLocation")
                    except FileNotFoundError:
                        pass

                    if install_source or install_location:
                        software_info.append({
                            "DisplayName": display_name,
                            "InstallSource": install_source,
                            "InstallLocation": install_location
                        })

                    winreg.CloseKey(subkey)
                except (FileNotFoundError, OSError):
                    continue
        except FileNotFoundError:
            continue

    return software_info
