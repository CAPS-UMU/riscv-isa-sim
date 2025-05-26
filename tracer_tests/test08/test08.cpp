#include "g4tracer-interface.h"

#include <iostream>
#include <thread>
#include <barrier>
#include <vector>

using namespace std;

const int padding_size = 0;
struct Item {
  int i = 0;
  mutex m;
  char padding[padding_size];
};

void adder_thread(barrier<>& barrier, int n_ops_thread, vector<Item>* items) {
  g4tracer_init_current_thread();
  g4tracer_start_tracing();

  g4tracer_begin_sm_barrier(&barrier);
  barrier.arrive_and_wait();
  g4tracer_end_sm();
  
  int n_items = items->size();
  int nl = 0;
  g4tracer_start_ROI();
  for (int op = 0; op < n_ops_thread; ++op) {
    auto& i = (*items)[nl];
    g4tracer_begin_sm_mutex_lock(i.m.native_handle());
    i.m.lock();
    g4tracer_end_sm();
    ++i.i;
    g4tracer_begin_sm_mutex_unlock(i.m.native_handle());
    i.m.unlock();
    g4tracer_end_sm();
    
    ++nl;
    if (nl == n_items) {
      nl = 0;
    }
  }
  g4tracer_end_ROI();
}


int main(int argc, char* argv[]) {
  int n_ops_thread = 100000;
  int n_locks = 1;
  int n_threads = 4;
  
  if (argc > 1) {
    n_threads = stoi(argv[1]);
  }
  if (argc > 2) {
    n_locks = stoi(argv[2]);
  }
  if (argc > 3) {
    n_ops_thread = stoi(argv[3]);
  }
  
  vector<Item> items(n_locks);
  vector<thread> threads;
  barrier barrier(n_threads);
  for (int i = 0; i < n_threads; ++i) {
    threads.emplace_back(adder_thread, ref(barrier), n_ops_thread, &items);
  }
  for (auto& t: threads) {
    t.join();
  }
  for (auto& i: items) {
    cout << i.i << endl;
  }
  return 0;
}
