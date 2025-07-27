#include "kernel.h"

// Page frame management
#define MAX_PHYSICAL_PAGES 1024
#define PAGE_FRAME_SIZE 4096

typedef struct {
    uint64_t virtual_addr;
    uint64_t physical_addr;
    bool dirty;
    bool accessed;
    uint64_t last_access;
} page_frame_t;

page_frame_t page_frames[MAX_PHYSICAL_PAGES];
int page_frame_count = 0;
uint64_t next_physical_addr = 0x1000000; // Start after kernel

// LRU list for page replacement
int lru_list[MAX_PHYSICAL_PAGES];
int lru_head = 0;
int lru_tail = 0;

// Initialize virtual memory
void vm_init(void) {
    // Clear page frames
    for (int i = 0; i < MAX_PHYSICAL_PAGES; i++) {
        page_frames[i].virtual_addr = 0;
        page_frames[i].physical_addr = 0;
        page_frames[i].dirty = false;
        page_frames[i].accessed = false;
        page_frames[i].last_access = 0;
    }
    
    // Initialize LRU list
    for (int i = 0; i < MAX_PHYSICAL_PAGES; i++) {
        lru_list[i] = i;
    }
    lru_head = 0;
    lru_tail = MAX_PHYSICAL_PAGES - 1;
    
    page_frame_count = 0;
}

// Page fault handler
void page_fault_handler_c(uint64_t fault_addr) {
    // Check if it's a valid user address
    if (fault_addr < USER_BASE || fault_addr >= USER_STACK_TOP) {
        panic("Page fault at invalid address");
    }
    
    // Try to allocate page
    if (allocate_page(fault_addr) < 0) {
        // No free pages, evict LRU page
        evict_lru_page();
        allocate_page(fault_addr);
    }
}

// Allocate page for virtual address
int allocate_page(uint64_t virtual_addr) {
    // Check if page is already mapped
    for (int i = 0; i < page_frame_count; i++) {
        if (page_frames[i].virtual_addr == virtual_addr) {
            // Update access time
            page_frames[i].accessed = true;
            page_frames[i].last_access = get_timestamp();
            return 0;
        }
    }
    
    // Check if we have free pages
    if (page_frame_count >= MAX_PHYSICAL_PAGES) {
        return -1; // No free pages
    }
    
    // Allocate new page frame
    uint64_t physical_addr = next_physical_addr;
    next_physical_addr += PAGE_FRAME_SIZE;
    
    // Clear the page
    for (int i = 0; i < PAGE_FRAME_SIZE / 8; i++) {
        ((uint64_t*)physical_addr)[i] = 0;
    }
    
    // Add to page frames
    page_frames[page_frame_count].virtual_addr = virtual_addr;
    page_frames[page_frame_count].physical_addr = physical_addr;
    page_frames[page_frame_count].dirty = false;
    page_frames[page_frame_count].accessed = true;
    page_frames[page_frame_count].last_access = get_timestamp();
    
    // Map the page
    map_page(virtual_addr, physical_addr, true, true);
    
    page_frame_count++;
    return 0;
}

// Evict least recently used page
void evict_lru_page(void) {
    if (page_frame_count == 0) {
        return;
    }
    
    // Find least recently used page
    int lru_index = -1;
    uint64_t oldest_access = 0xFFFFFFFFFFFFFFFF;
    
    for (int i = 0; i < page_frame_count; i++) {
        if (page_frames[i].last_access < oldest_access) {
            oldest_access = page_frames[i].last_access;
            lru_index = i;
        }
    }
    
    if (lru_index == -1) {
        return;
    }
    
    // If page is dirty, write it back (simplified)
    if (page_frames[lru_index].dirty) {
        // In a real system, this would write to disk
        // For this prototype, we just mark it as clean
        page_frames[lru_index].dirty = false;
    }
    
    // Unmap the page
    unmap_page(page_frames[lru_index].virtual_addr);
    
    // Remove from page frames
    for (int i = lru_index; i < page_frame_count - 1; i++) {
        page_frames[i] = page_frames[i + 1];
    }
    page_frame_count--;
}

// Map virtual address to physical address
void map_page(uint64_t virtual_addr, uint64_t physical_addr, bool user, bool writable) {
    // Calculate page table indices
    uint64_t pml4_index = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdp_index = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_addr >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_addr >> 12) & 0x1FF;
    
    // Get page table base
    uint64_t pml4_base = read_cr3();
    
    // Ensure PML4 entry exists
    uint64_t *pml4 = (uint64_t*)pml4_base;
    if (!(pml4[pml4_index] & 1)) {
        // Allocate PDPT
        uint64_t pdpt_addr = next_physical_addr;
        next_physical_addr += 4096;
        
        // Clear PDPT
        for (int i = 0; i < 512; i++) {
            ((uint64_t*)pdpt_addr)[i] = 0;
        }
        
        pml4[pml4_index] = pdpt_addr | 3; // Present + Read/Write
    }
    
    // Get PDPT
    uint64_t *pdpt = (uint64_t*)(pml4[pml4_index] & ~0xFFF);
    
    // Ensure PDP entry exists
    if (!(pdpt[pdp_index] & 1)) {
        // Allocate PD
        uint64_t pd_addr = next_physical_addr;
        next_physical_addr += 4096;
        
        // Clear PD
        for (int i = 0; i < 512; i++) {
            ((uint64_t*)pd_addr)[i] = 0;
        }
        
        pdpt[pdp_index] = pd_addr | 3; // Present + Read/Write
    }
    
    // Get PD
    uint64_t *pd = (uint64_t*)(pdpt[pdp_index] & ~0xFFF);
    
    // Set up page table entry
    uint64_t flags = 1; // Present
    if (writable) flags |= 2; // Read/Write
    if (user) flags |= 4; // User
    if (!user) flags |= 0x10; // Global
    
    pd[pd_index] = physical_addr | flags;
    
    // Reload page tables
    write_cr3(read_cr3());
}

// Unmap virtual address
void unmap_page(uint64_t virtual_addr) {
    // Calculate page table indices
    uint64_t pml4_index = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdp_index = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_addr >> 21) & 0x1FF;
    
    // Get page table base
    uint64_t pml4_base = read_cr3();
    uint64_t *pml4 = (uint64_t*)pml4_base;
    
    if (!(pml4[pml4_index] & 1)) {
        return; // Not mapped
    }
    
    uint64_t *pdpt = (uint64_t*)(pml4[pml4_index] & ~0xFFF);
    if (!(pdpt[pdp_index] & 1)) {
        return; // Not mapped
    }
    
    uint64_t *pd = (uint64_t*)(pdpt[pdp_index] & ~0xFFF);
    pd[pd_index] = 0; // Clear entry
    
    // Reload page tables
    write_cr3(read_cr3());
}

// Get physical address for virtual address
uint64_t get_physical_address(uint64_t virtual_addr) {
    // Calculate page table indices
    uint64_t pml4_index = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdp_index = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_addr >> 21) & 0x1FF;
    
    // Get page table base
    uint64_t pml4_base = read_cr3();
    uint64_t *pml4 = (uint64_t*)pml4_base;
    
    if (!(pml4[pml4_index] & 1)) {
        return 0; // Not mapped
    }
    
    uint64_t *pdpt = (uint64_t*)(pml4[pml4_index] & ~0xFFF);
    if (!(pdpt[pdp_index] & 1)) {
        return 0; // Not mapped
    }
    
    uint64_t *pd = (uint64_t*)(pdpt[pdp_index] & ~0xFFF);
    if (!(pd[pd_index] & 1)) {
        return 0; // Not mapped
    }
    
    return (pd[pd_index] & ~0xFFF) | (virtual_addr & 0x1FFFFF);
}

// Get timestamp for LRU
uint64_t get_timestamp(void) {
    // Simple timestamp implementation
    static uint64_t timestamp = 0;
    return ++timestamp;
}

// Mark page as dirty
void mark_page_dirty(uint64_t virtual_addr) {
    for (int i = 0; i < page_frame_count; i++) {
        if (page_frames[i].virtual_addr == virtual_addr) {
            page_frames[i].dirty = true;
            break;
        }
    }
}

// Get memory statistics
typedef struct {
    int total_pages;
    int used_pages;
    int free_pages;
    int page_faults;
    int page_evictions;
} vm_stats_t;

vm_stats_t vm_stats = {0, 0, 0, 0, 0};

vm_stats_t get_vm_stats(void) {
    vm_stats.total_pages = MAX_PHYSICAL_PAGES;
    vm_stats.used_pages = page_frame_count;
    vm_stats.free_pages = MAX_PHYSICAL_PAGES - page_frame_count;
    return vm_stats;
} 