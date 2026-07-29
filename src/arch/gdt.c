#include "gdt.h"

static struct gdt_entry gdt[6];
static struct gdt_ptr   gdtp;

static void gdt_set_gate(int n, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran) {
    gdt[n].base_low   = base & 0xFFFF;
    gdt[n].base_mid   = (base >> 16) & 0xFF;
    gdt[n].base_high  = (base >> 24) & 0xFF;
    gdt[n].limit_low  = limit & 0xFFFF;
    gdt[n].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[n].access     = access;
}

void gdt_init() {
    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base  = (uint32_t)&gdt;

    gdt_set_gate(0, 0, 0,          0x00, 0x00);
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    __asm__ volatile(
        "lgdt %0\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "ljmp $0x08, $1f\n\t"
        "1:\n\t"
        : : "m"(gdtp) : "eax"
    );
}
