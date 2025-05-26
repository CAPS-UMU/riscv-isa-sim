#include "g4tracer-interface.h"
#include <pthread.h>
#include <stdio.h>

const int padding_size = 0;
struct Item {
  int i = 0;
  pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
  char padding[padding_size];
};

const int MAX_ITEMS = 1024;
struct Item items[MAX_ITEMS];
int n_items = 0;

int n_ops_thread = 100000;

pthread_barrier_t barrier;

void* adder_thread(void *) {
  g4tracer_init_current_thread();
  g4tracer_start_tracing();

  g4tracer_begin_sm_barrier(&barrier);
  pthread_barrier_wait(&barrier);
  g4tracer_end_sm();
  
  int nl = 0;
  g4tracer_start_ROI();
  for (int op = 0; op < n_ops_thread; ++op) {
    g4tracer_begin_sm_mutex_lock(&items[nl].m);
    pthread_mutex_lock(&items[nl].m);
    g4tracer_end_sm();
    ++items[nl].i;
    g4tracer_begin_sm_mutex_unlock(&items[nl].m);
    pthread_mutex_unlock(&items[nl].m);
    g4tracer_end_sm();
    
    ++nl;
    if (nl == n_items) {
      nl = 0;
    }
  }
  g4tracer_end_ROI();
  return NULL;
}


int main(int argc, char* argv[]) {
  int n_locks = 1;
  const int MAX_THREADS = 1024;
  int n_threads = 4;
  
  if (argc > 1) {
    n_threads = atoi(argv[1]);
  }
  if (argc > 2) {
    n_locks = atoi(argv[2]);
  }
  if (argc > 3) {
    n_ops_thread = atoi(argv[3]);
  }

  pthread_barrier_init(&barrier, NULL, n_threads);

  n_items = n_locks;
  pthread_t threads[MAX_THREADS];
  for (int i = 0; i < n_threads; ++i) {
    pthread_create(&threads[i], NULL, adder_thread, NULL);
  }
  for (int i = 0; i < n_threads; ++i) {
    pthread_join(threads[i], NULL);
  }
  for (int i = 0; i < n_items; ++i) {
    printf("%d\n", items[i].i);
  }
  return 0;
}
