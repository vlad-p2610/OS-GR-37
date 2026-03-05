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

char client2dealer_name[30];
char dealer2worker1_name[30];
char dealer2worker2_name[30];
char worker2dealer_name[30];

#define STUDENT_NAME "Group_37"

int main (int argc, char * argv[])
{
  if (argc != 1)
  {
    fprintf (stderr, "%s: invalid arguments\n", argv[0]);
  }
  
  // TODO:
    //  * create the message queues (see message_queue_test() in
    //    interprocess_basic.c)
    
    sprintf (client2dealer_name, "/Req_queue_%s_%d", STUDENT_NAME, getpid());
    sprintf (dealer2worker1_name, "/S1_queue_%s_%d", STUDENT_NAME, getpid());
    sprintf (dealer2worker2_name, "/S2_queue_%s_%d", STUDENT_NAME, getpid());
    sprintf (worker2dealer_name, "/Rsp_queue_%s_%d", STUDENT_NAME, getpid());

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
    pid_t client_id, s1_id[N_SERV1], s2_id[N_SERV2];      /* Process ID from fork() */

    fprintf (stderr, "parent pid:%d\n", getpid());
    client_id = fork();
    if (client_id < 0)
    {
        perror("fork() for client failed");
        exit (1);
    }
    else
    {
        if (client_id == 0)
        {
            fprintf (stderr, "client  pid:%d\n", getpid());
            fflush(stdout);
            execlp ("./client", "client", client2dealer_name, NULL);

            // we should never arrive here...
            perror ("execlp() failed");
            exit(1);
        }
    }
    for (int i = 0; i < N_SERV1; i++) {
      s1_id[i] = fork();
      if (s1_id[i] < 0)
      {
          fprintf(stderr, "fork() for s1[%d] failed\n", i);
          exit (1);
      }
      else
      {
        if (s1_id[i] == 0)
        {
            fprintf (stderr, "s1[%d]  pid:%d\n", i, getpid());
            fflush(stdout);
            execlp ("./worker_s1", "worker_s1", dealer2worker1_name, worker2dealer_name, NULL);

            // we should never arrive here...
            perror ("execlp() failed");
            exit(1);
        }
      }
    }

    for (int i = 0; i < N_SERV2; i++) {
      s2_id[i] = fork();
      if (s2_id[i] < 0)
      {
          fprintf(stderr, "fork() for s2[%d] failed\n", i);
          exit (1);
      }
      else
      {
        if (s2_id[i] == 0)
        {
            fprintf (stderr, "s2[%d]  pid:%d\n", i, getpid());
            fflush(stdout);
            execlp ("./worker_s2", "worker_s2", dealer2worker2_name, worker2dealer_name, NULL);

            // we should never arrive here...
            perror ("execlp() failed");
            exit(1);
        }
      }
    }
    //  * read requests from the Req queue and transfer them to the workers
    //    with the Sx queues
    pid_t terminated = (pid_t)0;
    int status;
    while(terminated != client_id) {
        //  * read answers from workers in the Rep queue and print them
        ipc_msg_t clientReq;
        ipc_msg_t dealerReq;
        ipc_msg_t servResp;

        ssize_t n = mq_receive (c2d, (char *) &clientReq, sizeof (clientReq), NULL);
        if (n == -1) perror("mqreceive error in router");

        if (clientReq.service_id == 1) {
          n = mq_send (d2w1, (char *) &dealerReq, sizeof (dealerReq), 0);
          if (n == -1) perror("mqsend error in router for s1");
        }

        if (clientReq.service_id == 12 {
          n = mq_send (d2w2, (char *) &dealerReq, sizeof (dealerReq), 0);
          if (n == -1) perror("mqsend error in router for s2");
        }
        //  * wait until the client has been stopped (see process_test())
        terminated = waitpid(client_id, &status, WNOHANG);
        if (terminated == -1) {
          perror("waitpid on client failed");
          terminated = client_id; //so it can clean the services and mqs
        }
    }

    //send services the death pill and print anything in resp queeu
    int alive = N_SERV1 + N_SERV2;
    while (alive > 0) {

    }

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
