#include "g4tracer-interface.h"

const int n = 100000;
double a[n], b[n], c[n];
double k = 7.23;

int main() {
  g4tracer_init_current_thread();
  g4tracer_start_tracing();
  for(int i = 0; i < n; i++) {
    a[i] = 0.125;
    b[i] = 0.25;
    c[i] = 0.5;
  }

  g4tracer_start_ROI_verbose();
  for (int j = 0; j < 10; ++j)
  for(int i = 0; i < n; i++) {
    c[i] = c[i] * j + a[i] + b[i] * k;
  }
  g4tracer_end_ROI_verbose();

  return 0;
}
