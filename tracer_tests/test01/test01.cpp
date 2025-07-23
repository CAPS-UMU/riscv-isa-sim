#include "g4tracer-interface.h"

const uint64_t n = 1000000;
double a[n], b[n], c[n];
double k = 7.23;

int main() {
  g4tracer_init_current_thread();
  g4tracer_start_tracing();
  for(uint64_t i = 0; i < n; i++) {
    a[i] = 0.125 + i;
    b[i] = 0.25 + i;
    c[i] = 0.5;
  }

  g4tracer_start_ROI_verbose();
  for(uint64_t i = 0; i < n; i++) {
    c[i] = a[i] + b[i] * k;
  }
  g4tracer_end_ROI_verbose();

  return 0;
}
