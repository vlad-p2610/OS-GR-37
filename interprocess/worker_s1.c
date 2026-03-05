/* 
 * Operating Systems  (2INCO)  Practical Assignment
 * Interprocess Communication
 *
 * Victor Bagrin (2011204)
 *
 *
 * Grading:
 * Your work will be evaluated based on the following criteria:
 * - Satisfaction of all the specifications
 * - Correctness of the program
 * - Coding style
 * - Report quality
 * - Deadlock analysis
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>      // for perror()
#include <unistd.h>     // for getpid()
#include <mqueue.h>     // for mq-stuff
#include <time.h>       // for time()

#include "messages.h"
#include "service1.h"

static void rsleep (int t);


int main (int argc, char * argv[])
{
    // TODO:
    // (see message_queue_test() in interprocess_basic.c)
    //  * open the two message queues (whose names are provided in the
    //    arguments)
    //  * repeatedly:
    //      - read from the S1 message queue the new job to do
    //      - wait a random amount of time (e.g. rsleep(10000);)
    //      - do the job 
    //      - write the results to the Rsp message queue
    //    until there are no more tasks to do
    //  * close the message queues

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <S1_QUEUE_NAME> <RSP_QUEUE_NAME>\n", argv[0]);
        return 1;
    }

    const char *s1_qname  = argv[1];
    const char *rsp_qname = argv[2];

    // Opening  queues
    mqd_t s1_mq = mq_open(s1_qname, O_RDONLY);
    if (s1_mq == (mqd_t)-1) {
        perror("worker_s1: mq_open(S1)");
        return 1;
    }

    mqd_t rsp_mq = mq_open(rsp_qname, O_WRONLY);
    if (rsp_mq == (mqd_t)-1) {
        perror("worker_s1: mq_open(Rsp)");
        mq_close(s1_mq);
        return 1;
    }

    while (1) {
        ipc_msg_t msg;

        while (mq_receive(s1_mq, (char *)&msg, sizeof(ipc_msg_t), NULL) == -1) {
            if (errno == EINTR) continue;
            perror("worker_s1: mq_receive(S1)");
            mq_close(rsp_mq);
            mq_close(s1_mq);
            return 1;
        }

        if (msg.kind == IPC_MSG_TERM) {
            break;
        }

        if (msg.kind != IPC_MSG_JOB) {
            continue;
        }

        rsleep(10000);

        int result = service(msg.data);

        // Response back to router
        ipc_msg_t rsp;
        rsp.kind       = IPC_MSG_RSP;
        rsp.job_id     = msg.job_id;
        rsp.service_id = 0;
        rsp.data       = 0;
        rsp.result     = result;
        rsp._reserved  = 0;

        while (mq_send(rsp_mq, (const char *)&rsp, sizeof(ipc_msg_t), 0) == -1) {
            if (errno == EINTR) continue;
            perror("worker_s1: mq_send(Rsp)");
            mq_close(rsp_mq);
            mq_close(s1_mq);
            return 1;
        }
    }

    if (mq_close(rsp_mq) == -1) {
        perror("worker_s1: mq_close(Rsp)");
    }
    if (mq_close(s1_mq) == -1) {
        perror("worker_s1: mq_close(S1)");
    }


    return(0);
}

/*
 * rsleep(int t)
 *
 * The calling thread will be suspended for a random amount of time
 * between 0 and t microseconds
 * At the first call, the random generator is seeded with the current time
 */
static void rsleep (int t)
{
    static bool first_call = true;
    
    if (first_call == true)
    {
        srandom (time (NULL) % getpid ());
        first_call = false;
    }
    usleep (random() % t);
}
