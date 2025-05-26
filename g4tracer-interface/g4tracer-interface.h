#pragma once
#ifndef __G4TRACER_INTERFACE_H__
#define __G4TRACER_INTERFACE_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

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

enum G4TraceAnnotationId {
    G4_TRACE_ANNOTATION_ID_START_TRACING = 0x101,
    G4_TRACE_ANNOTATION_ID_START_REGION_OF_INTEREST = 0x102,
    G4_TRACE_ANNOTATION_ID_END_REGION_OF_INTEREST = 0x103,

    // Synchronization markers
    G4_TRACE_ANNOTATION_ID_BEGIN_SM_MUTEX_ACQUIRE = 0x110,
    G4_TRACE_ANNOTATION_ID_BEGIN_SM_MUTEX_RELEASE = 0x111,
    G4_TRACE_ANNOTATION_ID_BEGIN_SM_BARRIER = 0x112,
    G4_TRACE_ANNOTATION_ID_BEGIN_SM_CONDITION_SIGNAL = 0x113,
    G4_TRACE_ANNOTATION_ID_BEGIN_SM_CONDITION_BROADCAST = 0x114,
    G4_TRACE_ANNOTATION_ID_BEGIN_SM_CONDITION_WAIT = 0x115,

    G4_TRACE_ANNOTATION_ID_END_SM = 0x1FF,
};

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
  // bind threads from last available processor to first, so that processor 0 will be free as long as thare are more processors that threads.
  int proc = np - 1 - (id % np);
  bind_thread_to_processor(thread, proc);
}

G4TRACER_INTERFACE_FUNC void g4tracer_init_current_thread() {
  pthread_t current_thread = pthread_self();
  g4tracer_init_thread(current_thread);
}


#if defined __riscv
#define G4_TRACE_HINT_ASM(hint_id)                      \
  __asm__ volatile("sltiu zero, zero, %[id]"            \
                   : : [id]"i"(hint_id) : "memory");

#define G4_TRACE_HINT_ASM_1(hint_id, arg_a0)                    \
  register void *__a0 asm ("a0") = arg_a0;                      \
  __asm__ volatile("sltiu zero, zero, %[id]"                    \
                   : : [id]"i"(hint_id), "r"(__a0) : "memory");

#define G4_TRACE_HINT_ASM_2(hint_id, arg_a0, arg_a1)                    \
  register void *__a0 asm ("a0") = arg_a0;                              \
  register void *__a1 asm ("a1") = arg_a1;                              \
  __asm__ volatile("sltiu zero, zero, %[id]"                            \
                   : : [id]"i"(hint_id), "r"(__a0), "r"(__a1)  : "memory");

G4TRACER_INTERFACE_FUNC void g4tracer_start_tracing() { G4_TRACE_HINT_ASM(G4_TRACE_ANNOTATION_ID_START_TRACING); }
G4TRACER_INTERFACE_FUNC void g4tracer_start_ROI() { G4_TRACE_HINT_ASM(G4_TRACE_ANNOTATION_ID_START_REGION_OF_INTEREST); }
G4TRACER_INTERFACE_FUNC void g4tracer_end_ROI() { G4_TRACE_HINT_ASM(G4_TRACE_ANNOTATION_ID_END_REGION_OF_INTEREST); }
G4TRACER_INTERFACE_FUNC void g4tracer_begin_sm_mutex_lock(void *mutex) { G4_TRACE_HINT_ASM_1(G4_TRACE_ANNOTATION_ID_BEGIN_SM_MUTEX_ACQUIRE, mutex); }
G4TRACER_INTERFACE_FUNC void g4tracer_begin_sm_mutex_unlock(void *mutex) { G4_TRACE_HINT_ASM_1(G4_TRACE_ANNOTATION_ID_BEGIN_SM_MUTEX_RELEASE, mutex); }
G4TRACER_INTERFACE_FUNC void g4tracer_begin_sm_barrier(void *barrier) { G4_TRACE_HINT_ASM_1(G4_TRACE_ANNOTATION_ID_BEGIN_SM_BARRIER, barrier); }
G4TRACER_INTERFACE_FUNC void g4tracer_begin_sm_condition_signal(void *cond) { G4_TRACE_HINT_ASM_1(G4_TRACE_ANNOTATION_ID_BEGIN_SM_CONDITION_SIGNAL, cond); }
G4TRACER_INTERFACE_FUNC void g4tracer_begin_sm_condition_broadcast(void *cond) { G4_TRACE_HINT_ASM_1(G4_TRACE_ANNOTATION_ID_BEGIN_SM_CONDITION_BROADCAST, cond); }
G4TRACER_INTERFACE_FUNC void g4tracer_begin_sm_condition_wait(void *cond, void *mutex) { G4_TRACE_HINT_ASM_2(G4_TRACE_ANNOTATION_ID_BEGIN_SM_CONDITION_WAIT, cond, mutex); }
G4TRACER_INTERFACE_FUNC void g4tracer_end_sm() { G4_TRACE_HINT_ASM(G4_TRACE_ANNOTATION_ID_END_SM); }


G4TRACER_INTERFACE_FUNC uint64_t g4tracer_rdtime() {
  unsigned long cycles;
  __asm__ volatile("rdtime %0" : "=r"(cycles) :: "memory");
  return cycles;
}

G4TRACER_INTERFACE_FUNC uint64_t g4tracer_rdcycle() {
  unsigned long cycles;
  __asm__ volatile("rdcycle %0" : "=r"(cycles) :: "memory");
  return cycles;
}

G4TRACER_INTERFACE_FUNC uint64_t g4tracer_rdinstret() {
  unsigned long instr;
  __asm__ volatile("rdinstret %0" : "=r"(instr) :: "memory");
  return instr;
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

G4TRACER_INTERFACE_FUNC void g4tracer_begin_sm_mutex_lock(void *mutex) {
  // TODO
}

G4TRACER_INTERFACE_FUNC void g4tracer_begin_sm_mutex_unlock(void *mutex) {
  // TODO
}

G4TRACER_INTERFACE_FUNC void g4tracer_begin_sm_barrier(void *barrier) {
  // TODO
}

G4TRACER_INTERFACE_FUNC void g4tracer_begin_sm_condition_signal(void *cond) {
  // TODO
}

G4TRACER_INTERFACE_FUNC void g4tracer_begin_sm_condition_broadcast(void *cond) {
  // TODO
}

G4TRACER_INTERFACE_FUNC void g4tracer_begin_sm_condition_wait(void *cond, void *mutex) {
  // TODO
}

G4TRACER_INTERFACE_FUNC void g4tracer_end_sm() {
  // TODO
}
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

// TODO: move to another file

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
