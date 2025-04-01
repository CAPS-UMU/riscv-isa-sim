#pragma once
#ifndef __G4TRACER_INTERFACE_H__
#define __G4TRACER_INTERFACE_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <sys/sysinfo.h>

#ifdef __cpp
#include <cstdint>
#include <cstdlib>
#include <cstdbool>
#else
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#define thread_local __thread
#endif

#ifndef G4TRACER_VERBOSE
// These will only work correctly if g4tracer_start_ROI and g4tracer_end_ROI are called from the same file
#define G4TRACER_VERBOSE 1
#endif
#ifndef G4TRACER_VERBOSE_GETTIME
#define G4TRACER_VERBOSE_GETTIME 1
#endif
#ifndef G4TRACER_VERBOSE_RDTIME
#define G4TRACER_VERBOSE_RDTIME 0
#endif
#ifndef G4TRACER_VERBOSE_RDCYCLE
#define G4TRACER_VERBOSE_RDCYCLE 0 // Using RDCYLE and RDINSTRET from userspace is deprecated and requires enabling kernel support
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

#if G4TRACER_VERBOSE_RDTIME
static thread_local uint64_t _g4tracer_roi_start_rdtime;
#endif
#if G4TRACER_VERBOSE_GETTIME
static thread_local uint64_t _g4tracer_roi_start_gettime;
#endif
#if G4TRACER_VERBOSE_RDCYCLE
static thread_local uint64_t _g4tracer_roi_start_cycle;
static thread_local uint64_t _g4tracer_roi_start_instret;
#endif

#define G4TRACER_INTERFACE_FUNC static inline __attribute__((always_inline))

G4TRACER_INTERFACE_FUNC void bind_thread_to_processor(pthread_t thread, int proc) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(proc, &cpuset);
  pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
}

static atomic_int _g4tracer_thread_id = 0;

G4TRACER_INTERFACE_FUNC void g4tracer_init_thread(pthread_t thread) {
  int np = get_nprocs();
  int id = _g4tracer_thread_id++;
  bind_thread_to_processor(thread, ((np - id) % np + np) % np); // bind threads from last available processor to first, so that processor will be free as long as thare are more processors that threads.
}

G4TRACER_INTERFACE_FUNC void g4tracer_init_current_thread() {
  pthread_t current_thread = pthread_self();
  g4tracer_init_thread(current_thread);
}

#ifdef __cpp
#include <cstdio>
#include <ctime>
#include <cassert>
#else
#include <stdio.h>
#include <time.h>
#include <assert.h>
#endif

#if G4TRACER_VERBOSE_GETTIME
G4TRACER_INTERFACE_FUNC uint64_t get_time_ns() {
  struct timespec  ts;
  /*int err = */clock_gettime(CLOCK_MONOTONIC, &ts);
  /*assert(!err);*/
  return ts.tv_sec * 1000000000 + ts.tv_nsec;
}
#endif

#if defined __riscv
G4TRACER_INTERFACE_FUNC uint64_t g4tracer_rdtime() {
  unsigned long cycles;
  asm volatile("rdtime %0" : "=r"(cycles) :: "memory");
  return cycles;
}

G4TRACER_INTERFACE_FUNC uint64_t g4tracer_rdcycle() {
  unsigned long cycles;
  asm volatile("rdcycle %0" : "=r"(cycles) :: "memory");
  return cycles;
}

G4TRACER_INTERFACE_FUNC uint64_t g4tracer_rdinstret() {
  unsigned long instr;
  asm volatile("rdinstret %0" : "=r"(instr) :: "memory");
  return instr;
}

G4TRACER_INTERFACE_FUNC void g4tracer_start_tracing() {
  asm volatile("srai zero, zero, 2" ::: "memory");
}

G4TRACER_INTERFACE_FUNC void g4tracer_start_ROI() {
  __asm__ volatile("srai zero, zero, 0" ::: "memory");
}

G4TRACER_INTERFACE_FUNC void g4tracer_end_ROI() {
  __asm__ volatile("srai zero, zero, 1" ::: "memory");
}
#elif defined __x86_64__
#include <x86intrin.h>

G4TRACER_INTERFACE_FUNC uint64_t g4tracer_rdtime() {
  return _rdtsc();
}

G4TRACER_INTERFACE_FUNC uint64_t g4tracer_rdcycle() {
  return _rdtsc();
}

G4TRACER_INTERFACE_FUNC uint64_t g4tracer_rdinstret() {
  // TODO
  return 0;
}

G4TRACER_INTERFACE_FUNC void g4tracer_start_tracing() {
  // TODO
}

G4TRACER_INTERFACE_FUNC void g4tracer_start_ROI() {
  // TODO
}

G4TRACER_INTERFACE_FUNC void g4tracer_end_ROI() {
  // TODO
}
#endif

G4TRACER_INTERFACE_FUNC void g4tracer_start_ROI_verbose() {
#if G4TRACER_VERBOSE
#if G4TRACER_VERBOSE_RDTIME
  _g4tracer_roi_start_rdtime = g4tracer_rdtime();
#endif
#if G4TRACER_VERBOSE_GETTIME
  _g4tracer_roi_start_gettime = get_time_ns();
#endif
#if G4TRACER_VERBOSE_RDCYCLE
  _g4tracer_roi_start_cycle = g4tracer_rdcycle();
  _g4tracer_roi_start_instret = g4tracer_rdinstret();
#endif
#endif
  g4tracer_start_ROI();
}

G4TRACER_INTERFACE_FUNC void g4tracer_end_ROI_verbose() {
  g4tracer_end_ROI();
#if G4TRACER_VERBOSE
#if G4TRACER_VERBOSE_RDTIME
  uint64_t elapsed_rdtime = g4tracer_rdtime() - _g4tracer_roi_start_rdtime;
#endif
#if G4TRACER_VERBOSE_GETTIME
  uint64_t elapsed_gettime = get_time_ns() - _g4tracer_roi_start_gettime;
#endif
#if G4TRACER_VERBOSE_RDCYCLE
  uint64_t elapsed_cycles = g4tracer_rdcycle() - _g4tracer_roi_start_cycle;
  uint64_t elapsed_instret = g4tracer_rdinstret() - _g4tracer_roi_start_instret;
#endif
#if G4TRACER_VERBOSE_RDTIME
  printf("rdtime:      %15ld\n", elapsed_rdtime);
#endif
#if G4TRACER_VERBOSE_GETTIME
  printf("gettime:     %15ld\n", elapsed_gettime);
#endif
#if G4TRACER_VERBOSE_RDCYCLE
  printf("intructions: %15ld\ncycles:      %15ld\nCPI:         %15f\n", elapsed_instret, elapsed_cycles, ((double) elapsed_cycles) / elapsed_instret);
#endif
#endif
}

struct g4tracer_thread_wrapper_data {
  void *(*start)(void *);
  void *arg;
  bool start_tracing;
  bool start_roi;
};

static void *g4tracer_thread_wrapper_func(void *wrapper_data) {
  struct g4tracer_thread_wrapper_data *data = (struct g4tracer_thread_wrapper_data *) wrapper_data;
  g4tracer_init_current_thread();
  bool tracing = data->start_tracing;
  bool roi = data->start_roi;
  void *(*start)(void *) = data->start;
  void *arg = data->arg;
  free(data);
  if (tracing) g4tracer_start_tracing();
  if (roi) g4tracer_start_ROI();
  return start(arg);
}

G4TRACER_INTERFACE_FUNC int g4tracer_pthread_create(pthread_t *thr,
                                                    void *(*start)(void *),
                                                    void *arg,
                                                    bool start_tracing,
                                                    bool start_roi) {
  struct g4tracer_thread_wrapper_data *data = (struct g4tracer_thread_wrapper_data *) malloc(sizeof(struct g4tracer_thread_wrapper_data));
  data->start = start;
  data->arg = arg;
  data->start_tracing = start_tracing;
  data->start_roi = start_roi;
  return pthread_create(thr, NULL, g4tracer_thread_wrapper_func, data);
}

#pragma GCC diagnostic pop

#endif
