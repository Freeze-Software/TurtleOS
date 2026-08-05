#include <stdint.h>
#include "arch/gdt.h"
#include "arch/idt.h"
#include "memory/main.h"
#include "pit/pit.h"
#include "kernel.h"
#include "multitasking/task.h"
#include "arch/interrupts/main.h"
#include "mouse/mouse.h"

void kernel_main(uint32_t magic, uint32_t mb_addr) {
    cli();
    gdt_init();
    idt_init();
    init_heap();
    ps2mouse_init();
    pit_init(100);
    console_init(magic, mb_addr);
    add_task(shell, "Shell", 32768);
    sti();
}
