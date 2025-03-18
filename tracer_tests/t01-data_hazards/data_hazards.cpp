#include <cassert>
#include <cstdio>
#include <memory>
#include "util.h"

using namespace std;

struct {
  size_t repeat_times = 3; // total, including warmup
  size_t warmup_times = 1;
  size_t num_vars = 0;
  bool print_each_time = false;
  size_t total_iterations = 64000000L;
} options;

template<int num_vars>
[[gnu::noinline]] // Non standard, but supported by GCC, ICC and CLang
void calculate(float& result, size_t total_iterations) {
  tracer_start_tracing();
  float a[num_vars];
  float one = 1.0f;
  for (size_t i = 0; i < num_vars; ++i) {
    a[i] = 0.0f;
  }
  tracer_start_ROI();
  for (size_t j = 0; j < total_iterations; ++j) {
    for (int k = 0; k < num_vars; ++k) {
      a[k] = a[k] + one; // TODO: use an intrinsic
    } 
  }
  tracer_end_ROI();
  for (size_t i = 1; i < num_vars; ++i) {
    a[0] = a[0] + a[i];
  }
  result = a[0];
}

template<int num_vars>
void measure() {
  long n_flop = options.total_iterations * num_vars;
  vector<double> times;

  float first_result = -1;
  for (size_t i = 0; i < options.repeat_times; ++i) {
    float result;
    auto elapsed_cycles = measure_cycles(calculate<num_vars>, result, options.total_iterations);
    if (i == 0) {
      first_result = result;
    } else {
      assert(result == first_result);
    }
    if (i >= options.warmup_times) {
      times.push_back(elapsed_cycles);
    }
    if (options.print_each_time) {
      if (i == 0) { printf("\n"); }
      printf("    Run %2ld/%2ld: %9ld cycles  %s  result = %f\n", i + 1, options.repeat_times, elapsed_cycles, i < options.warmup_times ? "(warmup)" : "        ", result);
    }
  }

  double average_time = vector_average(times);
  double stddev_time = vector_stddev(times);
  printf("%10ld MFLOP, %2d vars: ", n_flop/1000000, num_vars);
  printf("avg cycles: %11.0f±%9.0f  cycles/op:   %7.2f±%.2f    result = %f\n", average_time, stddev_time, average_time / n_flop, stddev_time / n_flop, first_result);
}

int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    if (!parse_bool_arg(argv[i], "print-each-time", options.print_each_time)
        && !parse_size_arg(argv[i], "repeat-times", options.repeat_times)
        && !parse_size_arg(argv[i], "warmup-times", options.warmup_times)
        && !parse_size_arg(argv[i], "total-iterations", options.total_iterations)
        && !parse_size_arg(argv[i], "num-vars", options.num_vars)) {
      fprintf(stderr, "Incorrect argument: %s\n", argv[i]);
      return 1;
    }
  }

#define measure_if_num_vars(i) if (options.num_vars == i || options.num_vars == 0) measure<i>();
  measure_if_num_vars(1);
  measure_if_num_vars(2);
  measure_if_num_vars(3);
  measure_if_num_vars(4);
  measure_if_num_vars(5);
  measure_if_num_vars(6);
  measure_if_num_vars(7);
  measure_if_num_vars(8);
  measure_if_num_vars(9);
  measure_if_num_vars(10);
  measure_if_num_vars(11);
  measure_if_num_vars(12);
  measure_if_num_vars(13);
  measure_if_num_vars(14);
  measure_if_num_vars(15);
  measure_if_num_vars(16);
  measure_if_num_vars(17);
  measure_if_num_vars(18);
  measure_if_num_vars(19);
  measure_if_num_vars(20);
  measure_if_num_vars(21);
  measure_if_num_vars(22);
  measure_if_num_vars(23);
  measure_if_num_vars(24);
  measure_if_num_vars(25);
  measure_if_num_vars(26);
  measure_if_num_vars(27);
  measure_if_num_vars(28);
  measure_if_num_vars(29);
  measure_if_num_vars(30);
  measure_if_num_vars(31);
  measure_if_num_vars(32);
#undef measure_if_num_vars
  return 0;
}
