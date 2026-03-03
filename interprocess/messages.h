/* 
 * Operating Systems  (2INCO)  Practical Assignment
 * Interprocess Communication
 *
 * Catalin Locoman (2128179)
 *
 * Grading:
 * Your work will be evaluated based on the following criteria:
 * - Satisfaction of all the specifications
 * - Correctness of the program
 * - Coding style
 * - Report quality
 * - Deadlock analysis
 */

#ifndef MESSAGES_H
#define MESSAGES_H

/*
 * IMPORTANT RULE:
 *  - Create ALL message queues with mq_msgsize = sizeof(ipc_msg_t)
 *  - Always mq_send / mq_receive using sizeof(ipc_msg_t)
 */

typedef enum {
    IPC_MSG_REQ  = 1,   // client -> router: {job_id, service_id, data}
    IPC_MSG_JOB  = 2,   // router -> worker: {job_id, data}
    IPC_MSG_RSP  = 3,   // worker -> router: {job_id, result}
    IPC_MSG_TERM = 4    // router -> worker: termination signal (poison pill)
} ipc_msg_kind_t;

typedef struct {
    int32_t kind;        // ipc_msg_kind_t
    int32_t job_id;      // request id

    int32_t service_id;  // only meaningful for IPC_MSG_REQ (1 or 2)
    int32_t data;        // input data (REQ/JOB)
    int32_t result;      // output (RSP)

    int32_t _reserved;   // keep struct aligned
} ipc_msg_t;


#endif
