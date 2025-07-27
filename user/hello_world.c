// Simple user program for microkernel OS
// This program demonstrates basic user space functionality

#include <stdint.h>

// System call numbers
#define SYS_SEND 1
#define SYS_RECV 2
#define SYS_YIELD 3
#define SYS_EXIT 4

// Simple system call wrapper
static inline int64_t syscall(int64_t number, int64_t arg1, int64_t arg2, int64_t arg3) {
    int64_t result;
    __asm__ volatile(
        "syscall"
        : "=a" (result)
        : "a" (number), "D" (arg1), "S" (arg2), "d" (arg3)
        : "rcx", "r11", "memory"
    );
    return result;
}

// Simple print function (writes to video memory)
void print_string(const char *str) {
    volatile uint16_t *video = (volatile uint16_t*)0xB8000;
    static int x = 0, y = 0;
    
    while (*str) {
        if (*str == '\n') {
            x = 0;
            y++;
            if (y >= 25) y = 0;
        } else {
            video[y * 80 + x] = 0x0F00 | *str; // White on black
            x++;
            if (x >= 80) {
                x = 0;
                y++;
                if (y >= 25) y = 0;
            }
        }
        str++;
    }
}

// Simple number to string conversion
void print_number(int num) {
    char buf[32];
    int i = 0;
    
    if (num == 0) {
        buf[i++] = '0';
    } else {
        while (num > 0) {
            buf[i++] = '0' + (num % 10);
            num /= 10;
        }
    }
    
    // Reverse the string
    for (int j = 0; j < i / 2; j++) {
        char temp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = temp;
    }
    buf[i] = '\0';
    
    print_string(buf);
}

// Main function
void _start(void) {
    // Print hello message
    print_string("Hello from user task!\n");
    
    // Get our task ID (simplified)
    int task_id = 1; // This would normally come from kernel
    
    print_string("Task ID: ");
    print_number(task_id);
    print_string("\n");
    
    // Simple loop with yield
    for (int i = 0; i < 10; i++) {
        print_string("Task ");
        print_number(task_id);
        print_string(" iteration ");
        print_number(i);
        print_string("\n");
        
        // Yield to other tasks
        syscall(SYS_YIELD, 0, 0, 0);
    }
    
    // Exit
    print_string("Task ");
    print_number(task_id);
    print_string(" completed!\n");
    syscall(SYS_EXIT, 0, 0, 0);
    
    // Should never reach here
    while (1) {
        __asm__("hlt");
    }
}

// IPC test program
void ipc_test(void) {
    print_string("IPC Test Program\n");
    
    // Send a message
    char msg[] = "Hello from IPC test!";
    int result = syscall(SYS_SEND, 2, (int64_t)msg, sizeof(msg));
    
    if (result > 0) {
        print_string("Message sent successfully\n");
    } else {
        print_string("Failed to send message\n");
    }
    
    // Try to receive a message
    char buf[256];
    result = syscall(SYS_RECV, (int64_t)buf, sizeof(buf), 0);
    
    if (result > 0) {
        print_string("Received message: ");
        print_string(buf);
        print_string("\n");
    } else {
        print_string("No message received\n");
    }
    
    syscall(SYS_EXIT, 0, 0, 0);
}

// Page fault test program
void page_test(void) {
    print_string("Page Fault Test Program\n");
    
    // Try to access memory that will cause page faults
    volatile char *ptr = (volatile char*)0x500000;
    
    for (int i = 0; i < 10; i++) {
        print_string("Accessing page ");
        print_number(i);
        print_string("\n");
        
        // This will trigger page faults
        ptr[i * 4096] = 'X';
        
        syscall(SYS_YIELD, 0, 0, 0);
    }
    
    print_string("Page fault test completed\n");
    syscall(SYS_EXIT, 0, 0, 0);
} 