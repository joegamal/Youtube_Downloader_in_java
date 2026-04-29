"""
YouTube Downloader - Windows 7 x32 Compatible
Requirements: pip install yt-dlp
Python 3.8 x86 recommended for Win7 x32
"""

import os
import sys
import threading
import tkinter as tk
from tkinter import ttk, messagebox, filedialog


# ---------------------------------------------------------------------------
# Try to import yt-dlp; show helpful error if missing
# ---------------------------------------------------------------------------
try:
    import yt_dlp
except ImportError:
    root = tk.Tk()
    root.withdraw()
    messagebox.showerror(
        "Missing dependency",
        "yt-dlp is not installed.\n\nPlease run:\n  pip install yt-dlp\n\nthen restart the application."
    )
    sys.exit(1)


# ---------------------------------------------------------------------------
# Main Application
# ---------------------------------------------------------------------------
class YouTubeDownloaderApp(tk.Tk):
    def __init__(self):
        super().__init__()

        self.title("YouTube Downloader Crafted by Yusuf Gamal")
        self.resizable(False, False)
        self._center_window(460, 260)
        self.configure(bg="#1e1e2e")

        self._build_ui()

    # ------------------------------------------------------------------
    # UI Construction
    # ------------------------------------------------------------------
    def _build_ui(self):
        PAD = 18
        BG        = "#1e1e2e"
        FG        = "#cdd6f4"
        ENTRY_BG  = "#313244"
        ENTRY_FG  = "#cdd6f4"
        BTN_BG    = "#89b4fa"
        BTN_FG    = "#1e1e2e"
        BAR_COLOR = "#89b4fa"

        tk.Label(
            self, text="YouTube Downloader",
            bg=BG, fg="#89b4fa",
            font=("Segoe UI", 14, "bold")
        ).pack(pady=(PAD, 4))

        tk.Label(
            self, text="Video URL",
            bg=BG, fg=FG,
            font=("Segoe UI", 9)
        ).pack(anchor="w", padx=PAD)

        self.url_var = tk.StringVar()
        tk.Entry(
            self,
            textvariable=self.url_var,
            bg=ENTRY_BG, fg=ENTRY_FG,
            insertbackground=ENTRY_FG,
            relief="flat",
            font=("Segoe UI", 10),
            width=50
        ).pack(padx=PAD, pady=(2, 10), ipady=6, fill="x")

        self.download_btn = tk.Button(
            self,
            text="  Download",
            command=self._on_download_click,
            bg=BTN_BG, fg=BTN_FG,
            activebackground="#74c7ec",
            activeforeground=BTN_FG,
            relief="flat",
            font=("Segoe UI", 10, "bold"),
            cursor="hand2",
            padx=10, pady=6
        )
        self.download_btn.pack(padx=PAD, pady=(0, 10))

        style = ttk.Style(self)
        style.theme_use("default")
        style.configure(
            "blue.Horizontal.TProgressbar",
            troughcolor="#313244",
            background=BAR_COLOR,
            bordercolor="#1e1e2e",
            lightcolor=BAR_COLOR,
            darkcolor=BAR_COLOR
        )

        self.progress_var = tk.DoubleVar(value=0.0)
        ttk.Progressbar(
            self,
            variable=self.progress_var,
            maximum=100,
            style="blue.Horizontal.TProgressbar",
            length=420
        ).pack(padx=PAD, pady=(0, 6))

        self.status_var = tk.StringVar(value="Paste a YouTube URL and press Download.")
        tk.Label(
            self,
            textvariable=self.status_var,
            bg=BG, fg="#a6adc8",
            font=("Segoe UI", 8),
            wraplength=420,
            justify="left"
        ).pack(padx=PAD, anchor="w")

    # ------------------------------------------------------------------
    # Button handler
    # ------------------------------------------------------------------
    def _on_download_click(self):
        url = self.url_var.get().strip()
        if not url:
            messagebox.showwarning("No URL", "Please enter a YouTube video URL.")
            return

        dest_dir = filedialog.askdirectory(title="Choose destination folder")
        if not dest_dir:
            return

        self._set_ui_busy(True)
        self.progress_var.set(0)
        self.status_var.set("Starting download...")

        threading.Thread(
            target=self._download,
            args=(url, dest_dir),
            daemon=True
        ).start()

    # ------------------------------------------------------------------
    # Download logic (background thread)
    # ------------------------------------------------------------------
    def _download(self, url, dest_dir):
        outtmpl = os.path.join(dest_dir, "%(title)s.%(ext)s")

        ydl_opts = {
            "outtmpl": outtmpl,
            # "best" selects ONLY pre-merged single-file formats.
            # yt-dlp will NEVER attempt to combine separate streams,
            # so ffmpeg is never called under any circumstance.
            "format": "best",
            "progress_hooks": [self._progress_hook],
            "quiet": True,
            "no_warnings": True,
            # Disable all post-processors explicitly
            "postprocessors": [],
            "prefer_ffmpeg": False,
        }

        try:
            with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                ydl.download([url])
            self.after(0, self._on_success)
        except yt_dlp.utils.DownloadError as exc:
            self.after(0, self._on_error, str(exc))
        except Exception as exc:
            self.after(0, self._on_error, str(exc))

    # ------------------------------------------------------------------
    # yt-dlp progress hook
    # ------------------------------------------------------------------
    def _progress_hook(self, d):
        status = d.get("status", "")

        if status == "downloading":
            downloaded = d.get("downloaded_bytes", 0) or 0
            total      = d.get("total_bytes") or d.get("total_bytes_estimate") or 0
            speed_raw  = d.get("speed") or 0
            eta        = d.get("eta") or 0

            percent = (downloaded / total * 100) if total else 0
            speed   = self._fmt_size(speed_raw) + "/s" if speed_raw else ""
            msg     = "Downloading...  {:.1f}%  {}  ETA {}s".format(percent, speed, eta)

            self.after(0, self.progress_var.set, percent)
            self.after(0, self.status_var.set, msg)

        elif status == "finished":
            self.after(0, self.progress_var.set, 99)
            self.after(0, self.status_var.set, "Finishing...")

    # ------------------------------------------------------------------
    # Callbacks (UI thread)
    # ------------------------------------------------------------------
    def _on_success(self):
        self.progress_var.set(100)
        self.status_var.set("Download complete!")
        self._set_ui_busy(False)
        messagebox.showinfo("Done", "Video downloaded successfully!")
        self.url_var.set("")
        self.progress_var.set(0)
        self.status_var.set("Paste a YouTube URL and press Download.")

    def _on_error(self, msg):
        self.progress_var.set(0)
        self.status_var.set("Error: " + msg[:120])
        self._set_ui_busy(False)
        messagebox.showerror("Download failed", msg)

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------
    def _set_ui_busy(self, busy):
        self.download_btn.config(state="disabled" if busy else "normal")

    @staticmethod
    def _fmt_size(num_bytes):
        for unit in ("B", "KB", "MB", "GB"):
            if abs(num_bytes) < 1024.0:
                return "{:.1f} {}".format(num_bytes, unit)
            num_bytes /= 1024.0
        return "{:.1f} TB".format(num_bytes)

    def _center_window(self, w, h):
        self.update_idletasks()
        sw = self.winfo_screenwidth()
        sh = self.winfo_screenheight()
        x  = (sw - w) // 2
        y  = (sh - h) // 2
        self.geometry("{}x{}+{}+{}".format(w, h, x, y))


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    app = YouTubeDownloaderApp()
    app.mainloop()