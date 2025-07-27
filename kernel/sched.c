#include "kernel.h"

// Ready queue for round-robin scheduling
pcb_t *ready_queue[MAX_TASKS];
int ready_queue_head = 0;
int ready_queue_tail = 0;
int ready_queue_count = 0;

// Task creation
int create_task(const char *name, uint64_t entry_point, int priority) {
    // Find free task slot
    int task_id = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_ZOMBIE) {
            task_id = i;
            break;
        }
    }
    
    if (task_id == -1) {
        return -1; // No free slots
    }
    
    // Initialize task
    pcb_t *task = &tasks[task_id];
    task->pid = next_pid++;
    task->state = TASK_READY;
    task->priority = priority;
    task->stack_base = entry_point - USER_STACK_SIZE;
    task->stack_size = USER_STACK_SIZE;
    task->rip = entry_point;
    task->rsp = entry_point;
    task->rflags = 0x202; // Interrupts enabled, IOPL=0
    task->cr3 = read_cr3(); // Use kernel page tables for now
    
    // Copy task name
    int name_len = 0;
    while (name[name_len] && name_len < 31) {
        task->name[name_len] = name[name_len];
        name_len++;
    }
    task->name[name_len] = '\0';
    
    // Set up initial stack
    uint64_t *stack = (uint64_t*)task->rsp;
    stack[-1] = entry_point; // Return address
    stack[-2] = 0x202;       // RFLAGS
    stack[-3] = 0x18;        // CS (user code segment)
    stack[-4] = 0x20;        // SS (user data segment)
    stack[-5] = 0;           // R15
    stack[-6] = 0;           // R14
    stack[-7] = 0;           // R13
    stack[-8] = 0;           // R12
    stack[-9] = 0;           // R11
    stack[-10] = 0;          // R10
    stack[-11] = 0;          // R9
    stack[-12] = 0;          // R8
    stack[-13] = 0;          // RDI
    stack[-14] = 0;          // RSI
    stack[-15] = 0;          // RBP
    stack[-16] = 0;          // RDX
    stack[-17] = 0;          // RCX
    stack[-18] = 0;          // RBX
    stack[-19] = 0;          // RAX
    
    task->rsp = (uint64_t)&stack[-19];
    
    // Add to ready queue
    enqueue_ready(task);
    
    return task->pid;
}

// Enqueue task to ready queue
void enqueue_ready(pcb_t *task) {
    if (ready_queue_count < MAX_TASKS) {
        ready_queue[ready_queue_tail] = task;
        ready_queue_tail = (ready_queue_tail + 1) % MAX_TASKS;
        ready_queue_count++;
    }
}

// Dequeue task from ready queue
pcb_t *dequeue_ready(void) {
    if (ready_queue_count > 0) {
        pcb_t *task = ready_queue[ready_queue_head];
        ready_queue_head = (ready_queue_head + 1) % MAX_TASKS;
        ready_queue_count--;
        return task;
    }
    return NULL;
}

// Enqueue current task back to ready queue
void enqueue_current(void) {
    if (current_task && current_task->state == TASK_RUNNING) {
        current_task->state = TASK_READY;
        enqueue_ready(current_task);
    }
}

// Main scheduler function
void schedule(void) {
    // If no current task, get one from ready queue
    if (!current_task) {
        current_task = dequeue_ready();
        if (!current_task) {
            return; // No tasks to run
        }
    }
    
    // Get next task from ready queue
    pcb_t *next_task = dequeue_ready();
    if (!next_task) {
        // No other tasks, keep current task
        return;
    }
    
    // Put current task back in ready queue
    enqueue_current();
    
    // Switch to next task
    pcb_t *prev_task = current_task;
    current_task = next_task;
    current_task->state = TASK_RUNNING;
    
    // Perform context switch
    switch_to_asm(current_task);
}

// Context switch wrapper
void switch_to(pcb_t *task) {
    if (current_task) {
        current_task->state = TASK_READY;
    }
    current_task = task;
    task->state = TASK_RUNNING;
    switch_to_asm(task);
}

// Yield current task
void yield(void) {
    if (current_task) {
        current_task->state = TASK_READY;
        enqueue_ready(current_task);
    }
    
    pcb_t *next_task = dequeue_ready();
    if (next_task) {
        current_task = next_task;
        current_task->state = TASK_RUNNING;
        switch_to_asm(current_task);
    }
}

// Exit current task
void exit_task(int exit_code) {
    if (current_task) {
        current_task->state = TASK_ZOMBIE;
        kprintf("Task exited\n");
        
        // Get next task
        pcb_t *next_task = dequeue_ready();
        if (next_task) {
            current_task = next_task;
            current_task->state = TASK_RUNNING;
            switch_to_asm(current_task);
        } else {
            current_task = NULL;
            // No more tasks, halt
            kprintf("No more tasks to run, halting...\n");
            while (1) {
                __asm__("hlt");
            }
        }
    }
}

// Timer handler for preemption
void timer_handler(void) {
    // Send EOI to PIC
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));
    
    // Trigger scheduling
    if (current_task) {
        yield();
    }
}

// Get current task
pcb_t *get_current_task(void) {
    return current_task;
}

// Get task by PID
pcb_t *get_task_by_pid(int pid) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].pid == pid && tasks[i].state != TASK_ZOMBIE) {
            return &tasks[i];
        }
    }
    return NULL;
}

// Block current task
void block_task(void) {
    if (current_task) {
        current_task->state = TASK_BLOCKED;
        
        // Get next task
        pcb_t *next_task = dequeue_ready();
        if (next_task) {
            current_task = next_task;
            current_task->state = TASK_RUNNING;
            switch_to_asm(current_task);
        } else {
            current_task = NULL;
            // No more tasks, halt
            while (1) {
                __asm__("hlt");
            }
        }
    }
}

// Unblock task
void unblock_task(pcb_t *task) {
    if (task && task->state == TASK_BLOCKED) {
        task->state = TASK_READY;
        enqueue_ready(task);
    }
} 