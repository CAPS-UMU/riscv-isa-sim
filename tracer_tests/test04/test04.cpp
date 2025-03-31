#include "g4tracer-interface.h"

const int n = 100000;
int offsets[n];
double a[n];
double b[n];

int main() {
  g4tracer_start_tracing();
  for(int i = 0; i < n; i++) {
    a[i] = i;
    offsets[i] = n - i;
  }

  g4tracer_start_ROI_verbose();
  for(int i = 0 ; i < n ; i++)  { // ROI
    b[i] = a[offsets[i]];
    //b[offsets[i]] = a[i];
  }
  g4tracer_end_ROI_verbose();

  return 0;
}
