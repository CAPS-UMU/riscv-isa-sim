#include "g4tracer-interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

struct shared_data_t {
  int buffer[10];
  int count = 0;
  int in = 0;
  int out = 0;
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
  pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
};

shared_data_t shared_data;

void* producer(void* arg) {
  g4tracer_init_current_thread();
  g4tracer_start_tracing();
  g4tracer_start_ROI();

  int item = 1;
    
  for (int i = 0; i < 20; i++) {
    g4tracer_begin_sm_mutex_lock(&shared_data.mutex);
    pthread_mutex_lock(&shared_data.mutex);
    g4tracer_end_sm();
        
    // Wait while buffer is full
    while (shared_data.count == 10) {
      printf("Producer waiting: buffer full\n");
      g4tracer_begin_sm_condition_wait(&shared_data.not_full, &shared_data.mutex);
      pthread_cond_wait(&shared_data.not_full, &shared_data.mutex);
      g4tracer_end_sm();
    }
        
    // Produce item
    shared_data.buffer[shared_data.in] = item;
    shared_data.in = (shared_data.in + 1) % 10;
    shared_data.count++;
        
    printf("Produced item %d (buffer count: %d)\n", item, shared_data.count);
    item++;
        
    // Signal that buffer is not empty
    g4tracer_begin_sm_condition_signal(&shared_data.not_empty);
    pthread_cond_signal(&shared_data.not_empty);
    g4tracer_end_sm();
        
    g4tracer_begin_sm_mutex_unlock(&shared_data.mutex);
    pthread_mutex_unlock(&shared_data.mutex);
    g4tracer_end_sm();
        
    // Simulate work
    //usleep(100000); // 100ms
  }
    
  g4tracer_end_ROI();
  return NULL;
}

void* consumer(void* arg) {
  g4tracer_init_current_thread();
  g4tracer_start_tracing();
  g4tracer_start_ROI();

  int consumer_id = *(int*)arg;
    
  for (int i = 0; i < 10; i++) {
    g4tracer_begin_sm_mutex_lock(&shared_data.mutex);
    pthread_mutex_lock(&shared_data.mutex);
    g4tracer_end_sm();
        
    // Wait while buffer is empty
    while (shared_data.count == 0) {
      printf("Consumer %d waiting: buffer empty\n", consumer_id);
      g4tracer_begin_sm_condition_wait(&shared_data.not_empty, &shared_data.mutex);
      pthread_cond_wait(&shared_data.not_empty, &shared_data.mutex);
      g4tracer_end_sm();
    }
        
    // Consume item
    int item = shared_data.buffer[shared_data.out];
    shared_data.out = (shared_data.out + 1) % 10;
    shared_data.count--;
        
    printf("Consumer %d consumed item %d (buffer count: %d)\n", 
           consumer_id, item, shared_data.count);
        
    // Signal that buffer is not full
    g4tracer_begin_sm_condition_signal(&shared_data.not_full);
    pthread_cond_signal(&shared_data.not_full);
    g4tracer_end_sm();
        
    g4tracer_begin_sm_mutex_unlock(&shared_data.mutex);
    pthread_mutex_unlock(&shared_data.mutex);
    g4tracer_end_sm();
        
    // Simulate work
    //usleep(200000); // 200ms
  }
    
  return NULL;
}

int main() {
  pthread_t producer_thread;
  pthread_t consumer_threads[2];
  int consumer_ids[2] = {1, 2};
    
  // Create threads
  pthread_create(&producer_thread, NULL, producer, NULL);
    
  for (int i = 0; i < 2; i++) {
    pthread_create(&consumer_threads[i], NULL, consumer, &consumer_ids[i]);
  }
    
  // Wait for threads to complete
  pthread_join(producer_thread, NULL);
    
  for (int i = 0; i < 2; i++) {
    pthread_join(consumer_threads[i], NULL);
  }
    
  // Cleanup
  pthread_mutex_destroy(&shared_data.mutex);
  pthread_cond_destroy(&shared_data.not_empty);
  pthread_cond_destroy(&shared_data.not_full);
  return 0;
}
