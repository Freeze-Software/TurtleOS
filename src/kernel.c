#include "kernel.h"
// Yea this thing is the big bad brainyy smart controller of everything
#define CMD_BUF_SIZE 128

static int streq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int starts_with(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

void reboot(void) {
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);
}

static void print_help(void) {
    console_writeln("  help");
    console_writeln("  clear");
    console_writeln("  echo");
    console_writeln("  reboot");
    console_writeln("  halt");
}

static void run_command(const char *cmd) {
    if (cmd[0] == '\0') {
        return;
    }

    if (streq(cmd, "help")) {
        print_help();
        return;
    }

    if (streq(cmd, "clear")) {
        console_clear();
        return;
    }

    if (starts_with(cmd, "echo ")) {
        console_writeln(cmd + 5);
        return;
    }

    if (streq(cmd, "reboot")) {
        console_writeln("Rebooting...");
        reboot();
        return;
    }

    if (streq(cmd, "halt")) {
        console_writeln("Halting CPU.");
        __asm__ volatile("cli");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    console_writeln("Unknown command. Type 'help'.");
}

void kernel_main(void) {
    char cmd_buf[CMD_BUF_SIZE];
    size_t cmd_len = 0;

    console_init();
    console_writeln("NewOS");
    console_writeln("Truely new.");

    for (;;) {
        console_write("NewOS> ");
        cmd_len = 0;

        for (;;) {
            char c = console_getc_blocking();

            if (c == '\r' || c == '\n') {
                console_putc('\n');
                cmd_buf[cmd_len] = '\0';
                break;
            }

            if (c == '\b' || c == 127) {
                if (cmd_len > 0) {
                    cmd_len--;
                    console_putc('\b');
                    console_putc(' ');
                    console_putc('\b');
                }
                continue;
            }

            if (c >= 32 && c <= 126) {
                if (cmd_len < CMD_BUF_SIZE - 1) {
                    cmd_buf[cmd_len++] = c;
                    console_putc(c);
                }
            }
        }

        run_command(cmd_buf);
    }
}
