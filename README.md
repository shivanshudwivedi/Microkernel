# Microkernel OS

A minimal microkernel supporting preemptive multitasking, inter-process communication, and demand-paged virtual memory on x86_64 architecture.

**Repository**: github.com/you/microkernel 
**Tech Stack**: C11, x86_64 Assembly, Makefile, QEMU, GDB

## Table of Contents
- [Product Overview](#product-overview)
- [Goals & Success Metrics](#goals--success-metrics)
- [Requirements](#requirements)
- [High-Level Architecture](#high-level-architecture)
- [Component Specs](#component-specs)
- [Directory Structure](#directory-structure)
- [Build & Installation](#build--installation)
- [Usage Examples](#usage-examples)
- [Roadmap & Prototype Limitations](#roadmap--prototype-limitations)
- [License](#license)

## Product Overview

A minimal microkernel supporting:

- **Preemptive multitasking** with round-robin scheduling
- **Inter-Process Communication (IPC)** via message passing
- **Demand-paged virtual memory** with LRU replacement
- **Hardware-enforced isolation** between tasks
- **8 concurrent user tasks** on QEMU/x86_64

This prototype demonstrates core OS primitives; real-world version is locked down.

## Goals & Success Metrics

| Goal | Metric |
|------|--------|
| Preemption | ≤ 10 μs context-switch overhead |
| Concurrency | ≥ 8 user tasks in round-robin |
| IPC Throughput | ≥ 10 k messages/s |
| Page-Fault Reduction | ≥ 37% fewer faults vs no paging |
| Isolation | 0 cross-task memory violations |

## Requirements

### Functional
- Boot via a 16-bit real-mode bootloader → switch CPU to 64-bit long mode
- Kernel init: set up GDT, IDT, page tables
- Preemptive Scheduler: timer IRQ → context switch among ready tasks
- IPC: send/receive fixed-size messages (≤ 256 B) via syscall
- Virtual Memory: demand-page on page fault → LRU page replacement
- User Tasks: simple ELF loader for statically linked apps

### Non-Functional
- Modularity: each subsystem in its own directory
- Build: Makefile → kernel.bin + initrd.img
- Emulation: run under QEMU with gdb stub
- Debug: symbols enabled, source-level debugging

## High-Level Architecture

```
┌─────────┐   ┌────────┐   ┌───────────────┐   ┌───────────┐
│ Bootldr │──▶│ Kernel │──▶│ Scheduler &   │──▶│ User Tasks│
│ (boot.S)│   │ (main) │   │ IPC (C/asm)   │   │ (ELF apps)│
└─────────┘   └────────┘   └───────────────┘   └───────────┘
      │              │          │  ▲                
      ▼              ▼          ▼  │                
  Hardware      Page Manager  IRQ Timer             
               & LRU Cache                                
```

## Component Specs

### 1. Bootloader (boot.S)
Switch from 16→64-bit, load kernel at 0x100000

### 2. Kernel Core (main.c)
Initialize GDT, IDT, PIC/APIC, PIT/TSC timer, page tables bootstrap

### 3. Preemptive Scheduler (sched.c / context.S)
Round-Robin queue, optimized context switch in assembly

### 4. IPC Subsystem (ipc.c)
Syscalls: send(pid, buf, len), recv(buf, len), buffer queues per task

### 5. Demand-Paged Virtual Memory (vm.c)
Page Fault Handler → allocate page, map physical frame, LRU replacement on OOM

### 6. User-Space Task Loader (loader.c)
ELF parser → load segments into VM, set initial RIP, RSP, pass arguments

## Directory Structure

```
/
├── boot/               
│   └── boot.S
├── kernel/
│   ├── main.c
│   ├── sched.c
│   ├── ipc.c
│   ├── vm.c
│   ├── context.S
│   └── include/*.h
├── loader/
│   └── loader.c
├── user/
│   └── hello_world.c    # sample user app
├── Makefile
├── initrd.img           # generated initrd with user apps
└── README.md
```

## Build & Installation

**Prerequisites**: gcc, nasm, qemu-system-x86_64, make

### Build:
```bash
make all
```
Produces `kernel.bin` and `initrd.img`

### Run:
```bash
qemu-system-x86_64 \
  -kernel kernel.bin \
  -initrd initrd.img \
  -nographic -m 512M \
  -s -S            # for GDB stub on port 1234
```

### Debug (in another shell):
```bash
gdb -ex "target remote :1234" \
    -ex "symbol-file kernel.elf"
```

## Usage Examples

- **Round-Robin Test**: boot → observe eight hello_world tasks printing in turn
- **IPC Demo**: two user apps send ping/pong to each other
- **Page Fault Test**: user app touches > RAM size → triggers LRU eviction

## Roadmap & Prototype Limitations

**Prototype omits:**
- SMP support
- Advanced driver model (no PCI, USB)
- Full syscall table
- Secure loader / cryptographic signatures

**Future:**
- Device driver framework
- File system integration (e.g., FAT32)
- Dynamic linking for user apps

## License

MIT © Your Lab / Organization
