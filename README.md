# System Monitor

Native Ubuntu desktop system monitor written in C.

The GTK desktop app reads system data directly through the C backend modules.

## Project Layout

- `app/` - GTK3 desktop application.
- `backend/src/proc_parser.*` - CPU, memory, GPU and process data collection.
- `backend/src/history.*` - fixed-size history ring buffer for charts.
- `tests/` - unit and integration-style tests for backend and app logic.

## Requirements

```bash
sudo apt install build-essential libgtk-3-dev
```

Optional NVIDIA GPU metrics require `nvidia-smi`.

## Build

```bash
make -C app check-deps
make -C backend
make -C app
```

The app binary is created at:

```bash
app/build/system-monitor-app
```

## Run

```bash
cd app
make run
```

## Tests

```bash
make -f Makefile.test run
```

Valgrind:

```bash
make -f Makefile.test valgrind
```

Clean test artifacts:

```bash
make -f Makefile.test clean
```
