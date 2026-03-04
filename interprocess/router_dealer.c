/* 
 * Operating Systems  (2INCO)  Practical Assignment
 * Interprocess Communication
 *
 * PARICI VLAD ANDREI (2112744)
 * LOCOMAN CATALIN (STUDENT_NR_2)
 * BAGRIN VICTOR ()
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
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>    
#include <unistd.h>    // for execlp
#include <mqueue.h>    // for mq


#include "settings.h"  
#include "messages.h"

char client2dealer_name[30] = "/client2dealer";
char dealer2worker1_name[30] = "/dealer2worker1";
char dealer2worker2_name[30] = "/dealer2worker2";
char worker2dealer_name[30] = "/worker2dealer";

int main (int argc, char * argv[])
{
  if (argc != 1)
  {
    fprintf (stderr, "%s: invalid arguments\n", argv[0]);
  }
  
  // TODO:
    //  * create the message queues (see message_queue_test() in
    //    interprocess_basic.c)
    struct mq_attr attr;
    attr.mq_maxmsg = MQ_MAX_MESSAGES;
    attr.mq_msgsize = sizeof(ipc_msg_t);

    mqd_t c2d = mq_open(client2dealer_name, O_CREAT | O_RDONLY | O_EXCL, 0600, &attr);
    if(c2d == -1)
    {
        perror("client to router mq creation failed!\n");
        exit(1);
    }

    mqd_t d2w1 = mq_open(dealer2worker1_name, O_CREAT | O_WRONLY | O_EXCL, 0600, &attr);
    if(d2w1 == -1)
    {
        perror("dealer to worker 1 mq creation failed!\n");
        exit(1);
    }

    mqd_t d2w2 = mq_open(dealer2worker2_name, O_CREAT | O_WRONLY | O_EXCL, 0600, &attr);
    if(d2w2 == -1)
    {
        perror("dealer to worker 2 mq creation failed!\n");
        exit(1);
    }

    mqd_t w2d = mq_open(worker2dealer_name, O_CREAT | O_RDONLY | O_EXCL, 0600, &attr);
    if(w2d == -1)
    {
        perror("worker to dealer mq creation failed!\n");
        exit(1);
    }
    //  * create the child processes (see process_test() and
    //    message_queue_test())

    //  * read requests from the Req queue and transfer them to the workers
    //    with the Sx queues

    //  * read answers from workers in the Rep queue and print them

    //  * wait until the client has been stopped (see process_test())

    //  * clean up the message queues (see message_queue_test())
    int rtn_clean;
    rtn_clean = mq_close(c2d);
    if (rtn_clean == -1)
    perror("mq close failed on c2d \n");

    rtn_clean = mq_unlink(client2dealer_name);
    if (rtn_clean == -1)
    perror("mq unlink failed on c2d \n");

    rtn_clean = mq_close(d2w1);
    if (rtn_clean == -1)
    perror("mq close failed on d2w1 \n");

    rtn_clean = mq_unlink(dealer2worker1_name);
    if (rtn_clean == -1)
    perror("mq unlink failed on d2w1 \n");

    rtn_clean = mq_close(d2w2);
    if (rtn_clean == -1)
    perror("mq close failed on d2w2 \n");

    rtn_clean = mq_unlink(dealer2worker2_name);
    if (rtn_clean == -1)
    perror("mq unlink failed on d2w2 \n");

    rtn_clean = mq_close(w2d);
    if (rtn_clean == -1)
    perror("mq close failed on w2d \n");

    rtn_clean = mq_unlink(worker2dealer_name);
    if (rtn_clean == -1)
    perror("mq unlink failed on w2d \n");
    // Important notice: make sure that the names of the message queues
    // contain your goup number (to ensure uniqueness during testing)
  
  return (0);
}
