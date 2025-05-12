# KHVNC - Hidden Virtual Network Computing (HVNC)

**KHVNC** is a remote desktop solution designed for educational and research purposes. Unlike traditional remote desktop software such as AnyDesk, TeamViewer, or VNC, KHVNC allows you to create and interact with a **hidden virtual desktop session** on a remote machine. This ensures that user activity does not interfere with the physical screen of the remote system.

> ⚠️ **Disclaimer**: This project is intended strictly for educational, ethical, and research purposes. Misuse of this software may violate privacy laws and software use policies. The author assumes no responsibility for misuse.

---

## 🔍 What is Hidden VNC?

Hidden VNC (HVNC) or virtual screen control allows a user to spawn a **separate desktop session** on a remote machine that is invisible to the local user. This virtual session can be used to:

- Run applications
- Execute scripts
- Test GUI software
- Automate browser tasks  
All without disrupting the user on the physical console.

---

## 🚀 Features

- ✅ Launch Hidden Desktop Session (Explorer)
- 🏁 Access "Run" dialog
- 🖥️ Start PowerShell
- 🌐 Launch multiple browsers in background:
  - Google Chrome
  - Microsoft Edge
  - Brave Browser
  - Mozilla Firefox
  - Internet Explorer
- 💼 Start specific apps (e.g., Itaú Banking Software - optional)

---

## 🧱 Compiled Structure

- `Client.exe` – Main client used to capture images and send interaction commands.
- `Server.exe` – Server-side binary to host the hidden desktop and manage sessions.

---

## 🛠️ How to Use

1. Run `Server.exe` on the **remote machine** where you want to initiate a virtual desktop.
2. Run `Client.exe` on the **local machine** to connect and control the remote virtual screen.

> 📌 Note: Both binaries must be on the same network or configured with port forwarding if used remotely.

---

## 🎥 Demo Video

[![Watch Demo](https://img.icons8.com/ios-filled/100/000000/play-button-circled.png)](./demo.mp4)

[![Watch Demo](https://img.icons8.com/ios-filled/100/000000/play-button-circled.png)](./open_chrome.mp4)


## 🔄 Python Version (Coming Soon)

We are working on converting this HVNC concept into a **Python-based version** for cross-platform support and easier integration with automation tools.

Stay tuned!

---

## 📬 Contact

Have questions or want to contribute?

- **Telegram**: [@worksnipz](https://t.me/worksnipz)
- **Donate(BTC)**: bc1q43u0n865fuxc4j2vgm4wp98xuuaawgkgq8yrf4
---

## 🧠 Related Keywords
Virtual Screen, Windows, Remote Control, Hide, Hidden
