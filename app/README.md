# System Monitor Desktop App

Native Ubuntu desktop client written in C with GTK3.

The app does not use the web frontend or the HTTP server. It links directly to the existing backend modules:

- `backend/src/proc_parser.c`
- `backend/src/history.c`

## Requirements

```bash
sudo apt install build-essential libgtk-3-dev
```

Optional GPU metrics require NVIDIA drivers and `nvidia-smi`.

## Build And Run

```bash
cd app
make check-deps
make
make run
```

The binary is created at:

```bash
app/build/system-monitor-app
```

The main window starts maximized and the dashboard is scrollable. Use the `Processes` button in the header, or the button in the process preview card, to open the full process list in a separate window.

## Desktop Entry

`packaging/system-monitor.desktop` is a template. Replace `/absolute/path/to/system-monitor` with the real project path before installing it to `~/.local/share/applications/`.
