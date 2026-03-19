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
 * 7-mutex conflict design
 *
 * Mutex 0 (South Merge):           { NORTH-STRAIGHT, EAST-LEFT, WEST-RIGHT }
 * Mutex 1 (West Merge):            { NORTH-RIGHT, EAST-STRAIGHT, SOUTH-LEFT }
 * Mutex 2 (North Merge):           { EAST-RIGHT, SOUTH-STRAIGHT, WEST-LEFT }
 * Mutex 3 (Center-South Crossing): { NORTH-STRAIGHT, EAST-LEFT, SOUTH-LEFT }
 * Mutex 4 (Grand Central):         { NORTH-STRAIGHT, EAST-STRAIGHT, SOUTH-LEFT, WEST-LEFT }
 * Mutex 5 (Center-North Crossing): { EAST-STRAIGHT, SOUTH-STRAIGHT, WEST-LEFT }
 * Mutex 6 (Center-East Crossing):  { EAST-LEFT, SOUTH-STRAIGHT }
 */
static pthread_mutex_t conflict_mutexes[7];

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
    int num_curr_arrivals[4][3] = {
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
    };

    for (int i = 0; i < (int)(sizeof(input_arrivals) / sizeof(Arrival)); i++)
    {
        Arrival arrival = input_arrivals[i];

        sleep_until_arrival(arrival.time);

        curr_arrivals[arrival.side][arrival.direction]
                     [num_curr_arrivals[arrival.side][arrival.direction]] = arrival;
        num_curr_arrivals[arrival.side][arrival.direction] += 1;

        sem_post(&semaphores[arrival.side][arrival.direction]);
    }

    return NULL;
}

/*
 * get_required_mutexes()
 *
 * Returns in mutexes[] the list of mutex IDs required by the path
 * corresponding to (side, direction), and returns the number of mutexes.
 *
 * The mutex IDs are always stored in strictly increasing order.
 * This lets all threads lock them in the same global order, which prevents deadlock.
 *
 * For invalid/not pictured movements, we conservatively lock all mutexes.
 */
static int get_required_mutexes(int side, int direction, int mutexes[7])
{
    int count = 0;

    switch (side)
    {
        case NORTH:
            if (direction == RIGHT)
            {
                /* NORTH-RIGHT -> {1} */
                mutexes[count++] = 1;
            }
            else if (direction == STRAIGHT)
            {
                /* NORTH-STRAIGHT -> {0, 3, 4} */
                mutexes[count++] = 0;
                mutexes[count++] = 3;
                mutexes[count++] = 4;
            }
            else
            {
                /* NORTH-LEFT invalid -> conservative fallback */
                for (int i = 0; i < 7; i++) mutexes[count++] = i;
            }
            break;

        case EAST:
            if (direction == RIGHT)
            {
                /* EAST-RIGHT -> {2} */
                mutexes[count++] = 2;
            }
            else if (direction == STRAIGHT)
            {
                /* EAST-STRAIGHT -> {1, 4, 5} */
                mutexes[count++] = 1;
                mutexes[count++] = 4;
                mutexes[count++] = 5;
            }
            else if (direction == LEFT)
            {
                /* EAST-LEFT -> {0, 3, 6} */
                mutexes[count++] = 0;
                mutexes[count++] = 3;
                mutexes[count++] = 6;
            }
            break;

        case SOUTH:
            if (direction == STRAIGHT)
            {
                /* SOUTH-STRAIGHT -> {2, 5, 6} */
                mutexes[count++] = 2;
                mutexes[count++] = 5;
                mutexes[count++] = 6;
            }
            else if (direction == LEFT)
            {
                /* SOUTH-LEFT -> {1, 3, 4} */
                mutexes[count++] = 1;
                mutexes[count++] = 3;
                mutexes[count++] = 4;
            }
            else
            {
                /* SOUTH-RIGHT invalid -> conservative fallback */
                for (int i = 0; i < 7; i++) mutexes[count++] = i;
            }
            break;

        case WEST:
            if (direction == RIGHT)
            {
                /* WEST-RIGHT -> {0} */
                mutexes[count++] = 0;
            }
            else if (direction == LEFT)
            {
                /* WEST-LEFT -> {2, 4, 5} */
                mutexes[count++] = 2;
                mutexes[count++] = 4;
                mutexes[count++] = 5;
            }
            else
            {
                /* WEST-STRAIGHT invalid -> conservative fallback */
                for (int i = 0; i < 7; i++) mutexes[count++] = i;
            }
            break;

        default:
            /* Defensive fallback */
            for (int i = 0; i < 7; i++) mutexes[count++] = i;
            break;
    }

    return count;
}

/*
 * lock_required_mutexes()
 *
 * Locks all required mutexes in ascending order.
 */
static void lock_required_mutexes(const int mutexes[], int count)
{
    for (int i = 0; i < count; i++)
    {
        pthread_mutex_lock(&conflict_mutexes[mutexes[i]]);
    }
}

/*
 * unlock_required_mutexes()
 *
 * Unlocks all required mutexes in reverse order.
 */
static void unlock_required_mutexes(const int mutexes[], int count)
{
    for (int i = count - 1; i >= 0; i--)
    {
        pthread_mutex_unlock(&conflict_mutexes[mutexes[i]]);
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

        int mutexes[7];
        int mutex_count = get_required_mutexes(side, direction, mutexes);

        lock_required_mutexes(mutexes, mutex_count);

        printf("traffic light %d %d turns green at time %d for car %d\n",
               side, direction, get_time_passed(), arrival.id);
        fflush(stdout);

        sleep(CROSS_TIME);

        printf("traffic light %d %d turns red at time %d\n",
               side, direction, get_time_passed());
        fflush(stdout);

        unlock_required_mutexes(mutexes, mutex_count);
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

    /* Initialize conflict mutexes */
    for (int i = 0; i < 7; i++)
    {
        pthread_mutex_init(&conflict_mutexes[i], NULL);
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

    for (int i = 0; i < 7; i++)
    {
        pthread_mutex_destroy(&conflict_mutexes[i]);
    }

    return 0;
}
