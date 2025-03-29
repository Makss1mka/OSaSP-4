#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include "./headers/queue.h"

void queue_init(message_queue_t* queue) {
    if (queue == NULL) return;

    queue->head = 0;
    queue->tail = 0;
    queue->len = 0;
}

void queue_push(message_queue_element_t new_message, message_queue_t* queue) {
    if (queue == NULL) {
        printf("Queue: queue ptr is null");
        exit(1);
    }    

    if (queue->len >= QUEUE_SIZE) {
        printf("\nQueue: cannot push message, queue is full");
        fflush(stdout);
        return;
    } 

    if (queue->len != 0) {
        queue->tail = (queue->tail + 1) % QUEUE_SIZE;
    }

    queue->messages[queue->tail] = new_message;
    queue->len++;

    printf("\nQueue: message was pushed");
    fflush(stdout);
}

message_queue_element_t queue_pop(message_queue_t* queue) {
    if (queue == NULL) {
        printf("\nQueue: queue ptr is null");
        exit(1);
    };

    // if (queue->len == 0) {
    //     printf("\nQueue: cannot pop message, queue is empty");
    //     return NULL;
    // }

    message_queue_element_t data = queue->messages[queue->head];

    queue->head = (queue->head + 1) % QUEUE_SIZE;
    queue->len--;

    return data;
}

message_queue_element_t queue_generate_message() {
    message_queue_element_t message;
    message.size = rand() % 20 + 1;
    message.type = 1;
    message.hash = 0;

    for (int i = 0; i < message.size; i++) {
        message.data[i] = rand() % 9 + 1;
    }

    return message;
}

void queue_print(message_queue_t* queue) {
    if (queue == NULL) return;

    printf("\nQueue: current state of queue: ");

    for (int i = queue->head, j = 0; j < queue->len; i = (i + 1) % QUEUE_SIZE, j++) {
        printf("\n\t");
        for (int j = 0; j < queue->messages[i].size; j++) {
            printf("%d", queue->messages[i].data[j]);
        }
    }
}
