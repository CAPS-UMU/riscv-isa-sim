#pragma once
#ifndef __TRACER_INTERFACE_H__
#define __TRACER_INTERFACE_H__

#ifdef __cpp
#include <cstdint>
#else
#include <stdint.h>
#endif

#ifndef TRACER_VERBOSE
#define TRACER_VERBOSE 1
#endif
#ifndef TRACER_VERBOSE_RDCYCLE
#define TRACER_VERBOSE_RDCYCLE 0
#endif

#if TRACER_VERBOSE
// These will only work correctly if tracer_start_ROI and tracer_end_ROI are called from the same file
static uint64_t _tracer_roi_start_rdtime;
#if TRACER_VERBOSE_RDCYCLE
static uint64_t _tracer_roi_start_cycle;
static uint64_t _tracer_roi_start_instret;
#endif

#ifdef __cpp
#include <cstdio>
#else
#include <stdio.h>
#endif

#endif

#if defined __riscv
static inline __attribute__((always_inline)) uint64_t tracer_rdtime() {
  unsigned long cycles;
  asm volatile("rdtime %0" : "=r"(cycles) :: "memory");
  return cycles;
}

static inline __attribute__((always_inline)) uint64_t tracer_rdcycle() {
  unsigned long cycles;
  asm volatile("rdcycle %0" : "=r"(cycles) :: "memory");
  return cycles;
}

static inline __attribute__((always_inline)) uint64_t tracer_rdinstret() {
  unsigned long instr;
  asm volatile("rdinstret %0" : "=r"(instr) :: "memory");
  return instr;
}

static inline __attribute__((always_inline)) void tracer_start_tracing() {
  asm volatile("srai zero, zero, 2" ::: "memory");
}

static inline __attribute__((always_inline)) void tracer_start_ROI() {
#if TRACER_VERBOSE
  _tracer_roi_start_rdtime = tracer_rdtime();
#if TRACER_VERBOSE_RDCYCLE
  _tracer_roi_start_cycle = tracer_rdcycle();
  _tracer_roi_start_instret = tracer_rdinstret();
#endif
#endif
  __asm__ volatile("srai zero, zero, 0" ::: "memory");
}

static inline __attribute__((always_inline)) void tracer_end_ROI() {
  __asm__ volatile("srai zero, zero, 1" ::: "memory");
#if TRACER_VERBOSE
  {
    uint64_t elapsed_rdtime = tracer_rdtime() - _tracer_roi_start_rdtime;
#if TRACER_VERBOSE_RDCYCLE
    uint64_t elapsed_cycles = tracer_rdcycle() - _tracer_roi_start_cycle;
    uint64_t elapsed_instret = tracer_rdinstret() - _tracer_roi_start_instret;
#endif
    printf("rdtime:      %15ld\n", elapsed_rdtime);
#if TRACER_VERBOSE_RDCYCLE
    printf("intructions: %15ld\ncycles:      %15ld\nCPI:         %15f\n", elapsed_instret, elapsed_cycles, ((double) elapsed_cycles) / elapsed_instret);
#endif
  }
#endif
}
#elif defined __x86_64__
#include <x86intrin.h>

static inline __attribute__((always_inline)) uint64_t tracer_rdtime() {
  return _rdtsc();
}

static inline __attribute__((always_inline)) uint64_t tracer_rdcycle() {
  return _rdtsc();
}

static inline __attribute__((always_inline)) uint64_t tracer_rdinstret() {
  // TODO
  return 0;
}

static inline __attribute__((always_inline)) void tracer_start_tracing() {
  // TODO
}

static inline __attribute__((always_inline)) void tracer_start_ROI() {
  // TODO
#if TRACER_VERBOSE
  _tracer_roi_start_cycle = tracer_rdcycle();
  _tracer_roi_start_instret = tracer_rdinstret();
#endif
}

static inline __attribute__((always_inline)) void tracer_end_ROI() {
  // TODO
#if TRACER_VERBOSE
  {
    uint64_t elapsed_cycles = tracer_rdcycle() - _tracer_roi_start_cycle;
    uint64_t elapsed_instret = tracer_rdinstret() - _tracer_roi_start_instret;
    printf("intructions: %15ld\ncycles:      %15ld\nCPI:         %15f\n", elapsed_instret, elapsed_cycles, double(elapsed_cycles) / elapsed_instret);
  }
#endif
}
#endif

#endif
