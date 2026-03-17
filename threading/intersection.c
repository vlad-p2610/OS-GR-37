#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#include "arrivals.h"
#include "intersection_time.h"
#include "input.h"

/* 
 * curr_arrivals[][][]
 *
 * A 3D array that stores the arrivals that have occurred
 * The first two indices determine the entry lane: first index is Side, second index is Direction
 * curr_arrivals[s][d] returns an array of all arrivals for the entry lane on side s for direction d,
 *   ordered in the same order as they arrived
 */
static Arrival curr_arrivals[4][3][20];

/*
 * semaphores[][]
 *
 * A 2D array that defines a semaphore for each entry lane,
 *   which are used to signal the corresponding traffic light that a car has arrived
 * The two indices determine the entry lane: first index is Side, second index is Direction
 */
static sem_t semaphores[4][3];

/*
 * 3x3-grid mutex design that we decided to implement:
 *
 * q1 q2 q3
 * q4 q5 q6
 * q7 q8 q9
 *
 * We store them in a 0-based array:
 * q[0] = q1, q[1] = q2, ..., q[8] = q9
 */

static pthread_mutex_t grid_mutexes[9];


// Leaving the same as for the basic implementation
typedef struct
{
  int side;
  int direction;
  struct timespec end_time;
} LightArgs;

/*
 * supply_arrivals()
 *
 * A function for supplying arrivals to the intersection
 * This should be executed by a separate thread
 */
static void* supply_arrivals()
{
  int num_curr_arrivals[4][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

  // for every arrival in the list
  for (int i = 0; i < sizeof(input_arrivals)/sizeof(Arrival); i++)
  {
    // get the next arrival in the list
    Arrival arrival = input_arrivals[i];
    // wait until this arrival is supposed to arrive
    sleep_until_arrival(arrival.time);
    // store the new arrival in curr_arrivals
    curr_arrivals[arrival.side][arrival.direction][num_curr_arrivals[arrival.side][arrival.direction]] = arrival;
    num_curr_arrivals[arrival.side][arrival.direction] += 1;
    // increment the semaphore for the traffic light that the arrival is for
    sem_post(&semaphores[arrival.side][arrival.direction]);
  }
  return 0;
}

/*
 * get_required_cells()
 *
 * Fills cells[] with the q-cells needed by a movement and returns the count.
 *
 * Cell numbers are stored 0-based:
 * q1 -> 0, q2 -> 1, ..., q9 -> 8
 *
 * Your 3x3-grid design:
 *
 * NORTH, RIGHT      -> q1
 * NORTH, STRAIGHT   -> q2, q5, q7, q8
 *
 * EAST, RIGHT       -> q3
 * EAST, STRAIGHT    -> q1, q4, q5, q6
 * EAST, LEFT        -> q7, q8, q9
 *
 * SOUTH, STRAIGHT   -> q3, q6, q9
 * SOUTH, LEFT       -> q1, q2, q5, q8
 *
 * WEST, RIGHT       -> q7
 * WEST, LEFT        -> q3, q4, q5, q6
 *
 * For movements that do not exist in the pictured intersection
 * (NORTH, LEFT), (SOUTH, RIGHT), (WEST, STRAIGHT),
 * we use a conservative fallback: lock the whole grid.
 * That preserves safety if such an input ever appears unexpectedly.
 */
static int get_required_cells(int side, int direction, int cells[9])
{
    int count = 0;

    switch (side)
    {
        case NORTH:
            if (direction == RIGHT)
            {
                cells[count++] = 0;              /* q1 */
            }
            else if (direction == STRAIGHT)
            {
                cells[count++] = 1;              /* q2 */
                cells[count++] = 4;              /* q5 */
                cells[count++] = 6;              /* q7 */
                cells[count++] = 7;              /* q8 */
            }
            else
            {
                /* NORTH, LEFT -> conservative fallback */
                for (int i = 0; i < 9; i++) cells[count++] = i;
            }
            break;

        case EAST:
            if (direction == RIGHT)
            {
                cells[count++] = 2;              /* q3 */
            }
            else if (direction == STRAIGHT)
            {
                cells[count++] = 0;              /* q1 */
                cells[count++] = 3;              /* q4 */
                cells[count++] = 4;              /* q5 */
                cells[count++] = 5;              /* q6 */
            }
            else if (direction == LEFT)
            {
                cells[count++] = 6;              /* q7 */
                cells[count++] = 7;              /* q8 */
                cells[count++] = 8;              /* q9 */
            }
            break;

        case SOUTH:
            if (direction == STRAIGHT)
            {
                cells[count++] = 2;              /* q3 */
                cells[count++] = 5;              /* q6 */
                cells[count++] = 8;              /* q9 */
            }
            else if (direction == LEFT)
            {
                cells[count++] = 0;              /* q1 */
                cells[count++] = 1;              /* q2 */
                cells[count++] = 4;              /* q5 */
                cells[count++] = 7;              /* q8 */
            }
            else
            {
                /* SOUTH, RIGHT -> conservative fallback */
                for (int i = 0; i < 9; i++) cells[count++] = i;
            }
            break;

        case WEST:
            if (direction == RIGHT)
            {
                cells[count++] = 6;              /* q7 */
            }
            else if (direction == LEFT)
            {
                cells[count++] = 2;              /* q3 */
                cells[count++] = 3;              /* q4 */
                cells[count++] = 4;              /* q5 */
                cells[count++] = 5;              /* q6 */
            }
            else
            {
                /* WEST, STRAIGHT -> conservative fallback */
                for (int i = 0; i < 9; i++) cells[count++] = i;
            }
            break;

        default:
            /* Defensive fallback */
            for (int i = 0; i < 9; i++) cells[count++] = i;
            break;
    }

    return count;
}

/*
 * lock_required_cells()
 *
 * Locks all needed cells in ascending order.
 * This fixed global order avoids deadlock.
 */
static void lock_required_cells(const int cells[], int count)
{
    for (int i = 0; i < count; i++)
    {
        pthread_mutex_lock(&grid_mutexes[cells[i]]);
    }
}

/*
 * unlock_required_cells()
 *
 * Unlocks in reverse order.
 */
static void unlock_required_cells(const int cells[], int count)
{
    for (int i = count - 1; i >= 0; i--)
    {
        pthread_mutex_unlock(&grid_mutexes[cells[i]]);
    }
}

/*
 * manage_light(void* arg)
 *
 * Implements one traffic light thread.
 */
static void* manage_light(void* arg)
{
    LightArgs* light = (LightArgs*)arg;
    int side = light->side;
    int direction = light->direction;
    int next_arrival = 0;

    while (1)
    {
        int result = sem_timedwait(&semaphores[side][direction], &light->end_time);

        if (result == -1)
        {
            if (errno == ETIMEDOUT)
            {
                break;
            }
            if (errno == EINTR)
            {
                continue;
            }

            perror("sem_timedwait");
            break;
        }

        Arrival arrival = curr_arrivals[side][direction][next_arrival];
        next_arrival++;

        int cells[9];
        int cell_count = get_required_cells(side, direction, cells);

        lock_required_cells(cells, cell_count);

        printf("traffic light %d %d turns green at time %d for car %d\n",
               side, direction, get_time_passed(), arrival.id);
        fflush(stdout);

        sleep(CROSS_TIME);

        printf("traffic light %d %d turns red at time %d\n",
               side, direction, get_time_passed());
        fflush(stdout);

        unlock_required_cells(cells, cell_count);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    pthread_t light_threads[4][3];
    pthread_t supplier_thread;
    LightArgs light_args[4][3];
    struct timespec end_time;

    /* Initialize lane semaphores */
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            sem_init(&semaphores[i][j], 0, 0);
        }
    }

    /* Initialize 3x3-grid mutexes */
    for (int i = 0; i < 9; i++)
    {
        pthread_mutex_init(&grid_mutexes[i], NULL);
    }

    start_time();

    clock_gettime(CLOCK_REALTIME, &end_time);
    end_time.tv_sec += END_TIME;

    /* One traffic-light thread per lane */
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            light_args[i][j].side = i;
            light_args[i][j].direction = j;
            light_args[i][j].end_time = end_time;

            pthread_create(&light_threads[i][j], NULL, manage_light, &light_args[i][j]);
        }
    }

    /* Arrival-supplier thread */
    pthread_create(&supplier_thread, NULL, supply_arrivals, NULL);

    pthread_join(supplier_thread, NULL);

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            pthread_join(light_threads[i][j], NULL);
        }
    }

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            sem_destroy(&semaphores[i][j]);
        }
    }

    for (int i = 0; i < 9; i++)
    {
        pthread_mutex_destroy(&grid_mutexes[i]);
    }

    return 0;
}
