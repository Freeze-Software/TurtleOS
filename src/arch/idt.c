#include <stdint.h>
#include "../kernel.h"
#include "idt.h"

/* =========================
   IDT STRUCTURES
========================= */

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

/* =========================
   ASM FUNCTIONS
========================= */

extern void idt_load(void* idt_ptr);
extern void irq1_handler();
extern void timer_stub();
extern void irq12_mouse_stub();

/* =========================
   IDT SETUP
========================= */

void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;

    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

/* =========================
   DEFAULT INTERRUPT HANDLER
========================= */

typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) registers_t;

static const char *exception_messages[] = {
    "Division By Zero",                 // 0
    "Debug",                            // 1
    "Non Maskable Interrupt",           // 2
    "Breakpoint",                       // 3
    "Into Detected Overflow",           // 4
    "Out of Bounds",                    // 5
    "Invalid Opcode",                   // 6
    "No Coprocessor",                   // 7
    "Double Fault",                     // 8
    "Coprocessor Segment Overrun",      // 9
    "Bad TSS",                          // 10
    "Segment Not Present",              // 11
    "Stack Fault",                      // 12
    "General Protection Fault (#GP)",   // 13
    "Page Fault (#PF)",                 // 14
    "Unknown Interrupt",                // 15
    "Coprocessor Fault",                // 16
    "Alignment Check",                  // 17
    "Machine Check",                    // 18
    // 19-31 are reserved
};

void kernel_panic(registers_t *regs) {
    const char *reason;

    if (regs->int_no < 19) {
        reason = exception_messages[regs->int_no];
    } else if (regs->int_no < 32) {
        reason = "Reserved Exception";
    } else {
        reason = "User/Hardware Interrupt";
    }

    set_console_bg_color(0xFF0000);
    set_console_fg_color(0x000000);
    console_clear();
    console_writefln("\n================ KERNEL PANIC ================");
    console_writefln("Reason:   %s (INT %d / 0x%x)", reason, regs->int_no, regs->int_no);
    console_writefln("Err Code: 0x%x", regs->err_code);
    console_writefln("EIP:      0x%x", regs->eip);

    if (regs->int_no == 14) {
        uint32_t cr2;
        asm volatile("mov %%cr2, %0" : "=r"(cr2));
        console_writefln("Fault Address (CR2): 0x%x", cr2);
    }
    console_writefln("ESP: %x  EBP: %x  ESI: %x  EDI: %x", regs->esp, regs->ebp, regs->esi, regs->edi);
    console_writefln("EAX: %x  EBX: %x  EDX: %x  ECX: %x", regs->eax, regs->ebx, regs->edx, regs->ecx);
    console_writefln("==============================================");

    while (1) {
        asm volatile("cli; hlt");
    }
}

void isr_handler(registers_t *regs) {
    if (regs->int_no < 32) {
        kernel_panic(regs);
    }
}

/* =========================
   PIC REMAP
========================= */

void pic_remap() {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xF8); // Master PIC: IRQ0 (timer), IRQ1 (keyboard), IRQ2 (cascade) enabled
    outb(0xA1, 0xEF); // Slave PIC: IRQ12 (mouse) enabled
}

/* =========================
   IDT INIT
========================= */

void* isr_stub_table[32] = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
};

extern void timer_stub(void);

void idt_init() {
    idtp.limit = sizeof(struct idt_entry) * 256 - 1;
    idtp.base  = (uint32_t)&idt;
    pic_remap();

    for (int i = 0; i < 32; i++) {
	    idt_set_gate(i, (uint32_t)isr_stub_table[i], 0x08, 0x8E);
    }

    for (int i = 32; i < 256; i++) {
        idt_set_gate(i, (uint32_t)isr_handler, 0x08, 0x8E);
    }

    idt_set_gate(0x21, (uint32_t)irq1_handler, 0x08, 0x8E);
    idt_set_gate(0x20, (uint32_t)timer_stub, 0x08, 0x8E);
    idt_set_gate(0x2C, (uint32_t)irq12_mouse_stub, 0x08, 0x8E);

    idt_load(&idtp);
}
