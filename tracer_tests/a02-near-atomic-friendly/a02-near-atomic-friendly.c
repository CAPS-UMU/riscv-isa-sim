#define _GNU_SOURCE  /* for pthread_setaffinity_np */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

#define g4tracer_VERBOSE false
#include "g4tracer-interface.h"

#define NUM_THREADS 4
#define ARRAY_ELEMENTS 16
#define OPERATIONS_PER_THREAD 32

pthread_barrier_t barrier;
int array_elements_per_thread;
atomic_int *atomic_array;

void* mythread_atomic_inc (void* thr_data) {
    g4tracer_init_thread();

    int thread_id = *((int *) thr_data);
    int base = thread_id * array_elements_per_thread;
    int n = 0;
    int atomic_int; // warning: variable 'atomic_int' set but not used

    g4tracer_start_tracing();

    pthread_barrier_wait(&barrier);

    g4tracer_start_ROI();
    for(int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
        ++atomic_array[base + n];  // Fixed variable name
        atomic_int = atomic_array[base + n];
        n++;
        if (n == array_elements_per_thread) {
            n = 0;
        }
    }
    g4tracer_end_ROI();

    return NULL;
}

int main() {
    pthread_t thread_id[NUM_THREADS];
    int thread_args[NUM_THREADS];  // Array for thread IDs
    
    pthread_barrier_init(&barrier, NULL, NUM_THREADS);
    
    atomic_array = malloc(ARRAY_ELEMENTS * sizeof(atomic_int));
    if (atomic_array == NULL) {
        printf("Error allocating memory.\n");
        return 1;
    }

    for (int i = 0; i < ARRAY_ELEMENTS; i++) {
        atomic_init(&atomic_array[i], 0);  // Correct way to initialize atomic_int
    }
    
    array_elements_per_thread = ARRAY_ELEMENTS / NUM_THREADS;

    for(int i = 0; i < NUM_THREADS; i++) {
        thread_args[i] = i;
        pthread_create(&thread_id[i], NULL, mythread_atomic_inc, &thread_args[i]);  // Pass address
    }

    for(int n = 0; n < NUM_THREADS; ++n) {
        pthread_join(thread_id[n], NULL);
    }

    free(atomic_array);  // Free allocated memory
    return 0;
}
