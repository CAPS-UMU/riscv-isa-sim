#include "g4tracer-interface.h"

const int n = 100000;
struct P {
  float x, y, z;
};
struct P ps[n];
float r;

int main() {
  g4tracer_init_thread();
  g4tracer_start_tracing();
  for(int i = 0; i < n; i++) {
    ps[i].x = i;
    ps[i].y = 5;
    ps[i].z = i;
  }

  r = 0;
  g4tracer_start_ROI_verbose();
  for(int i = 0 ; i < n ; i++)  { // ROI
    r = r + 
      ps[i].x +
      (ps[i].y + 3.1f) +
      ps[i].z;        
  }
  g4tracer_end_ROI_verbose();

  return 0;
}
