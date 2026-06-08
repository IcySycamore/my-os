/*
 * vm.c — RISC-V Sv39 Page Tables for my-os v2 (Lab 3: pgtbl)
 *
 * Implements: walk(), mappages(), kvminit(), vmprint()
 * Uses identity mapping (VA == PA). M-mode only, so no PTE_U.
 */

void uart_puts(const char *s);
void uart_puthex(unsigned long x);

#define PGSIZE  4096
#define PGSHIFT 12
#define PX(lv, va) (((va) >> (12 + (lv)*9)) & 0x1FF)

// PTE flags (RISC-V privileged spec)
#define PTE_V (1UL<<0)
#define PTE_R (1UL<<1)
#define PTE_W (1UL<<2)
#define PTE_X (1UL<<3)
#define PTE_U (1UL<<4)
#define PTE_G (1UL<<5)
#define PTE_A (1UL<<6)
#define PTE_D (1UL<<7)

#define PTE2PA(pte) (((pte)>>10)<<12)
#define PA2PTE(pa)  ((((unsigned long)(pa))>>12)<<10)

#define SATP_SV39 (8UL<<60)
#define MAKE_SATP(pt) (SATP_SV39 | (((unsigned long)(pt))>>12))

static inline void w_satp(unsigned long x)  { asm("csrw satp, %0" ::"r"(x)); }
static inline void sfence_vma(void)          { asm("sfence.vma zero, zero"); }

// ---- Bump allocator (physical pages) ----
extern char __end[];
static unsigned long alloc_ptr;

void kalloc_init(void)
{
    alloc_ptr = (unsigned long)__end;
    alloc_ptr = (alloc_ptr + PGSIZE - 1) & ~(PGSIZE - 1);
}

void *kalloc_page(void)
{
    unsigned long p = alloc_ptr;
    alloc_ptr += PGSIZE;
    for (unsigned long *x = (unsigned long *)p;
         (unsigned long)x < alloc_ptr; x++) *x = 0;
    return (void *)p;
}

// ---- Sv39 Page Table (Lab 3) ----
unsigned long *walk(unsigned long *pt, unsigned long va, int alloc)
{
    for (int lv = 2; lv > 0; lv--) {
        unsigned long *pte = &pt[PX(lv, va)];
        if (*pte & PTE_V)
            pt = (unsigned long *)PTE2PA(*pte);
        else {
            if (!alloc) return 0;
            pt = (unsigned long *)kalloc_page();
            *pte = PA2PTE((unsigned long)pt) | PTE_V;
        }
    }
    return &pt[PX(0, va)];
}

int mappages(unsigned long *pt, unsigned long va, unsigned long sz,
             unsigned long pa, unsigned long perm)
{
    for (unsigned long a = va; a < va + sz; a += PGSIZE, pa += PGSIZE) {
        unsigned long *pte = walk(pt, a, 1);
        if (!pte) return -1;
        *pte = PA2PTE(pa) | perm | PTE_V;
    }
    return 0;
}

// ---- Init (identity-map kernel + devices) ----
static unsigned long *kernel_pagetable;

void kvminit(void)
{
    kernel_pagetable = (unsigned long *)kalloc_page();

    // Identity-map 4MB for kernel code/data/stack/BSS
    mappages(kernel_pagetable,
             0x80000000UL, 4*1024*1024, 0x80000000UL,
             PTE_R | PTE_W | PTE_X);

    // Identity-map UART + VirtIO MMIO region (0x10000000-0x1001FFFF, 128KB)
    mappages(kernel_pagetable,
             0x10000000UL, 32*PGSIZE, 0x10000000UL, PTE_R | PTE_W);

    // Identity-map CLINT
    mappages(kernel_pagetable,
             0x02000000UL, PGSIZE, 0x02000000UL, PTE_R | PTE_W);

    // Enable paging
    w_satp(MAKE_SATP(kernel_pagetable));
    sfence_vma();
}

// ---- vmprint (Lab 3) ----
void vmprint(void)
{
    uart_puts("\n--- Kernel Page Table (Sv39) ---\n");
    uart_puts("(printing root entries only)\n");
    uart_puts("root @ ");
    uart_puthex((unsigned long)kernel_pagetable);
    uart_puts("\n");
    // Only print top-level entries (not full recursive dump)
    for (int i = 0; i < 512; i++) {
        unsigned long pte = kernel_pagetable[i];
        if (!(pte & PTE_V)) continue;
        uart_puts("L2[");
        uart_puthex(i);
        uart_puts("] pte=");
        uart_puthex(pte);
        int leaf = (pte & PTE_R) || (pte & PTE_W) || (pte & PTE_X);
        if (leaf)
            uart_puts(" (leaf!)\n");
        else {
            uart_puts(" ->\n");
            // Print one level deeper
            unsigned long *l1 = (unsigned long *)PTE2PA(pte);
            int count = 0;
            for (int j = 0; j < 512 && count < 5; j++) {
                if (!(l1[j] & PTE_V)) continue;
                uart_puts("  L1[");
                uart_puthex(j);
                uart_puts("] pte=");
                uart_puthex(l1[j]);
                uart_puts(" pa=");
                uart_puthex(PTE2PA(l1[j]));
                uart_puts("\n");
                count++;
            }
        }
    }
    uart_puts("--- End ---\n\n");
}
