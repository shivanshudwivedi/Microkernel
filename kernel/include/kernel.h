#ifndef KERNEL_H
#define KERNEL_H

// Standard integer types
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long int64_t;

// Standard definitions
typedef unsigned long size_t;
typedef int bool;
#define true 1
#define false 0

// Kernel configuration
#define MAX_TASKS 8
#define MAX_IPC_MESSAGES 32
#define MAX_MESSAGE_SIZE 256
#define PAGE_SIZE 4096
#define KERNEL_STACK_SIZE 8192
#define USER_STACK_SIZE 16384

// Memory layout
#define KERNEL_BASE 0x100000
#define KERNEL_STACK_TOP 0x200000
#define USER_BASE 0x400000
#define USER_STACK_TOP 0x600000

// System call numbers
#define SYS_SEND 1
#define SYS_RECV 2
#define SYS_YIELD 3
#define SYS_EXIT 4

// Task states
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_ZOMBIE
} task_state_t;

// Task control block
typedef struct {
    uint64_t rsp;          // Stack pointer
    uint64_t rip;          // Instruction pointer
    uint64_t rflags;       // Flags register
    uint64_t cr3;          // Page table base
    task_state_t state;    // Current state
    int pid;               // Process ID
    int priority;          // Priority level
    uint64_t stack_base;   // Stack base address
    uint64_t stack_size;   // Stack size
    char name[32];         // Task name
} pcb_t;

// IPC message structure
typedef struct {
    int sender_pid;
    int receiver_pid;
    size_t size;
    char data[MAX_MESSAGE_SIZE];
} ipc_message_t;

// Page table entry
typedef struct {
    uint64_t present : 1;
    uint64_t read_write : 1;
    uint64_t user_supervisor : 1;
    uint64_t write_through : 1;
    uint64_t cache_disable : 1;
    uint64_t accessed : 1;
    uint64_t dirty : 1;
    uint64_t huge_page : 1;
    uint64_t global : 1;
    uint64_t available : 3;
    uint64_t address : 40;
    uint64_t available2 : 11;
    uint64_t no_execute : 1;
} __attribute__((packed)) page_table_entry_t;

// Interrupt descriptor table entry
typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

// Global descriptor table entry
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

// Function declarations

// Kernel main
void kernel_main(void);

// GDT functions
void gdt_init(void);
void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity);

// IDT functions
void idt_init(void);
void idt_set_entry(int index, uint64_t handler, uint16_t selector, uint8_t flags);

// Paging functions
void paging_init(void);
void map_page(uint64_t virtual_addr, uint64_t physical_addr, bool user, bool writable);
void unmap_page(uint64_t virtual_addr);
uint64_t get_physical_address(uint64_t virtual_addr);

// Scheduler functions
void scheduler_init(void);
void schedule(void);
void switch_to(pcb_t *task);
void yield(void);
int create_task(const char *name, uint64_t entry_point, int priority);
void exit_task(int exit_code);

// IPC functions
int sys_send(int pid, void *msg, size_t len);
int sys_recv(void *buf, size_t len);
void ipc_init(void);

// Virtual memory functions
void vm_init(void);
void page_fault_handler(void);
int allocate_page(uint64_t addr);
void evict_lru_page(void);

// Timer functions
void timer_init(void);
void timer_handler(void);

// System call handler
void syscall_handler(void);

// Utility functions
void kprintf(const char *format, ...);
void panic(const char *message);
void halt(void);

// Assembly functions
extern void switch_to_asm(pcb_t *task);
extern void enable_interrupts(void);
extern void disable_interrupts(void);
extern uint64_t read_cr2(void);
extern uint64_t read_cr3(void);
extern void write_cr3(uint64_t value);

// Global variables
extern pcb_t *current_task;
extern pcb_t tasks[MAX_TASKS];
extern int next_pid;

#endif // KERNEL_H 