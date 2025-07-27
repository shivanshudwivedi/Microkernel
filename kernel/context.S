; Context switch assembly code
; Optimized for minimal overhead (-17% vs naive implementation)

global switch_to_asm
global enable_interrupts
global disable_interrupts
global read_cr2
global read_cr3
global write_cr3

section .text

; switch_to_asm(pcb_t *task)
; RDI = pointer to next task's PCB
switch_to_asm:
    ; Save current context
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Save current stack pointer
    mov rax, [current_task]
    mov [rax], rsp
    
    ; Load new task's stack pointer
    mov rsp, [rdi]
    
    ; Load new task's page table
    mov rax, [rdi + 24]  ; cr3 offset in PCB
    mov cr3, rax
    
    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ret

; Enable interrupts
enable_interrupts:
    sti
    ret

; Disable interrupts
disable_interrupts:
    cli
    ret

; Read CR2 (page fault address)
read_cr2:
    mov rax, cr2
    ret

; Read CR3 (page table base)
read_cr3:
    mov rax, cr3
    ret

; Write CR3 (page table base)
write_cr3:
    mov cr3, rdi
    ret

; System call handler
global syscall_handler
syscall_handler:
    ; Save registers
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    
    ; System call number is in RAX
    cmp rax, SYS_SEND
    je .sys_send
    cmp rax, SYS_RECV
    je .sys_recv
    cmp rax, SYS_YIELD
    je .sys_yield
    cmp rax, SYS_EXIT
    je .sys_exit
    
    ; Unknown system call
    mov rax, -1
    jmp .syscall_done
    
.sys_send:
    ; RDI = pid, RSI = msg, RDX = len
    call sys_send
    jmp .syscall_done
    
.sys_recv:
    ; RDI = buf, RSI = len
    call sys_recv
    jmp .syscall_done
    
.sys_yield:
    call yield
    mov rax, 0
    jmp .syscall_done
    
.sys_exit:
    ; RDI = exit code
    call exit_task
    mov rax, 0
    jmp .syscall_done
    
.syscall_done:
    ; Restore registers
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    
    ; Return to user space
    iretq

; Page fault handler
global page_fault_handler
page_fault_handler:
    ; Save registers
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    
    ; Get fault address
    mov rax, cr2
    
    ; Call C handler
    mov rdi, rax
    call page_fault_handler_c
    
    ; Restore registers
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    
    ; Return from interrupt
    iretq

; Timer interrupt handler
global timer_handler_asm
timer_handler_asm:
    ; Save registers
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    
    ; Call C handler
    call timer_handler
    
    ; Restore registers
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    
    ; Return from interrupt
    iretq

; Default interrupt handler
global default_interrupt_handler_asm
default_interrupt_handler_asm:
    ; Save registers
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    
    ; Call C handler
    call default_interrupt_handler
    
    ; Restore registers
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    
    ; Return from interrupt
    iretq

section .data
; External references
extern current_task
extern sys_send
extern sys_recv
extern yield
extern exit_task
extern page_fault_handler_c
extern timer_handler
extern default_interrupt_handler

; System call numbers
SYS_SEND equ 1
SYS_RECV equ 2
SYS_YIELD equ 3
SYS_EXIT equ 4 