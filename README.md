# System Monitor

Native Ubuntu desktop system monitor written in C.

System Monitor is a GTK3 desktop application that reads system metrics directly from Linux system interfaces and shows them in a native dashboard. The project does not use a web frontend, browser view, HTTP server, or JavaScript UI. The application is built around a small C backend and a native GTK app.

## Features

- CPU usage, core list, temperature and frequency.
- Memory usage with used/total values.
- GPU panel with source switcher for Integrated and NVIDIA.
- Integrated GPU support through DRM/sysfs, Intel sysfs and optional `intel_gpu_top`.
- NVIDIA GPU metrics through `nvidia-smi` when available.
- GPU usage, VRAM, temperature, power, clock and fan RPM when the hardware exposes them.
- Disk I/O read/write rates.
- Network RX/TX rates.
- Battery percentage, status, power draw and temperature.
- SSD/NVMe temperature and fan speed through `hwmon` when available.
- CPU/RAM/GPU history chart.
- Disk/network I/O history chart.
- Separate process window with search, sorting, refresh, `SIGTERM` and `SIGKILL`.
- Confirmation dialog before sending a signal to a process.
- Clear error dialog when process termination fails.
- Light and dark themes.
- Configurable refresh interval: `1s`, `2s`, `5s`.
- Settings persistence in `~/.config/system-monitor/config.ini`.
- Tray icon with quick show/hide and quit.
- Desktop notifications for high CPU/RAM/GPU load, high CPU/GPU temperature and low battery.
- Debian package generation with icon, desktop entry, changelog and package metadata.

## Design Goals

- Native desktop app for Ubuntu/Linux.
- Pure C codebase for the app and backend.
- No production fake data. If a metric cannot be read, the UI shows it as unavailable.
- Backend and UI are separated enough to keep metric collection testable.
- Metric collection runs outside the GTK UI thread so slow commands such as `nvidia-smi` do not freeze the interface.
- Tests cover backend parsers, history storage and app-level metric helpers.

## Project Layout

```text
.
├── Makefile
├── Makefile.test
├── README.md
├── app
│   ├── Makefile
│   ├── assets
│   │   └── system-monitor.svg
│   ├── packaging
│   │   └── system-monitor.desktop
│   └── src
│       ├── app_metrics.c
│       ├── app_metrics.h
│       ├── app_state.h
│       ├── main.c
│       ├── metrics_sampler.c
│       ├── metrics_sampler.h
│       ├── notifications.c
│       ├── notifications.h
│       ├── theme.c
│       └── theme.h
├── backend
│   ├── Makefile
│   └── src
│       ├── config.h
│       ├── history.c
│       ├── history.h
│       ├── proc_parser.c
│       └── proc_parser.h
└── tests
    ├── test_app_metrics.c
    ├── test_config.h
    ├── test_history.c
    ├── test_proc_parser.c
    └── test_runner.c
```

## Architecture

The project has three main layers.

### Backend

The backend lives in `backend/src`.

- `proc_parser.c` reads live system data from Linux interfaces.
- `history.c` stores fixed-size metric history for charts.
- `config.h` contains shared structs and constants.

The backend reads from:

- `/proc/stat`
- `/proc/meminfo`
- `/proc/diskstats`
- `/proc/net/dev`
- `/proc/<pid>/...`
- `/sys/class/drm`
- `/sys/class/hwmon`
- `/sys/class/power_supply`

Optional external tools:

- `nvidia-smi` for NVIDIA GPU metrics.
- `intel_gpu_top` for richer Intel integrated GPU usage, if installed and permitted.
- `sensors`/`lscpu` fallbacks may be used for CPU data when direct sysfs reads are not enough.

### Desktop App

The app lives in `app/src`.

- `main.c` builds the GTK UI and wires callbacks.
- `app_state.h` contains shared app state and snapshot structs.
- `metrics_sampler.c` collects a metrics snapshot in a background task.
- `notifications.c` handles desktop notifications and error dialogs.
- `theme.c` owns the light/dark CSS.
- `app_metrics.c` contains small testable helpers for formatting, clamping and alert state.

Metric collection flow:

```text
GTK timer
  -> starts GTask worker
  -> backend reads /proc, /sys and optional tools
  -> worker returns MetricsSnapshot
  -> GTK main thread applies snapshot to UI
  -> charts/history/process list update
```

### Tests

Tests live in `tests`.

The test runner covers:

- proc parser behavior;
- history ring buffer behavior;
- app metric helpers;
- alert threshold behavior;
- battery alert eligibility;
- sensor parser availability.

Some parser tests are integration-style tests because they read the current machine's `/proc` and `/sys`.

## Requirements

Install build tools and GTK3 development headers:

```bash
sudo apt install build-essential libgtk-3-dev
```

Recommended runtime tools:

```bash
sudo apt install procps coreutils
```

Optional hardware tools:

```bash
sudo apt install lm-sensors intel-gpu-tools
```

NVIDIA metrics require the NVIDIA driver stack and `nvidia-smi`.

## Build

Check GTK dependencies:

```bash
make -C app check-deps
```

Build everything:

```bash
make
```

The binary is created at:

```text
app/build/system-monitor-app
```

## Run

```bash
make run
```

Or run the binary directly:

```bash
./app/build/system-monitor-app
```

## Test

Run the full test suite:

```bash
make test
```

Run Valgrind through the test Makefile:

```bash
make -f Makefile.test valgrind
```

Clean build and test artifacts:

```bash
make clean
```

Expected current result:

```text
24 tests passed, 0 failed
```

## Debian Package

Build the `.deb` package:

```bash
make package-deb
```

The package is created at:

```text
dist/system-monitor_3.0.0_amd64.deb
```

Install it locally:

```bash
sudo apt install ./dist/system-monitor_3.0.0_amd64.deb
```

The package installs:

- `/usr/local/bin/system-monitor`
- `/usr/share/applications/system-monitor.desktop`
- `/usr/share/icons/hicolor/scalable/apps/system-monitor.svg`
- `/usr/share/doc/system-monitor/changelog.Debian.gz`
- `/usr/share/doc/system-monitor/copyright`

## Settings

User settings are stored at:

```text
~/.config/system-monitor/config.ini
```

Currently saved:

- selected theme;
- selected GPU source;
- window width and height;
- refresh interval.

Supported refresh intervals:

- `1s`
- `2s`
- `5s`

## GPU Notes

Integrated GPU support depends on what the system exposes.

For Intel integrated GPUs, the app tries:

- DRM/sysfs card discovery;
- GT frequency from sysfs;
- optional `intel_gpu_top -J` for usage when available.

For AMD integrated GPUs, the app uses DRM/hwmon data when exposed by the kernel.

For NVIDIA GPUs, the app tries:

- `nvidia-smi`;
- DRM/hwmon fallback for basic data if available.

If a GPU metric is unavailable, the app shows it as unavailable instead of inventing a value.

## Sensor Notes

Storage temperature and fan speed are read from `/sys/class/hwmon`.

Availability depends on:

- kernel driver support;
- sensor labels exposed by the hardware;
- permissions;
- laptop/desktop firmware behavior.

If no sensor is available, the app shows `Unavailable`.

## No Fake Production Data

The backend intentionally avoids fake production values. If a source such as `/proc`, `/sys`, `nvidia-smi` or `intel_gpu_top` cannot provide data, backend functions return an error or leave the metric unavailable. The UI then displays an unavailable state.

This is important because a system monitor should not silently show realistic-looking but false values.

## Useful Commands

```bash
make
make run
make test
make clean
make package-deb
```

## Current Limitations

- `GtkStatusIcon` is deprecated and may require tray support or shell extensions on some desktops.
- `intel_gpu_top` may require permissions or may be absent.
- NVIDIA metrics depend on `nvidia-smi`.
- Some sensors are not exposed on every laptop or motherboard.
- Process CPU usage needs at least two samples to become meaningful.
- Some tests depend on the current machine's live `/proc` and `/sys`.

## Roadmap

- Move the remaining UI code from `main.c` into dedicated dashboard, process window and tray modules.
- Replace `GtkStatusIcon` with AppIndicator/libayatana-appindicator.
- Add fixture-based parser tests for stable CI.
- Add `make sanitize` for ASan/UBSan.
- Add AppImage packaging.
- Add per-disk and per-network-interface selection.
- Add configurable notification thresholds.
- Add process details view.
