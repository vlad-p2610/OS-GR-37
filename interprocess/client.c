/* 
 * Operating Systems  (2INCO)  Practical Assignment
 * Interprocess Communication
 *
 * PARICI VLAD ANDREI (2112744)
 * Catalin Locoman (2128179)
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
#include "request.h"

static void rsleep (int t);


int main (int argc, char * argv[])
{
    // TODO:
    // (see message_queue_test() in interprocess_basic.c)
    //  * open the message queue (whose name is provided in the
    //    arguments)
    //  * repeatingly:
    //      - get the next job request 
    //      - send the request to the Req message queue
    //    until there are no more requests to send
    //  * close the message queue
    
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <REQ_QUEUE_NAME>\n", argv[0]);
        return 1;
    }

    const char *req_qname = argv[1];

    // Openning the request queue
    mqd_t req_mq = mq_open(req_qname, O_WRONLY);
    if (req_mq == (mqd_t)-1) {
        perror("client: mq_open(req)");
        return 1;
    }

    #ifdef getattr
    getattr(req_mq);
    #endif

    while (1) {
        int jobID = 0, data = 0, serviceID = 0;
        int rc = getNextRequest(&jobID, &data, &serviceID);

        if (rc == NO_REQ) {
            // No more requests to send
            break;
        }
        if (rc != NO_ERR) {
            fprintf(stderr, "client: getNextRequest returned error %d\n", rc);
            mq_close(req_mq);
            return 1;
        }

        ipc_msg_t msg;
        msg.kind       = IPC_MSG_REQ;
        msg.job_id     = jobID;
        msg.service_id = serviceID;
        msg.data       = data;
        msg.result     = 0;
        msg._reserved  = 0;

        //only workers are prohibited from busy wait
        ssize_t n;
        do {
             n = mq_send(req_mq, (const char *)&msg, sizeof(ipc_msg_t), 0);
             if(errno != EAGAIN) {
                perror("client: sending");
             }
        } while (n == -1);
    }

    // //termination signal
    // ipc_msg_t msg;
    //     msg.kind       = IPC_MSG_TERM;
    //     msg.job_id     = 0;
    //     msg.service_id = 0;
    //     msg.data       = 0;
    //     msg.result     = 0;
    //     msg._reserved  = 0;

    // if (mq_send(req_mq, (const char *)&msg, sizeof(ipc_msg_t), 0) == -1) {
    //         perror("client: mq_send(req)");
    //         mq_close(req_mq);
    //         return 1;
    // }

    if (mq_close(req_mq) == -1) {
        perror("client: mq_close(req)");
        return 1;
    }

    return (0);
}
