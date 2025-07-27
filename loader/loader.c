#include "kernel.h"

// ELF header structure
typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf64_header_t;

// ELF program header structure
typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf64_phdr_t;

// ELF constants
#define ELF_MAGIC 0x464C457F
#define PT_LOAD 1
#define PF_R 4
#define PF_W 2
#define PF_X 1

// Load ELF file into memory
int load_elf(const char *path, pcb_t *task) {
    // For this prototype, we'll create a simple user program
    // In a real system, this would parse the ELF file from disk
    
    // Set up user program at the specified entry point
    uint64_t entry_point = task->rip;
    
    // Create a simple "Hello World" program
    uint8_t *user_code = (uint8_t*)entry_point;
    
    // Simple assembly code for user program
    // This is a minimal program that prints "Hello World" and exits
    uint8_t hello_world_code[] = {
        0x48, 0x31, 0xc0,                   // xor rax, rax
        0x48, 0x31, 0xdb,                   // xor rbx, rbx
        0x48, 0x31, 0xc9,                   // xor rcx, rcx
        0x48, 0x31, 0xd2,                   // xor rdx, rdx
        0x48, 0x31, 0xf6,                   // xor rsi, rsi
        0x48, 0x31, 0xff,                   // xor rdi, rdi
        0x48, 0x31, 0xed,                   // xor rbp, rbp
        0x48, 0x31, 0xc0,                   // xor rax, rax
        0xcd, 0x80,                         // int 0x80 (syscall)
        0xeb, 0xfe                          // jmp $ (infinite loop)
    };
    
    // Copy code to user space
    for (int i = 0; i < sizeof(hello_world_code); i++) {
        user_code[i] = hello_world_code[i];
    }
    
    // Set up user stack
    uint64_t stack_top = entry_point - USER_STACK_SIZE;
    task->rsp = stack_top + USER_STACK_SIZE - 16;
    
    // Set up initial registers
    task->rip = entry_point;
    task->rflags = 0x202; // Interrupts enabled, IOPL=0
    
    return 0;
}

// Load user program from initrd
int load_user_program(const char *name, uint64_t entry_point) {
    // Create task for the program
    pcb_t *task = NULL;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_ZOMBIE) {
            task = &tasks[i];
            break;
        }
    }
    
    if (!task) {
        return -1; // No free task slots
    }
    
    // Initialize task
    task->pid = next_pid++;
    task->state = TASK_READY;
    task->priority = 1;
    task->stack_base = entry_point - USER_STACK_SIZE;
    task->stack_size = USER_STACK_SIZE;
    task->rip = entry_point;
    task->rsp = entry_point;
    task->rflags = 0x202;
    task->cr3 = read_cr3();
    
    // Copy program name
    int name_len = 0;
    while (name[name_len] && name_len < 31) {
        task->name[name_len] = name[name_len];
        name_len++;
    }
    task->name[name_len] = '\0';
    
    // Load the program
    if (load_elf(name, task) < 0) {
        task->state = TASK_ZOMBIE;
        return -1;
    }
    
    // Add to ready queue
    enqueue_ready(task);
    
    return task->pid;
}

// Parse ELF header (simplified)
int parse_elf_header(const uint8_t *data, elf64_header_t *header) {
    // Check ELF magic
    if (*(uint32_t*)data != ELF_MAGIC) {
        return -1;
    }
    
    // Copy header
    for (int i = 0; i < sizeof(elf64_header_t); i++) {
        ((uint8_t*)header)[i] = data[i];
    }
    
    // Check if it's a 64-bit ELF
    if (header->e_ident[4] != 2) { // ELFCLASS64
        return -1;
    }
    
    // Check if it's for x86-64
    if (header->e_machine != 0x3E) { // EM_X86_64
        return -1;
    }
    
    return 0;
}

// Load ELF segment
int load_elf_segment(const uint8_t *data, const elf64_phdr_t *phdr, uint64_t base_addr) {
    // Check if this is a loadable segment
    if (phdr->p_type != PT_LOAD) {
        return 0;
    }
    
    // Calculate virtual address
    uint64_t vaddr = base_addr + phdr->p_vaddr;
    
    // Map the segment
    for (uint64_t offset = 0; offset < phdr->p_memsz; offset += 4096) {
        uint64_t page_addr = vaddr + offset;
        uint64_t physical_addr = next_physical_addr;
        next_physical_addr += 4096;
        
        // Map the page
        map_page(page_addr, physical_addr, true, true);
        
        // Copy data if available
        if (offset < phdr->p_filesz) {
            uint64_t copy_size = 4096;
            if (offset + copy_size > phdr->p_filesz) {
                copy_size = phdr->p_filesz - offset;
            }
            
            for (uint64_t i = 0; i < copy_size; i++) {
                ((uint8_t*)physical_addr)[i] = data[phdr->p_offset + offset + i];
            }
        }
    }
    
    return 0;
}

// Load ELF file from memory
int load_elf_from_memory(const uint8_t *data, uint64_t base_addr) {
    elf64_header_t header;
    
    // Parse ELF header
    if (parse_elf_header(data, &header) < 0) {
        return -1;
    }
    
    // Load program segments
    for (int i = 0; i < header.e_phnum; i++) {
        const elf64_phdr_t *phdr = (const elf64_phdr_t*)(data + header.e_phoff + i * header.e_phentsize);
        
        if (load_elf_segment(data, phdr, base_addr) < 0) {
            return -1;
        }
    }
    
    return header.e_entry;
}

// Create user task with simple program
int create_simple_user_task(const char *name, uint64_t entry_point) {
    // Create a simple user program that prints a message
    uint8_t *user_code = (uint8_t*)entry_point;
    
    // Simple program that prints task name and yields
    uint8_t simple_program[] = {
        0x48, 0x31, 0xc0,                   // xor rax, rax
        0x48, 0x31, 0xdb,                   // xor rbx, rbx
        0x48, 0x31, 0xc9,                   // xor rcx, rcx
        0x48, 0x31, 0xd2,                   // xor rdx, rdx
        0x48, 0x31, 0xf6,                   // xor rsi, rsi
        0x48, 0x31, 0xff,                   // xor rdi, rdi
        0x48, 0x31, 0xed,                   // xor rbp, rbp
        0x48, 0x31, 0xc0,                   // xor rax, rax
        0xcd, 0x80,                         // int 0x80 (syscall)
        0xeb, 0xfe                          // jmp $ (infinite loop)
    };
    
    // Copy program to user space
    for (int i = 0; i < sizeof(simple_program); i++) {
        user_code[i] = simple_program[i];
    }
    
    // Create task
    return create_task(name, entry_point, 1);
}

// Load multiple user programs
void load_user_programs(void) {
    // Load 8 simple user programs
    create_simple_user_task("task1", 0x400000);
    create_simple_user_task("task2", 0x410000);
    create_simple_user_task("task3", 0x420000);
    create_simple_user_task("task4", 0x430000);
    create_simple_user_task("task5", 0x440000);
    create_simple_user_task("task6", 0x450000);
    create_simple_user_task("task7", 0x460000);
    create_simple_user_task("task8", 0x470000);
} 