/* 
 * Operating Systems  (2INC0)  Practical Assignment.
 * Condition Variables Application.
 *
 * PARICI_VLAD (2112744)
 * LOCOMAN_CATALIN (2128179)
 * BAGRIN_VICTOR (2011204)
 *
 * Grading:
 * Students who hand in clean code that fully satisfies the minimum requirements will get an 8. 
 * Extra steps can lead to higher marks because we want students to take the initiative.
 */
 
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>  
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "prodcons.h"

static ITEM buffer[BUFFER_SIZE];

static void rsleep (int t);	    // already implemented (see below)
static ITEM get_next_item (void);   // already implemented (see below)

// ours
int count = 0;		   // items in buffer
//we will implement a circular buffer so we dont have to shift it
int in = 0;			   // how many items put in buffer so far
int out = 0;		   // how many items pulled out of buffer so far
int next_expected = 0; // which one should come next ascending order
static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER; //protects buffer and the 3 counters
static pthread_cond_t can_produce = PTHREAD_COND_INITIALIZER;
static pthread_cond_t can_consume = PTHREAD_COND_INITIALIZER;
pthread_t prods[NROF_PRODUCERS];
pthread_t cons;


/* producer thread */
static void * 
producer (void * arg)
{	
	int rtnval;
	ITEM item = get_next_item();
    while (item != NROF_ITEMS) /* TODO: not all items produced */
    {
        rsleep (100);	// simulating all kind of activities...
	
		rtnval = pthread_mutex_lock (&buffer_mutex);
		if(rtnval != 0) {
			perror ("mutex lock failed in prod");
            exit (1);
		}

		while (count + 1 > BUFFER_SIZE || item != next_expected) {
			rtnval = pthread_cond_wait (&can_produce, &buffer_mutex);
			if(rtnval != 0) {
				perror ("wait failed in prod");
            	exit (1);
			}
		}
		
		buffer[in % BUFFER_SIZE] = item;
		in++;
		next_expected++;
		count++;
        
		rtnval = pthread_cond_signal (&can_consume);
		if(rtnval != 0) {
			perror ("signal failed in prod");
            exit (1);
		}

		rtnval = pthread_mutex_unlock (&buffer_mutex);
		if(rtnval != 0) {
			perror ("mutex unlock failed in prod");
            exit (1);
		}

		item = get_next_item();
    }
	return (NULL);
}

/* consumer thread */
static void * 
consumer (void * arg)
{
	int rtnval;
    while (out != NROF_ITEMS)/* TODO: not all items retrieved from buffer[] */
    {
		rtnval = pthread_mutex_lock (&buffer_mutex);
		if(rtnval != 0) {
			perror ("mutex lock failed in cons");
            exit (1);
		}

		while (count == 0) {
			rtnval = pthread_cond_wait (&can_consume, &buffer_mutex);
			if(rtnval != 0) {
				perror ("wait failed in cons");
            	exit (1);
			}
		}

		ITEM item = buffer[out % BUFFER_SIZE];
		out++;
		count--;

		printf ("%d\n", item);

		rtnval = pthread_cond_broadcast (&can_produce);
		if(rtnval != 0) {
			perror ("signal failed in cons");
            exit (1);
		}

		rtnval = pthread_mutex_unlock (&buffer_mutex);
		if(rtnval != 0) {
			perror ("mutex unlock failed in cons");
            exit (1);
		}

        // TODO: 
	      // * get the next item from buffer[]
	      // * print the number to stdout
        //
        // follow this pseudocode (according to the ConditionSynchronization lecture):
        //      mutex-lock;
        //      while not condition-for-this-consumer
        //          wait-cv;
        //      critical-section;
        //      possible-cv-signals;
        //      mutex-unlock;
		
        rsleep (100);		// simulating all kind of activities...
    }
	return (NULL);
}

int main (void)
{
    // TODO: 
    // * startup the producer threads and the consumer thread
    // * wait until all threads are finished  
	int rtnval;
    rtnval = pthread_create (&cons, NULL, consumer, NULL);
	if(rtnval != 0) {
		perror ("create consumer failed");
        exit (1);
	}

	for (int i = 0; i < NROF_PRODUCERS; i++) {
		rtnval = pthread_create (&prods[i], NULL, producer, NULL);
		if(rtnval != 0) {
			perror ("create prod failed");
        	exit (1);
		}
	}


	for (int i = 0; i < NROF_PRODUCERS; i++) {
		rtnval = pthread_join (prods[i], NULL);
		if(rtnval != 0) {
			perror ("join prod failed");
        	exit (1);
		}
	}

	rtnval = pthread_join (cons, NULL);
	if(rtnval != 0) {
		perror ("join consumer failed");
        exit (1);
	}

    return (0);
}

/*
 * rsleep(int t)
 *
 * The calling thread will be suspended for a random amount of time between 0 and t microseconds
 * At the first call, the random generator is seeded with the current time
 */
static void 
rsleep (int t)
{
    static bool first_call = true;
    
    if (first_call == true)
    {
        srandom (time(NULL));
        first_call = false;
    }
    usleep (random () % t);
}


/* 
 * get_next_item()
 *
 * description:
 *	thread-safe function to get a next job to be executed
 *	subsequent calls of get_next_item() yields the values 0..NROF_ITEMS-1 
 *	in arbitrary order 
 *	return value NROF_ITEMS indicates that all jobs have already been given
 * 
 * parameters:
 *	none
 *
 * return value:
 *	0..NROF_ITEMS-1: job number to be executed
 *	NROF_ITEMS:	 ready
 */
static ITEM
get_next_item(void)
{
    static pthread_mutex_t  job_mutex   = PTHREAD_MUTEX_INITIALIZER;
    static bool    jobs[NROF_ITEMS+1] = { false }; // keep track of issued jobs
    static int     counter = 0;    // seq.nr. of job to be handled
    ITEM           found;          // item to be returned
	
	/* avoid deadlock: when all producers are busy but none has the next expected item for the consumer 
	 * so requirement for get_next_item: when giving the (i+n)'th item, make sure that item (i) is going to be handled (with n=nrof-producers)
	 */
	pthread_mutex_lock (&job_mutex);

	counter++;
	if (counter > NROF_ITEMS)
	{
	    // we're ready
	    found = NROF_ITEMS;
	}
	else
	{
	    if (counter < NROF_PRODUCERS)
	    {
	        // for the first n-1 items: any job can be given
	        // e.g. "random() % NROF_ITEMS", but here we bias the lower items
	        found = (random() % (2*NROF_PRODUCERS)) % NROF_ITEMS;
	    }
	    else
	    {
	        // deadlock-avoidance: item 'counter - NROF_PRODUCERS' must be given now
	        found = counter - NROF_PRODUCERS;
	        if (jobs[found] == true)
	        {
	            // already handled, find a random one, with a bias for lower items
	            found = (counter + (random() % NROF_PRODUCERS)) % NROF_ITEMS;
	        }    
	    }
	    
	    // check if 'found' is really an unhandled item; 
	    // if not: find another one
	    if (jobs[found] == true)
	    {
	        // already handled, do linear search for the oldest
	        found = 0;
	        while (jobs[found] == true)
            {
                found++;
            }
	    }
	}
    	jobs[found] = true;
			
	pthread_mutex_unlock (&job_mutex);
	return (found);
}



