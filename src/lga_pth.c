// For some reason, the type pthread_barrier_t doesn't get defined without this
#define _XOPEN_SOURCE 600

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "lga_base.h"
#include "lga_pth.h"

// Data structure to hold data to pass around thread function calls
typedef struct
{
    // Interval of lines this thread is responsible for
    int i_start;
    int i_end;

    // Data about the grid needed for the updates
    byte *grid_1;
    byte *grid_2;
    int grid_size;

    // Syncronization barrier for the threads
    pthread_barrier_t *barrier;
} thread_arg, *ptr_thread_arg;

static byte
get_next_cell (int i, int j, byte *grid_in, int grid_size)
{
    byte next_cell = EMPTY;

    for (int dir = 0; dir < NUM_DIRECTIONS; dir++)
    {
        int rev_dir = (dir + NUM_DIRECTIONS / 2) % NUM_DIRECTIONS;
        byte rev_dir_mask = 0x01 << rev_dir;

        int di = directions[i % 2][dir][0];
        int dj = directions[i % 2][dir][1];
        int n_i = i + di;
        int n_j = j + dj;

        if (inbounds (n_i, n_j, grid_size))
        {
            if (grid_in[ind2d (n_i, n_j)] == WALL)
            {
                next_cell
                    |= from_wall_collision (i, j, grid_in, grid_size, dir);
            }
            else if (grid_in[ind2d (n_i, n_j)] & rev_dir_mask)
            {
                next_cell |= rev_dir_mask;
            }
        }
    }

    return check_particles_collision (next_cell);
}

// The update method now receives an interval for the first loop based on the
// index of the worker thread that is calling the method
static void
update (byte *grid_in, byte *grid_out, int grid_size, int i_start, int i_end)
{
    // This is the for that has been parallelized
    for (int i = i_start; i < i_end; i++)
    {
        for (int j = 0; j < grid_size; j++)
        {
            if (grid_in[ind2d (i, j)] == WALL)
                grid_out[ind2d (i, j)] = WALL;
            else
                grid_out[ind2d (i, j)]
                    = get_next_cell (i, j, grid_in, grid_size);
        }
    }
}

// This is the function the worker threads actually run
static void *
pthread_update (void *args)
{
    ptr_thread_arg thread_arg = (ptr_thread_arg)args;

    for (int i = 0; i < ITERATIONS / 2; i++)
    {
        update (thread_arg->grid_1, thread_arg->grid_2, thread_arg->grid_size,
                thread_arg->i_start, thread_arg->i_end);

        // Wait for all threads to finish the first update before continuing
        pthread_barrier_wait (thread_arg->barrier);

        update (thread_arg->grid_2, thread_arg->grid_1, thread_arg->grid_size,
                thread_arg->i_start, thread_arg->i_end);

        // Wait for all threads to finish the second update before continuing
        pthread_barrier_wait (thread_arg->barrier);
    }

    pthread_exit (NULL);
}

void
simulate_pth (byte *grid_1, byte *grid_2, int grid_size, int num_threads)
{
    /***************** Barrier *****************/
    // Creating the syncronization barrier for the threads
    pthread_barrier_t barrier;

    // Initializing the barrier
    pthread_barrier_init (&barrier, NULL, num_threads);

    /***************** Threads *****************/
    // Allocating memory for the threads and the thread arguments
    pthread_t *threads = malloc (num_threads * sizeof (pthread_t));
    thread_arg *args = malloc (num_threads * sizeof (thread_arg));

    // Populating the thread args
    int step = grid_size / num_threads;
    for (int i = 0; i < num_threads; i++)
    {
        args[i].i_start = i * step;
        args[i].i_end = (i + 1) * step;

        args[i].grid_1 = grid_1;
        args[i].grid_2 = grid_2;
        args[i].grid_size = grid_size;

        args[i].barrier = &barrier;
    }

    // Creating the threads
    for (int i = 0; i < num_threads; i++)
        pthread_create (&(threads[i]), NULL, pthread_update, &(args[i]));

    // Waiting for the threads to finish
    for (int i = 0; i < num_threads; i++)
        pthread_join (threads[i], NULL);

    /***************** Cleanup *****************/
    // Disposing of the barriers
    pthread_barrier_destroy (&barrier);

    // Freeing the memory
    free (threads);
    free (args);
}
