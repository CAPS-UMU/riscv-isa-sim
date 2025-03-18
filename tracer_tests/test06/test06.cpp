#include "tracer-interface.h"

const int n = 100000;
struct P {
  float x, y, z;
};
struct P ps[n];
float r;

int main() {
  tracer_start_tracing();
  for(int i = 0; i < n; i++) {
    ps[i].x = i;
    ps[i].y = 5;
    ps[i].z = i;
  }

  r = 0;
  tracer_start_ROI();
  for(int i = 0 ; i < n ; i++)  { // ROI
    r = r + 
      ps[i].x +
      (ps[i].y + 3.1f) +
      ps[i].z;        
  }
  tracer_end_ROI();

  return 0;
}
