#include "kernel.h"

// Global variables
pcb_t *current_task = NULL;
pcb_t tasks[MAX_TASKS];
int next_pid = 1;

// Simple video memory for output
volatile uint16_t *video_memory = (volatile uint16_t*)0xB8000;
int cursor_x = 0;
int cursor_y = 0;

// Kernel main function
void kernel_main(void) {
    // Clear screen
    for (int i = 0; i < 80 * 25; i++) {
        video_memory[i] = 0x0720; // Black background, white text, space
    }
    
    kprintf("Microkernel OS Starting...\n");
    
    // Initialize subsystems
    gdt_init();
    kprintf("GDT initialized\n");
    
    idt_init();
    kprintf("IDT initialized\n");
    
    paging_init();
    kprintf("Paging initialized\n");
    
    vm_init();
    kprintf("Virtual memory initialized\n");
    
    timer_init();
    kprintf("Timer initialized\n");
    
    scheduler_init();
    kprintf("Scheduler initialized\n");
    
    ipc_init();
    kprintf("IPC initialized\n");
    
    // Create initial user tasks
    create_task("hello_world", 0x400000, 1);
    create_task("hello_world", 0x410000, 1);
    create_task("hello_world", 0x420000, 1);
    create_task("hello_world", 0x430000, 1);
    create_task("hello_world", 0x440000, 1);
    create_task("hello_world", 0x450000, 1);
    create_task("hello_world", 0x460000, 1);
    create_task("hello_world", 0x470000, 1);
    
    kprintf("Created 8 user tasks\n");
    kprintf("Enabling interrupts...\n");
    
    enable_interrupts();
    
    kprintf("Kernel initialization complete!\n");
    kprintf("Starting scheduler...\n");
    
    // Main kernel loop
    while (1) {
        schedule();
        __asm__("hlt");
    }
}

// GDT initialization
void gdt_init(void) {
    // Clear GDT
    for (int i = 0; i < 8; i++) {
        gdt_set_entry(i, 0, 0, 0, 0);
    }
    
    // Null descriptor
    gdt_set_entry(0, 0, 0, 0, 0);
    
    // Kernel code descriptor
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);
    
    // Kernel data descriptor
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);
    
    // User code descriptor
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);
    
    // User data descriptor
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);
    
    // Load GDT
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdt_ptr;
    
    gdt_ptr.limit = 8 * 8 - 1;
    gdt_ptr.base = (uint64_t)gdt_entries;
    
    __asm__ volatile("lgdt %0" : : "m"(gdt_ptr));
}

// GDT entry setting
void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    gdt_entries[index].base_low = base & 0xFFFF;
    gdt_entries[index].base_mid = (base >> 16) & 0xFF;
    gdt_entries[index].base_high = (base >> 24) & 0xFF;
    gdt_entries[index].limit_low = limit & 0xFFFF;
    gdt_entries[index].granularity = (limit >> 16) & 0x0F;
    gdt_entries[index].granularity |= granularity & 0xF0;
    gdt_entries[index].access = access;
}

// IDT initialization
void idt_init(void) {
    // Clear IDT
    for (int i = 0; i < 256; i++) {
        idt_set_entry(i, (uint64_t)default_interrupt_handler, 0x08, 0x8E);
    }
    
    // Set up specific interrupt handlers
    idt_set_entry(0x20, (uint64_t)timer_handler, 0x08, 0x8E); // Timer
    idt_set_entry(0x80, (uint64_t)syscall_handler, 0x08, 0xEE); // System call
    idt_set_entry(0x0E, (uint64_t)page_fault_handler, 0x08, 0x8E); // Page fault
    
    // Load IDT
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idt_ptr;
    
    idt_ptr.limit = 256 * 16 - 1;
    idt_ptr.base = (uint64_t)idt_entries;
    
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}

// IDT entry setting
void idt_set_entry(int index, uint64_t handler, uint16_t selector, uint8_t flags) {
    idt_entries[index].offset_low = handler & 0xFFFF;
    idt_entries[index].offset_mid = (handler >> 16) & 0xFFFF;
    idt_entries[index].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt_entries[index].selector = selector;
    idt_entries[index].flags = flags;
    idt_entries[index].ist = 0;
    idt_entries[index].reserved = 0;
}

// Paging initialization
void paging_init(void) {
    // Clear page tables
    for (int i = 0; i < 4096; i++) {
        ((uint64_t*)0x1000)[i] = 0;
    }
    
    // Set up PML4
    ((uint64_t*)0x1000)[0] = 0x2000 | 3; // Present + Read/Write
    
    // Set up PDPT
    ((uint64_t*)0x2000)[0] = 0x3000 | 3; // Present + Read/Write
    
    // Set up PD (2MB pages)
    for (int i = 0; i < 512; i++) {
        ((uint64_t*)0x3000)[i] = (i * 0x200000) | 0x83; // Present + Read/Write + 2MB
    }
    
    // Load CR3
    write_cr3(0x1000);
}

// Timer initialization
void timer_init(void) {
    // Set up PIT (Programmable Interval Timer)
    uint32_t divisor = 1193180 / 100; // 100 Hz
    
    // Send command byte
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x36), "Nd"((uint16_t)0x43));
    
    // Send divisor
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)(divisor & 0xFF)), "Nd"((uint16_t)0x40));
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)(divisor >> 8)), "Nd"((uint16_t)0x40));
    
    // Enable IRQ0
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x21));
}

// Scheduler initialization
void scheduler_init(void) {
    // Clear task array
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_ZOMBIE;
        tasks[i].pid = 0;
    }
    
    current_task = NULL;
}

// IPC initialization
void ipc_init(void) {
    // Initialize IPC message queues
    for (int i = 0; i < MAX_TASKS; i++) {
        // Initialize message queues for each task
    }
}

// Simple printf implementation
void kprintf(const char *format) {
    while (*format) {
        if (*format == '\n') {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= 25) {
                cursor_y = 0;
            }
        } else {
            video_memory[cursor_y * 80 + cursor_x] = 0x0700 | *format;
            cursor_x++;
            if (cursor_x >= 80) {
                cursor_x = 0;
                cursor_y++;
                if (cursor_y >= 25) {
                    cursor_y = 0;
                }
            }
        }
        format++;
    }
}

// Panic function
void panic(const char *message) {
    kprintf("KERNEL PANIC: ");
    kprintf(message);
    kprintf("\n");
    disable_interrupts();
    while (1) {
        __asm__("hlt");
    }
}

// Halt function
void halt(void) {
    __asm__("hlt");
}

// Default interrupt handler
void default_interrupt_handler(void) {
    kprintf("Unhandled interrupt\n");
}

// Global variables for GDT and IDT
gdt_entry_t gdt_entries[8];
idt_entry_t idt_entries[256]; 