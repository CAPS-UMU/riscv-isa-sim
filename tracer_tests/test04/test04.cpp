#include "tracer-interface.h"

const int n = 100000;
int offsets[n];
double a[n];
double b[n];

int main() {
  tracer_start_tracing();
  for(int i = 0; i < n; i++) {
    a[i] = i;
    offsets[i] = n - i;
  }

  tracer_start_ROI();
  for(int i = 0 ; i < n ; i++)  { // ROI
    b[i] = a[offsets[i]];
    //b[offsets[i]] = a[i];
  }
  tracer_end_ROI();

  return 0;
}
