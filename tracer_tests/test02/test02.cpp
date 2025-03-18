#include "tracer-interface.h"

const int n = 100000;
double a[n], b[n], c[n];
double k = 7.23;

int main() {
  tracer_start_tracing();
  for(int i = 0 ; i < n ; i++) {
    a[i] = 1.0 / (double) n;
    b[i] = 2.0;
    
    c[i] = 0;
  }
  
  tracer_start_ROI();
  for(int i = 0 ; i < n ; i++) {
    if (i % 4 != 0) {
      c[i] = a[i] + b[i] * k;
    }
  }
  tracer_end_ROI();

  return 0;
}
