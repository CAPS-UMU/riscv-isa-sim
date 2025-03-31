#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <vector>
#include <random>
#include <cassert>

#include "g4tracer-interface.h"

using namespace std;

void* follow_pointers(void *first, size_t size, size_t iterations) {
  void *p = first;
  for (uint64_t j = 0; j < iterations; ++j) {
    //uint64_t count = 0;
    do {
      p = (void*) *(uint64_t*) p;
      //++count;
    } while (p != first);
    //assert(size == count);
  }
  return p;
}

void init_buffer(vector<void*>& buffer) {
  vector<uint64_t> positions(buffer.size());
  for (uint64_t i = 0; i < positions.size(); ++i) {
    positions[i] = i;
  }
  shuffle(positions.begin(), positions.end(), default_random_engine());
  uint64_t first_position = 1 + find(positions.begin(), positions.end(), 0) - positions.begin();
  uint64_t prev = 0;
  for (uint64_t i = 0; i < positions.size(); ++i) {
    uint64_t adjusted_i = (i + first_position) % positions.size();
    uint64_t pos = positions[adjusted_i];
    buffer[prev] = &buffer[pos];
    prev = pos;
  }
  void *p = buffer[0];
  for (uint64_t i = 0; i < positions.size(); ++i) {
    p = (void*) *(uint64_t*) p;
  }
  assert(p == buffer[0]);
  /*
  for (uint64_t i = 0; i < positions.size(); ++i) {
    uint64_t adjusted_i = (i + first_position) % positions.size();
    printf("%3ld %p %4ld %4ld\n", i, buffer[i], (size_t*)buffer[i] - (size_t*)(&buffer[0]), positions[adjusted_i]);
  }
  */
}

const uint64_t total_accesses = 1024 * 1024 * 1024;
const uint64_t warmup_iterations = 1;
void* global_var;

void measure_latency(uint64_t size_bytes) {
  vector<void*> buffer(size_bytes / sizeof(void*));
  init_buffer(buffer);
  g4tracer_start_tracing();
  auto res1 = follow_pointers(buffer[0], buffer.size(), warmup_iterations);
  global_var = res1;
  auto iterations = max(total_accesses / buffer.size(), 2UL);
  auto ta = g4tracer_rdcycle();
  g4tracer_start_ROI();
  auto res2 = follow_pointers(buffer[0], buffer.size(), iterations);
  g4tracer_end_ROI();
  auto tb = g4tracer_rdcycle();
  global_var = res2;
  assert(res1 == res2);

  auto cycles_it = double(tb - ta) / iterations;
  auto cycles_access = cycles_it / buffer.size();
  printf("%10lu\t%g\n", size_bytes / 1024, cycles_access);
}


int main (int argc, char** argv) {
  bool all = false;
  size_t size_bytes = 0;
  
  if (argc == 1) {
    all = true;
  } else if (argc == 2) {
    size_bytes = stoi(argv[1]);
  } else if (argc == 3) {
    size_bytes = stoi(argv[1]);
    if (string("B") == argv[2]) {
      size_bytes = size_bytes * 1;
    } else if (string("K") == argv[2]) {
      size_bytes = size_bytes * 1024;
    } else if (string("M") == argv[2]) {
      size_bytes = size_bytes * 1024 * 1024;
    } else {
      printf("ERROR: Wrong size unit (%s).\n", argv[2]);
      return 1;
    }
  } else {
    printf("ERROR: Wrong number of arguments.\n");
    return 1;
  }

  if (all) {
    for (auto s: vector<uint64_t> {
        1, 2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512,
        640, 768, 1024, 1536, 2048, 3072, 4096, 5120, 6144, 8192, 10240,
        12288, 16384, 24567, 32768, 49152, 65536, 98304, 131072, 262144,
        393216, 524288, 1048576, 1572864, 1048576, 2097152
      }) {
      measure_latency(s * 1024);
    }
  } else {
    measure_latency(size_bytes);
  }
  return 0;
}
