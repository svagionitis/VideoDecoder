# VideoDecoder Feature & Concurrency Roadmap

This document outlines the high-performance primitives implemented in the `VideoDecoder` C++ library, as well as the planned roadmap for future concurrency, low-latency, and zero-copy performance enhancements.

---

## Completed Features & Primitives

### 1. Lock-Free Single-Producer Single-Consumer (SPSC) Ring Buffer
- **File**: [`../lib/SPSCQueue.h`](../lib/SPSCQueue.h)
- **Description**: Zero-allocation lock-free ring buffer template (`videodecoder::SPSCQueue<T, Capacity>`) utilizing explicit C++17 `acquire`-`release` atomic memory order semantics.
- **Benefit**: Replaces blocking OS mutexes on frame handoff paths, achieving sub-microsecond push/pop latency.

### 2. Cache Line Padding (`alignas(64)`) to Prevent False Sharing
- **File**: [`../lib/SPSCQueue.h`](../lib/SPSCQueue.h)
- **Description**: `head` and `tail` atomic indices are aligned to distinct 64-byte boundaries using `alignas(hardware_destructive_interference_size)`.
- **Benefit**: Prevents CPU L1/L2 cache line invalidations (false sharing) between producer and consumer cores.

### 3. Asynchronous Non-Blocking Decoder Worker
- **File**: [`../lib/AsyncDecoderRunner.h`](../lib/AsyncDecoderRunner.h)
- **Description**: Dedicated background decoding worker (`videodecoder::AsyncDecoderRunner`) that continuously decodes frames and pushes them to an `SPSCQueue`.
- **Benefit**: Decouples frame decoding throughput from UI/render loops (SDL2, Qt `QOpenGLWidget`).

---

## Future Low-Latency & Performance Roadmap

The following high-performance concurrency primitives are planned for future integration into the `VideoDecoder` engine:

### 1. Atomic Triple Buffering (Zero-Copy Frame Exchange)
- **Concept**: Maintain 3 pre-allocated frame buffers:
  1. **Write Buffer**: Owned exclusively by the decoder thread.
  2. **Read Buffer**: Owned exclusively by the display/rendering thread.
  3. **Next Buffer**: Atomically swapped via `std::atomic<FrameBuffer*>`.
- **Goal**: Allow rendering at display refresh rate (60/120/144 Hz) without frame tearing or memory copies.

### 2. Adaptive Spinlocks with Active CPU Pauses (`_mm_pause()`)
- **Concept**: Implement a hybrid spinlock that uses CPU pause intrinsics (`_mm_pause()` on x86, `__builtin_arm_yield()` on ARM) before yielding to the OS scheduler.
- **Goal**: Replace `std::mutex` in critical sections lasting $< 2$ microseconds, eliminating kernel context-switch overhead (~2–10 µs).

### 3. SIMD-Aligned Zero-Copy Memory Pool Allocator
- **Concept**: A custom memory pool allocating 64-byte aligned memory blocks (`std::aligned_alloc` / `_aligned_malloc`) for raw RGB24/NV12 pixel buffers.
- **Goal**: Guarantee zero dynamic `malloc`/`free` calls during continuous video playback and optimize memory for AVX2/AVX-512/NEON SIMD operations.

### 4. Multi-Producer Single-Consumer (MPSC) Lock-Free Queue
- **Concept**: Lock-free queue using atomic compare-and-swap (`std::atomic::compare_exchange_weak`).
- **Goal**: Support multi-camera grid playback where multiple video decoding threads feed into a single UI compositor.

### 5. NUMA-Aware Memory & Thread Placement
- **Concept**: Bind decoder worker threads and their associated memory allocations to specific NUMA nodes (`numa_alloc_onnode` / Win32 `SetThreadGroupAffinity`).
- **Goal**: Minimize cross-socket memory access latency on multi-socket server hardware.
