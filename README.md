# GPIO Interrupt Event Logger: A Custom Linux Kernel Driver for Raspberry Pi 5

An interrupt-driven Linux character device driver that logs timestamped
events from a GPIO input, built for Raspberry Pi 5 (kernel 6.18,
64-bit).

## Overview

`eventlogger` registers a character device (`/dev/eventlogger`) and a
GPIO interrupt on a configurable pin. Each falling-edge interrupt is
captured, queued in a kernel-space ring buffer, and made available to
userspace through a blocking `read()` interface. Buffer statistics are
exposed via `ioctl()`, and a background kernel thread reports periodic
status to the kernel log.

## Features

- Real GPIO interrupt handling (`IRQF_TRIGGER_FALLING`), no polling
- Split top-half / bottom-half design (ISR + workqueue)
- Fixed-size circular buffer with spinlock-protected concurrent access
- Blocking `read()` via kernel wait queues
- `ioctl()` interface for querying total and pending event counts
- Background kernel thread for periodic status reporting
- Clean module load/unload with full resource teardown

## Requirements

- Linux kernel headers matching the target system
  (built/tested against `6.18.50+rpt-rpi-2712`)
- `build-essential` / kernel module build toolchain
- A GPIO input source (tested with a momentary push-button)

## Hardware setup

| Signal | Connection                |
|--------|----------------------------|
| S      | GPIO17 (physical pin 11)   |
| VCC    | 3.3V (physical pin 1)      |
| GND    | GND (physical pin 9)       |

## Building

```bash
make
```

## Usage

Load the module:

```bash
sudo insmod eventlogger.ko
sudo chmod 666 /dev/eventlogger
```

Read events (blocks until an event occurs):

```bash
cat /dev/eventlogger
# Event:1,timestamp:2145618501362
```

Query statistics via the included test client:

```bash
gcc test.c -o test
./test
```

Unload the module:

```bash
sudo rmmod eventlogger
```

## Architecture

```
GPIO interrupt (top half)
        │
        ▼
   schedule_work()
        │
        ▼
workqueue handler (bottom half)
        │
        ▼
spinlock-protected ring buffer
        │
   ┌────┴────┐
   ▼         ▼
 read()    ioctl()
(blocking) (stats)
```

## Project structure

```
.
├── eventlogger.c     # driver source
├── Makefile
├── test.c            # userspace test client
└── README.md
```

## Author

Jayaraj
