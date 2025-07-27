#include "kernel.h"

// IPC message queues per task
ipc_message_t message_queues[MAX_TASKS][MAX_IPC_MESSAGES];
int queue_head[MAX_TASKS];
int queue_tail[MAX_TASKS];
int queue_count[MAX_TASKS];

// Blocked tasks waiting for messages
pcb_t *blocked_tasks[MAX_TASKS];
int blocked_count = 0;

// Initialize IPC subsystem
void ipc_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        queue_head[i] = 0;
        queue_tail[i] = 0;
        queue_count[i] = 0;
    }
    blocked_count = 0;
}

// Send message to task
int sys_send(int pid, void *msg, size_t len) {
    if (!current_task) {
        return -1;
    }
    
    if (len > MAX_MESSAGE_SIZE) {
        return -1;
    }
    
    // Find target task
    pcb_t *target = get_task_by_pid(pid);
    if (!target) {
        return -1;
    }
    
    // Check if target task's queue is full
    if (queue_count[target->pid] >= MAX_IPC_MESSAGES) {
        return -1;
    }
    
    // Create message
    ipc_message_t *message = &message_queues[target->pid][queue_tail[target->pid]];
    message->sender_pid = current_task->pid;
    message->receiver_pid = pid;
    message->size = len;
    
    // Copy message data
    for (size_t i = 0; i < len; i++) {
        message->data[i] = ((char*)msg)[i];
    }
    
    // Add to queue
    queue_tail[target->pid] = (queue_tail[target->pid] + 1) % MAX_IPC_MESSAGES;
    queue_count[target->pid]++;
    
    // Unblock target task if it was waiting
    for (int i = 0; i < blocked_count; i++) {
        if (blocked_tasks[i] && blocked_tasks[i]->pid == pid) {
            unblock_task(blocked_tasks[i]);
            // Remove from blocked list
            for (int j = i; j < blocked_count - 1; j++) {
                blocked_tasks[j] = blocked_tasks[j + 1];
            }
            blocked_count--;
            break;
        }
    }
    
    return len;
}

// Receive message
int sys_recv(void *buf, size_t len) {
    if (!current_task) {
        return -1;
    }
    
    // Check if we have messages
    if (queue_count[current_task->pid] == 0) {
        // Block current task
        current_task->state = TASK_BLOCKED;
        blocked_tasks[blocked_count++] = current_task;
        
        // Switch to another task
        pcb_t *next_task = dequeue_ready();
        if (next_task) {
            current_task = next_task;
            current_task->state = TASK_RUNNING;
            switch_to_asm(current_task);
        } else {
            // No other tasks, halt
            while (1) {
                __asm__("hlt");
            }
        }
    }
    
    // Get message from queue
    ipc_message_t *message = &message_queues[current_task->pid][queue_head[current_task->pid]];
    
    // Copy message data
    size_t copy_len = (len < message->size) ? len : message->size;
    for (size_t i = 0; i < copy_len; i++) {
        ((char*)buf)[i] = message->data[i];
    }
    
    // Remove from queue
    queue_head[current_task->pid] = (queue_head[current_task->pid] + 1) % MAX_IPC_MESSAGES;
    queue_count[current_task->pid]--;
    
    return copy_len;
}

// Get message count for task
int get_message_count(int pid) {
    return queue_count[pid];
}

// Clear message queue for task
void clear_message_queue(int pid) {
    queue_head[pid] = 0;
    queue_tail[pid] = 0;
    queue_count[pid] = 0;
}

// Send broadcast message to all tasks
int broadcast_message(void *msg, size_t len) {
    if (!current_task) {
        return -1;
    }
    
    int sent_count = 0;
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_ZOMBIE && tasks[i].pid != current_task->pid) {
            if (sys_send(tasks[i].pid, msg, len) > 0) {
                sent_count++;
            }
        }
    }
    
    return sent_count;
}

// Get sender PID of last received message
int get_last_sender_pid(void) {
    if (!current_task || queue_count[current_task->pid] == 0) {
        return -1;
    }
    
    int last_index = (queue_head[current_task->pid] - 1 + MAX_IPC_MESSAGES) % MAX_IPC_MESSAGES;
    return message_queues[current_task->pid][last_index].sender_pid;
}

// Check if task has pending messages
bool has_pending_messages(int pid) {
    return queue_count[pid] > 0;
}

// Get message size without removing from queue
int peek_message_size(void) {
    if (!current_task || queue_count[current_task->pid] == 0) {
        return -1;
    }
    
    return message_queues[current_task->pid][queue_head[current_task->pid]].size;
}

// IPC statistics
typedef struct {
    int messages_sent;
    int messages_received;
    int messages_dropped;
    int tasks_blocked;
} ipc_stats_t;

ipc_stats_t ipc_stats = {0, 0, 0, 0};

// Get IPC statistics
ipc_stats_t get_ipc_stats(void) {
    return ipc_stats;
}

// Reset IPC statistics
void reset_ipc_stats(void) {
    ipc_stats.messages_sent = 0;
    ipc_stats.messages_received = 0;
    ipc_stats.messages_dropped = 0;
    ipc_stats.tasks_blocked = 0;
} 