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

static char ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

static int contains_text(const char *text, const char *pattern) {
    if (*pattern == '\0') {
        return 1;
    }

    while (*text) {
        const char *text_scan = text;
        const char *pattern_scan = pattern;

        while (*text_scan && *pattern_scan && ascii_lower(*text_scan) == ascii_lower(*pattern_scan)) {
            text_scan++;
            pattern_scan++;
        }

        if (*pattern_scan == '\0') {
            return 1;
        }

        text++;
    }

    return 0;
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
    console_writeln("  Turtle talk");
}

static void turtle_talk(const char *message) {
    if (message[0] == '\0') {
        console_writeln("James: Hello Folks! I am James the Turtle! how are you?");
        return;
    }

    if (contains_text(message, "hello") || contains_text(message, "hi")) {
        console_writeln("James: Hello there.");
        return;
    }

    if (contains_text(message, "how are you") || contains_text(message, "hru")) {
        console_writeln("James: I am doing good, How are you?");
        return;
    }

    if (contains_text(message, "name")) {
        console_writeln("James: My name? Seriously, well whatever folk, my name is James, James the Turtle.");
        return;
    }

    if (contains_text(message, "help")) {
        console_writeln("James: Ya need help? Dont worry, just ask me some things, and I am glad to answer.");
        return;
    }

    if (contains_text(message, "joke")) {
        console_writeln("James: Sorry but I am not very funny");
        return;
    }

    if (contains_text(message, "sad") || contains_text(message, "bad")) {
        console_writeln("James: Dont worry mates, it will be okay... For me I am just a turtle, I hope things get better for ya. Just keep going.");
        return;
    }

    if (contains_text(message, "good") || contains_text(message, "great")) {
        console_writeln("James: Nice! I am glad to hear that!");
        return;
    }

    if (contains_text(message, "bye")) {
        console_writeln("James: See you later.");
        return;
    }

    console_writeln("James: I dont speak very good english, maybe try saying something in Turtalese.");
}

static void run_command(const char *cmd) {
    if (cmd[0] == '\0') {
        return;
    }

    if (streq(cmd, "help")) {
        print_help();
        return;
    }

    if (streq(cmd, "Turtle talk")) {
        turtle_talk("");
        return;
    }

    if (starts_with(cmd, "Turtle talk ")) {
        turtle_talk(cmd + 12);
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

    if (streq(cmd, "sysinfo")) {
            console_writeln("                             ___-------___");
            console_writeln("                         _-~~             ~~-_");
            console_writeln("                      _-~                    /~-_");
            console_writeln("   /^\\__/^\\         /~  \\                   /    \\");
            console_writeln(" /|  O|| O|        /      \\_______________/        \\");
            console_writeln("| |___||__|      /       /                \\          \\   OS: TurtleOS");
            console_writeln("|          \\    /      /                    \\          \\    Kernel: x86_64");
            console_writeln("|   (_______) /______/                        \\_________ \\   Version: 0.6");
            console_writeln("|         / /         \\                      /            \\");
            console_writeln(" \\         \\^\\\\         \\                  /               \\     /");
            console_writeln("   \\         ||           \\______________/      _-_       //\\__//");
            console_writeln("     \\       ||------_-~~-_ ------------- \\ --/~   ~\\    || __/");
            console_writeln("       ~-----||====/~     |==================|       |/~~~~~");
            console_writeln("        (_(__/  ./     /                    \\_\\      \\");
            console_writeln("               (_(___/                         \\_____)_)");
        return;
    }

    if (streq(cmd, "halt")) {
        console_writeln("Halting the turtle's shell...");
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
    console_writeln("   version 0.6.    ");
    console_writeln("                             ___-------___");
    console_writeln("                         _-~~             ~~-_");
    console_writeln("                      _-~                    /~-_");
    console_writeln("   /^\\__/^\\         /~  \\                   /    \\");
    console_writeln(" /|  O|| O|        /      \\_______________/        \\");
    console_writeln("| |___||__|      /       /                \\          \\");
    console_writeln("|          \\    /      /                    \\          \\");
    console_writeln("|   (_______) /______/                        \\_________ \\");
    console_writeln("|         / /         \\                      /            \\");
    console_writeln(" \\         \\^\\\\         \\                  /               \\     /");
    console_writeln("   \\         ||           \\______________/      _-_       //\\__//");
    console_writeln("     \\       ||------_-~~-_ ------------- \\ --/~   ~\\    || __/");
    console_writeln("       ~-----||====/~     |==================|       |/~~~~~");
    console_writeln("        (_(__/  ./     /                    \\_\\      \\");
    console_writeln("               (_(___/                         \\_____)_)");
    console_writeln("Home Computer System");
    console_writeln("  ");

    for (;;) {
        console_write("TurtleOS> ");
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
