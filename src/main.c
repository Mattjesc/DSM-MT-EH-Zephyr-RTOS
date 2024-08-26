#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#define STACK_SIZE 1024
#define PRODUCER_PRIORITY 5
#define CONSUMER_PRIORITY 7
#define TIMER_PRIORITY 8
#define EVENT_THREAD_PRIORITY 6
#define MSG_SIZE 16
#define MSG_QUEUE_SIZE 10

// Event identifiers
enum event_id {
    EVENT_START,
    EVENT_STOP,
    EVENT_PAUSE,
    EVENT_RESUME,
    EVENT_ERROR,
    EVENT_RECOVER,
    EVENT_RESET,
    EVENT_WAIT,
    EVENT_CONTINUE,
    EVENT_RETRY
};

// Define a message queue with a message size of 16 bytes and a queue size of 10 messages
K_MSGQ_DEFINE(my_msgq, sizeof(char *), MSG_QUEUE_SIZE, 4);

// Define an event queue
K_MSGQ_DEFINE(event_queue, sizeof(enum event_id), 10, 4);

// Define a mutex to protect the shared resource
K_MUTEX_DEFINE(my_mutex);

// State machine states
enum states {
    IDLE,
    RUNNING,
    PAUSED,
    ERROR,
    FINISHED,
    RESET,
    WAITING,
    RETRY
};

volatile enum states current_state = IDLE;

// Timer handler function
void timer_handler(struct k_timer *dummy);

// Define a timer
K_TIMER_DEFINE(my_timer, timer_handler, NULL);

// Producer thread function
void producer_thread(void)
{
    while (1) {
        if (current_state == RUNNING) {
            char *message = k_malloc(MSG_SIZE);
            if (message == NULL) {
                printk("Failed to allocate memory for message\n");
                enum event_id error_event = EVENT_ERROR;
                k_msgq_put(&event_queue, &error_event, K_NO_WAIT);
                continue;
            }

            k_mutex_lock(&my_mutex, K_FOREVER);
            snprintf(message, MSG_SIZE, "Data %d", k_uptime_get_32());
            printk("Producing: %s\n", message);

            if (k_msgq_put(&my_msgq, &message, K_NO_WAIT) != 0) {
                printk("Message queue is full. Freeing message.\n");
                k_free(message);
            }

            k_mutex_unlock(&my_mutex);
            k_sleep(K_SECONDS(1));
        } else if (current_state == PAUSED || current_state == ERROR || current_state == WAITING || current_state == RETRY) {
            k_sleep(K_SECONDS(1));
        } else {
            k_sleep(K_SECONDS(1));
        }
    }
}

// Consumer thread function
void consumer_thread(void)
{
    char *msg;
    while (1) {
        if (current_state == RUNNING) {
            if (k_msgq_get(&my_msgq, &msg, K_FOREVER) == 0) {
                k_mutex_lock(&my_mutex, K_FOREVER);
                printk("Consuming: %s\n", msg);
                k_free(msg);
                k_mutex_unlock(&my_mutex);
                k_sleep(K_SECONDS(1));
            }
        } else if (current_state == PAUSED || current_state == ERROR || current_state == WAITING || current_state == RETRY) {
            k_sleep(K_SECONDS(1));
        } else {
            k_sleep(K_SECONDS(1));
        }
    }
}

// Event handler thread function
void event_handler_thread(void)
{
    enum event_id event;
    while (1) {
        printk("Waiting for event...\n");
        k_msgq_get(&event_queue, &event, K_FOREVER);
        printk("Handling event: %d\n", event);

        switch (event) {
            case EVENT_START:
                printk("Event: START received.\n");
                if (current_state == IDLE || current_state == PAUSED) {
                    current_state = RUNNING;
                    printk("State changed to RUNNING.\n");
                }
                break;
            case EVENT_STOP:
                printk("Event: STOP received.\n");
                if (current_state == RUNNING || current_state == PAUSED) {
                    current_state = FINISHED;
                    printk("State changed to FINISHED.\n");
                    k_timer_stop(&my_timer);
                }
                break;
            case EVENT_PAUSE:
                printk("Event: PAUSE received.\n");
                if (current_state == RUNNING) {
                    current_state = WAITING;
                    printk("State changed to WAITING.\n");
                }
                break;
            case EVENT_RESUME:
                printk("Event: RESUME received.\n");
                if (current_state == WAITING) {
                    current_state = RUNNING;
                    printk("State changed to RUNNING.\n");
                }
                break;
            case EVENT_ERROR:
                printk("Event: ERROR received.\n");
                if (current_state == RUNNING) {
                    current_state = ERROR;
                    printk("State changed to ERROR.\n");
                }
                break;
            case EVENT_RECOVER:
                printk("Event: RECOVER received.\n");
                if (current_state == ERROR) {
                    current_state = RETRY;
                    printk("State changed to RETRY.\n");
                }
                break;
            case EVENT_RESET:
                printk("Event: RESET received.\n");
                if (current_state == FINISHED) {
                    current_state = RESET;
                    printk("State changed to RESET.\n");
                    // Automatically transition back to IDLE after a short delay
                    k_sleep(K_SECONDS(2));
                    current_state = IDLE;
                    printk("State changed to IDLE.\n");
                }
                break;
            case EVENT_WAIT:
                printk("Event: WAIT received.\n");
                if (current_state == RUNNING) {
                    current_state = WAITING;
                    printk("State changed to WAITING.\n");
                }
                break;
            case EVENT_CONTINUE:
                printk("Event: CONTINUE received.\n");
                if (current_state == WAITING) {
                    current_state = RUNNING;
                    printk("State changed to RUNNING.\n");
                }
                break;
            case EVENT_RETRY:
                printk("Event: RETRY received.\n");
                if (current_state == ERROR) {
                    current_state = RETRY;
                    printk("State changed to RETRY.\n");
                }
                break;
        }
    }
}

// Simulate event thread function (starts the system)
void event_start_thread(void)
{
    printk("Event Start Thread Running...\n");
    while (1) {
        k_sleep(K_SECONDS(5));
        if (current_state == IDLE || current_state == PAUSED) {
            enum event_id event = EVENT_START;
            k_msgq_put(&event_queue, &event, K_NO_WAIT);
            printk("Simulating event: START\n");
        }
    }
}

// Simulate event thread function (stops the system)
void event_stop_thread(void)
{
    printk("Event Stop Thread Running...\n");
    while (1) {
        k_sleep(K_SECONDS(20));
        if (current_state == RUNNING || current_state == PAUSED) {
            enum event_id event = EVENT_STOP;
            k_msgq_put(&event_queue, &event, K_NO_WAIT);
            printk("Simulating event: STOP\n");
        }
    }
}

// Simulate event thread function (pauses the system)
void event_pause_thread(void)
{
    printk("Event Pause Thread Running...\n");
    while (1) {
        k_sleep(K_SECONDS(10));
        if (current_state == RUNNING) {
            enum event_id event = EVENT_PAUSE;
            k_msgq_put(&event_queue, &event, K_NO_WAIT);
            printk("Simulating event: PAUSE\n");
        }
    }
}

// Simulate event thread function (resumes the system)
void event_resume_thread(void)
{
    printk("Event Resume Thread Running...\n");
    while (1) {
        k_sleep(K_SECONDS(15));
        if (current_state == WAITING) {
            enum event_id event = EVENT_RESUME;
            k_msgq_put(&event_queue, &event, K_NO_WAIT);
            printk("Simulating event: RESUME\n");
        }
    }
}

// Simulate event thread function (recovers from error)
void event_recover_thread(void)
{
    printk("Event Recover Thread Running...\n");
    while (1) {
        k_sleep(K_SECONDS(25));
        if (current_state == ERROR) {
            enum event_id event = EVENT_RECOVER;
            k_msgq_put(&event_queue, &event, K_NO_WAIT);
            printk("Simulating event: RECOVER\n");
        }
    }
}

// Simulate event thread function (reset the system)
void event_reset_thread(void)
{
    printk("Event Reset Thread Running...\n");
    while (1) {
        k_sleep(K_SECONDS(30));
        if (current_state == FINISHED) {
            enum event_id event = EVENT_RESET;
            k_msgq_put(&event_queue, &event, K_NO_WAIT);
            printk("Simulating event: RESET\n");
        }
    }
}

// Timer handler function
void timer_handler(struct k_timer *dummy)
{
    printk("Timer handler executed.\n");

    switch (current_state) {
        case IDLE:
            printk("State: IDLE. Changing to RUNNING.\n");
            current_state = RUNNING;
            break;
        case RUNNING:
            printk("State: RUNNING. Changing to FINISHED.\n");
            current_state = FINISHED;
            break;
        case PAUSED:
        case ERROR:
        case WAITING:
        case FINISHED:
        case RESET:
        case RETRY:
            printk("State: %s. No further state changes.\n", 
                   current_state == PAUSED ? "PAUSED" : 
                   current_state == ERROR ? "ERROR" : 
                   current_state == WAITING ? "WAITING" : 
                   current_state == RESET ? "RESET" : 
                   current_state == RETRY ? "RETRY" : "FINISHED");
            k_timer_stop(&my_timer);
            break;
    }
}

// Timer thread function
void timer_thread(void)
{
    printk("Timer Thread Running...\n");
    k_timer_start(&my_timer, K_SECONDS(5), K_SECONDS(5));

    while (1) {
        k_sleep(K_FOREVER);
    }
}

// Define threads with different priorities
K_THREAD_DEFINE(producer_tid, STACK_SIZE, producer_thread, NULL, NULL, NULL, PRODUCER_PRIORITY, 0, 0);
K_THREAD_DEFINE(consumer_tid, STACK_SIZE, consumer_thread, NULL, NULL, NULL, CONSUMER_PRIORITY, 0, 0);
K_THREAD_DEFINE(timer_tid, STACK_SIZE, timer_thread, NULL, NULL, NULL, TIMER_PRIORITY, 0, 0);
K_THREAD_DEFINE(event_handler_tid, STACK_SIZE, event_handler_thread, NULL, NULL, NULL, EVENT_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(event_start_tid, STACK_SIZE, event_start_thread, NULL, NULL, NULL, EVENT_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(event_stop_tid, STACK_SIZE, event_stop_thread, NULL, NULL, NULL, EVENT_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(event_pause_tid, STACK_SIZE, event_pause_thread, NULL, NULL, NULL, EVENT_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(event_resume_tid, STACK_SIZE, event_resume_thread, NULL, NULL, NULL, EVENT_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(event_recover_tid, STACK_SIZE, event_recover_thread, NULL, NULL, NULL, EVENT_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(event_reset_tid, STACK_SIZE, event_reset_thread, NULL, NULL, NULL, EVENT_THREAD_PRIORITY, 0, 0);

// Main function
void main(void)
{
    printk("Multithreading with Expanded State Machine Example\n");
}
