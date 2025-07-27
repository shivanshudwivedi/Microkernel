; Microkernel OS Bootloader
; Switches from 16-bit real mode to 64-bit long mode
; Loads kernel at 0x100000

[bits 16]
[org 0x7c00]

; Boot sector entry point
start:
    cli                     ; Disable interrupts
    cld                     ; Clear direction flag
    
    ; Set up segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00         ; Set up stack
    
    ; Save boot drive number
    mov [boot_drive], dl
    
    ; Print boot message
    mov si, boot_msg
    call print_string
    
    ; Load kernel from disk
    mov ah, 0x02           ; BIOS read sectors
    mov al, 64             ; Number of sectors to read (32KB)
    mov ch, 0              ; Cylinder 0
    mov cl, 2              ; Sector 2 (sector 1 is boot sector)
    mov dh, 0              ; Head 0
    mov dl, [boot_drive]   ; Drive number
    mov bx, 0x1000         ; Load to 0x1000:0x0000
    int 0x13
    jc disk_error
    
    ; Load GDT
    lgdt [gdt_descriptor]
    
    ; Enable A20 line
    in al, 0x92
    or al, 2
    out 0x92, al
    
    ; Disable interrupts
    cli
    
    ; Enable protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Far jump to flush pipeline and switch to 32-bit
    jmp 0x08:protected_mode_entry

; Print string function (16-bit)
print_string:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0e
    int 0x10
    jmp print_string
.done:
    ret

; Disk error handler
disk_error:
    mov si, disk_error_msg
    call print_string
    jmp $

; 32-bit protected mode entry
[bits 32]
protected_mode_entry:
    ; Set up segment registers for 32-bit
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack
    mov esp, 0x90000
    
    ; Copy kernel to 0x100000
    mov esi, 0x1000
    mov edi, 0x100000
    mov ecx, 0x8000        ; 32KB
    rep movsd
    
    ; Set up page tables for long mode
    ; Clear page tables
    mov edi, 0x1000
    mov ecx, 0x1000
    xor eax, eax
    rep stosd
    
    ; Set up PML4
    mov eax, 0x2000        ; PDPT address
    or eax, 3              ; Present + Read/Write
    mov [0x1000], eax
    
    ; Set up PDPT
    mov eax, 0x3000        ; PD address
    or eax, 3              ; Present + Read/Write
    mov [0x2000], eax
    
    ; Set up PD (2MB pages)
    mov eax, 0x00000000    ; Physical address 0
    or eax, 0x83           ; Present + Read/Write + 2MB page
    mov [0x3000], eax
    
    mov eax, 0x00200000    ; Physical address 2MB
    or eax, 0x83           ; Present + Read/Write + 2MB page
    mov [0x3008], eax
    
    ; Load PML4 into CR3
    mov eax, 0x1000
    mov cr3, eax
    
    ; Enable PAE
    mov eax, cr4
    or eax, 0x20           ; PAE bit
    mov cr4, eax
    
    ; Enable long mode
    mov ecx, 0xC0000080    ; EFER MSR
    rdmsr
    or eax, 0x100          ; LME bit
    wrmsr
    
    ; Enable paging
    mov eax, cr0
    or eax, 0x80000000     ; PG bit
    mov cr0, eax
    
    ; Load 64-bit GDT
    lgdt [gdt64_descriptor]
    
    ; Far jump to 64-bit mode
    jmp 0x08:long_mode_entry

; 64-bit long mode entry
[bits 64]
long_mode_entry:
    ; Set up segment registers for 64-bit
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack
    mov rsp, 0x200000
    
    ; Clear BSS
    mov rdi, 0x100000
    mov rcx, 0x10000
    xor rax, rax
    rep stosb
    
    ; Jump to kernel
    call kernel_main
    
    ; Halt if kernel returns
    hlt

; Data section
boot_msg: db 'Microkernel OS Bootloader', 0x0d, 0x0a, 0
disk_error_msg: db 'Disk read error!', 0x0d, 0x0a, 0
boot_drive: db 0

; GDT for 32-bit protected mode
gdt:
    ; Null descriptor
    dq 0
    ; Code descriptor
    dw 0xffff      ; Limit 0:15
    dw 0x0000      ; Base 0:15
    db 0x00        ; Base 16:23
    db 10011010b   ; Access byte
    db 11001111b   ; Flags + Limit 16:19
    db 0x00        ; Base 24:31
    ; Data descriptor
    dw 0xffff      ; Limit 0:15
    dw 0x0000      ; Base 0:15
    db 0x00        ; Base 16:23
    db 10010010b   ; Access byte
    db 11001111b   ; Flags + Limit 16:19
    db 0x00        ; Base 24:31

gdt_descriptor:
    dw gdt_descriptor - gdt - 1
    dd gdt

; GDT for 64-bit long mode
gdt64:
    ; Null descriptor
    dq 0
    ; Code descriptor
    dw 0x0000      ; Limit (unused in long mode)
    dw 0x0000      ; Base (unused in long mode)
    db 0x00        ; Base (unused in long mode)
    db 10011010b   ; Access byte
    db 00100000b   ; Flags + Limit (unused in long mode)
    db 0x00        ; Base (unused in long mode)
    ; Data descriptor
    dw 0x0000      ; Limit (unused in long mode)
    dw 0x0000      ; Base (unused in long mode)
    db 0x00        ; Base (unused in long mode)
    db 10010010b   ; Access byte
    db 00000000b   ; Flags + Limit (unused in long mode)
    db 0x00        ; Base (unused in long mode)

gdt64_descriptor:
    dw gdt64_descriptor - gdt64 - 1
    dq gdt64

; Boot sector signature
times 510-($-$$) db 0
dw 0xAA55 