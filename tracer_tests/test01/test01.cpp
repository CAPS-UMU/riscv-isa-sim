#include "tracer-interface.h"

const int n = 100000;
double a[n], b[n], c[n];
double k = 7.23;

int main() {
  tracer_start_tracing();
  for(int i = 0; i < n; i++) {
    a[i] = 0.125;
    b[i] = 0.25;
    c[i] = 0.5;
  }

  tracer_start_ROI();
  for(int i = 0; i < n; i++) {
    c[i] = a[i] + b[i] * k;
  }
  tracer_end_ROI();

  return 0;
}
