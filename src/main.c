#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/wait.h>  
#include <sys/sem.h>
#include <sys/ipc.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <inttypes.h>
#include "./headers/queue.h"

#define MAX_CONSUMER_PROCESSES 10
#define MAX_PRODUCER_PROCESSES 10

int consumers_count = 0;
int producers_count = 0;

pid_t consumers[MAX_CONSUMER_PROCESSES];
pid_t producers[MAX_PRODUCER_PROCESSES];

key_t key_queue;
key_t sem_key;

message_queue_t* message_queue;
int queue_shm_id;
int sem_id;

int is_working = 1;

typedef enum {
    ITEMS_COUNT_SEM,
    FREE_SPACE_SEM,
    MUTEX_SEM
} semaphore_type;

// SIGNALS

void termination_handler(int signum) {
    (void)signum;
    is_working = 0;
}

void sem_wait(int sem_num) {
    struct sembuf op = {sem_num, -1, 0};
    if (semop(sem_id, &op, 1) == -1) {  
        perror("Semaphore wait operation failed ");
        exit(1);
    }
}

void sem_up(int sem_num) {
    struct sembuf op = {sem_num, 1, 0};
    if (semop(sem_id, &op, 1) == -1) {
        perror("Semaphore up operation failed ");
        exit(1);
    }
}



// SEMS INIT AND END

void init_semaphores() {
    sem_id = semget(sem_key, 3, IPC_CREAT | 0666);

    if (sem_id == -1) {
        perror("Semaphore creation failed");
        exit(1);
    }

    semctl(sem_id, ITEMS_COUNT_SEM, SETVAL, 0);
    semctl(sem_id, FREE_SPACE_SEM, SETVAL, QUEUE_SIZE);
    semctl(sem_id, MUTEX_SEM, SETVAL, 1);
}


// QUEUE OPS

void create_shared_queue() {
    queue_shm_id = shmget(key_queue, sizeof(message_queue_t), IPC_CREAT | 0666);
    if (queue_shm_id == -1) {
        perror("Main: shared segment creation failure.");
        exit(1);
    }

    message_queue = (message_queue_t*)shmat(queue_shm_id, NULL, 0);
    if (message_queue == (void*) -1) {
        perror("Main: cannot link shared queue.");
        exit(1);
    }

    queue_init(message_queue);
}

void unlink_and_free_queue() {
    shmdt(message_queue);
    shmctl(sem_id, IPC_RMID, NULL);
}


// CHILD PROCCESSING

void producer_process() {
    signal(SIGUSR1, termination_handler);
    
    while (is_working) {
        sleep(5);

        sem_wait(FREE_SPACE_SEM);
        sem_wait(MUTEX_SEM);

        queue_push(queue_generate_message(), message_queue);
        printf("\nProducer (PID %d): pushed item\n", getpid());
        fflush(stdout);

        sem_up(MUTEX_SEM);
        sem_up(ITEMS_COUNT_SEM);
    }

    printf("\nProducer: Terminating PID %d\n", getpid());
    exit(EXIT_SUCCESS);
}

void consumer_process() {
    signal(SIGUSR1, termination_handler);

    while (is_working) {
        sleep(2);

        sem_wait(ITEMS_COUNT_SEM);
        sem_wait(MUTEX_SEM);

        message_queue_element_t data = queue_pop(message_queue);

        printf("\nConsumer (PID %d), popped from queue: ", getpid());
        for (int i = 0; i < data.size; i++) {
            printf("%d", data.data[i]);
        }
        printf("\n");
        fflush(stdout);

        sem_up(MUTEX_SEM);
        sem_up(FREE_SPACE_SEM);
    }

    printf("\nConsumer: Terminating PID %d\n", getpid());
    exit(EXIT_SUCCESS);
}



// CHILD CREATION

void create_child(int opt) {
    // opt = +1 - producer 
    // opt = -1 - consumer

    if (opt == 1) {
        if (producers_count < MAX_PRODUCER_PROCESSES) {
            pid_t pid = fork();
            if (pid == -1) {
                perror("Error when creating new producer");
                exit(1);
            } else if (pid == 0) {
                producer_process();
            } else if (pid > 0) {
                printf("Parent: Created new producer with PID %d\n", pid);
                producers[producers_count++] = pid;
            }
        } else {
            printf("Parent: producers limit is reached");
        }
    } else if (opt == -1) {
        if (consumers_count < MAX_CONSUMER_PROCESSES) {
            pid_t pid = fork();

            if (pid == -1) {
                perror("Error when creating new consumer");
                exit(1);
            } else if (pid == 0) {
                consumer_process();
            } else if (pid > 0) {
                printf("Parent: Created new consumer with PID %d\n", pid);
                consumers[consumers_count++] = pid;
            }
        } else {
            printf("Parent: consumers limit is reached");
        }
    } else {
        printf("Parent: invalid child create option.");
    }
}


// KILLING PROCESSES

void kill_process_by_ind(int ind, int type) {
    if (type == 1) {
        if (producers_count > 0 && ind < producers_count) {
            pid_t pid = producers[ind];
    
            kill(pid, SIGUSR1);
    
            for (int i = ind + 1; i < producers_count; i++) {
                producers[i - 1] = producers[i];
            }
    
            producers_count--;
    
            printf("Parent: Killed producer with PID %d. Remaining: %d\n", pid, producers_count);
        } else {
            printf("Parent: No producers to kill\n");
        }
    } else if (type == -1) {
        if (consumers_count > 0 && ind < consumers_count) {
            pid_t pid = consumers[ind];
    
            kill(pid, SIGUSR1);
    
            for (int i = ind + 1; i < consumers_count; i++) {
                consumers[i - 1] = consumers[i];
            }
    
            consumers_count--;
    
            printf("Parent: Killed consumer with PID %d. Remaining: %d\n", pid, consumers_count);
        } else {
            printf("Parent: No consumers to kill\n");
        }
    } else {
        printf("Parent: invalid process type\n");
    }

}

void kill_all_processes(int type) {
    if (type == 1) {
        while (producers_count > 0) {
            kill_process_by_ind(producers_count - 1, type);
        }
    
        printf("Parent: Killed all producers\n");
    } else if (type == -1) {
        while (consumers_count > 0) {
            kill_process_by_ind(consumers_count - 1, type);
        }
    
        printf("Parent: Killed all consumers\n");
    } else {
        printf("Parent: invalid process type\n");
    }
}


// EXIT PROGRAM

void cleanup_and_exit(void) {
    printf("\nShutting down...\n");

    kill_all_processes(1);
    kill_all_processes(-1);

    while (wait(NULL) > 0);

    if (message_queue) {
        unlink_and_free_queue();
    }
    semctl(sem_id, 0, IPC_RMID);

    printf("\nCleanup complete. Exiting...\n");
    exit(EXIT_SUCCESS);
}



int main() {
    key_queue = ftok("/home/maks/PopuskPapka/lab04", 16);
    sem_key = ftok("/home/maks/PopuskPapka/lab04", 160);

    if (sem_key == -1) {
        perror("ftok failed");
        exit(1);
    }

    create_shared_queue();
    init_semaphores();

    printf("\nEnter option:");
    printf("\n+ add producer");
    printf("\n- remove last added producer");
    printf("\n* add consumer");
    printf("\n_ remove last added consumer");
    printf("\nl print queue");
    printf("\ns print childs");
    printf("\nq quit");

    while (1) {
        char option[10];

        printf("\nOPtion: ");
        if (scanf("%9s", option) != 1) {
            continue;
        }

        if (strcmp(option, "+") == 0) {
            create_child(1);
        } else if (strcmp(option, "*") == 0) {
            create_child(-1);
        } else if (strcmp(option, "-") == 0) {
            kill_process_by_ind(producers_count - 1, 1);
        } else if (strcmp(option, "_") == 0) {
            kill_process_by_ind(consumers_count - 1, -1);
        } else if (strcmp(option, "l") == 0) {
            queue_print(message_queue);
        } else if (strcmp(option, "s") == 0) {
            printf("\nParent: processes list:");

            for (int i = 0; i < producers_count; i++) {
                printf("\n\tProducer: pid %d", producers[i]);
            }

            for (int i = 0; i < consumers_count; i++) {
                printf("\n\tConsumer: pid %d", consumers[i]);
            }

            printf("\n");
        } else if (strcmp(option, "q") == 0) {
            cleanup_and_exit();
            printf("Parent: Exiting\n");
            break;
        } 
    }
    
    return 0;
}

    

