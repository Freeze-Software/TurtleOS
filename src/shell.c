#include "kernel.h"
#include "memory/main.h"
#include "arch/cpuinfo/main.h"
#include "libc/main.h"
#include "multitasking/task.h"
#include "arch/idt.h"
#include "arch/gdt.h"
#include "wm/main.h"
#include "keyboard/keyboard.h"
#include "fs/main.h"
#include "display.h"
#include "pci/pci.h"
#include "pit/pit.h"
#include "libc/main.h"
#include <stdint.h>
#define CMD_BUF_SIZE 128
#define USERNAME_MAX 31
#define PASSWORD_MAX 63
#define USER_DB_LBA 2048u
#define USER_DB_MAGIC 0x54555352u
#define USER_DB_VERSION 1u

typedef struct {
	int free;
	char user[USERNAME_MAX + 1];
	uint64_t password_hash;
} user_t;

typedef struct {
	user_t users[16];
	int has_users;
}user_db_t;

user_db_t user_db;
static int g_logged_in = 0;
static char g_current_user[USERNAME_MAX + 1];

void print_user_db() {
	for (int i = 0; i < 16; i++) {
		char username[16];
		strncpy(username, user_db.users[i].user, 15);
		username[15] = '\0';
		console_writefln("User: ", username);
		console_writefln("Hash: %d", user_db.users[i].password_hash);
	}
}

int current_dir_id = 0;

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

static size_t str_len(const char *s) {
    size_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static void mem_zero(void *ptr, size_t n) {
    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < n; i++) {
        p[i] = 0;
    }
}

static void str_copy(char *dst, size_t dst_size, const char *src) {
    size_t i = 0;
    if (dst_size == 0) {
        return;
    }
    while (i + 1 < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int parse_two_args(const char *s, char *a, size_t a_size, char *b, size_t b_size) {
    size_t i = 0;
    size_t j = 0;

    while (*s == ' ') {
        s++;
    }

    while (*s && *s != ' ') {
        if (i + 1 >= a_size) {
            return 0;
        }
        a[i++] = *s++;
    }
    a[i] = '\0';

    while (*s == ' ') {
        s++;
    }

    while (*s && *s != ' ') {
        if (j + 1 >= b_size) {
            return 0;
        }
        b[j++] = *s++;
    }
    b[j] = '\0';

    while (*s == ' ') {
        s++;
    }

    if (i == 0 || j == 0 || *s != '\0') {
        return 0;
    }

    return 1;
}

static void read_line_prompt(const char *prompt, char *buf, size_t buf_size, int hide_input) {
    size_t len = 0;
    console_write(prompt);

    for (;;) {
	if (kb_available()) {
            char c = getchar();

            if (c == '\r' || c == '\n') {
                console_putc('\n');
                break;
            }

            if (c == '\b' || c == 127) {
                if (len > 0) {
                    len--;
                    console_backspace();
                }
                continue;
            }

            if (c >= 32 && c <= 126 && len + 1 < buf_size) {
                buf[len++] = c;
                console_putc(hide_input ? '*' : c);
            }
        }
	__asm__ volatile("hlt");
    }

    buf[len] = '\0';
}

static uint32_t hash_password(const char *username, const char *password) {
    uint32_t h = 2166136261u;
    const uint32_t pepper = 0x9E3779B9u;

    while (*username) {
        h ^= (uint8_t)*username++;
        h *= 16777619u;
    }
    h ^= (uint32_t)':';
    h *= 16777619u;
    while (*password) {
        h ^= (uint8_t)*password++;
        h *= 16777619u;
    }

    h ^= pepper;
    h *= 16777619u;
    return h;
}

/*
static uint32_t user_db_checksum(const user_db_sector_t *db) {
    const uint8_t *bytes = (const uint8_t *)db;
    uint32_t sum = 5381u;

    for (size_t i = 0; i < sizeof(user_db_sector_t); i++) {
        if (i >= 8 && i < 12) {
            continue;
        }
        sum = ((sum << 5) + sum) + bytes[i];
    }

    return sum;
}
*/

int user_db_save(void) {
	int users_file = find_entry("users", find_entry("etc", 0));
	int success = file_write(users_file, &user_db, sizeof(user_db));
	if (success == 0) {
		success = 1;
	}
	console_writefln("Save Success: %d", success);
	return success;
}

int user_db_load(void) {
    int users_file = find_entry("users", find_entry("etc", 0));
    size_t size = file_get_size(users_file);

    if (size < sizeof(user_db)) {
        console_writeln("User file size mismatch");
        return -1;
    }

    size_t alloc_size = (sizeof(user_db) + 511) & ~511;
    void *buffer = malloc(alloc_size);
    int success = file_read(users_file, buffer);

    console_writefln("File read success: %d", success);

    if (success == 0) {
        memcpy(&user_db, buffer, sizeof(user_db));
        success = 1;
    }

    free(buffer);
    console_writefln("Load Success: %d", success);
    return success;
}

static void user_db_reset(void) {
    mem_zero(&user_db, sizeof(user_db));
    for (int i = 0; i < 16; i++) {
	    user_db.users[i].free = 1;
    }
    user_db_save();
    user_db_load();
}

/*
static void user_db_load(void) {
    if (!ata_read_sector(USER_DB_LBA, &g_user_db)) {
        user_db_reset();
        return;
    }

    if (g_user_db.magic != USER_DB_MAGIC || g_user_db.version != USER_DB_VERSION) {
        user_db_reset();
        return;
    }

    if (g_user_db.checksum != user_db_checksum(&g_user_db)) {
        user_db_reset();
        return;
    }
}
*/

static int create_user(const char *username, const char *password) {
    char log_out[128];
    int home_dir_id = find_entry("home", 0);
    make_dir((char*)username, home_dir_id);
    sprintf(log_out, "Made new home directory for %s", username);
    log(log_out, "SHELL");
    if (str_len(username) == 0 || str_len(password) == 0) {
        return 0;
    }
    if (str_len(username) > USERNAME_MAX || str_len(password) > PASSWORD_MAX) {
        return 0;
    }

    user_db.has_users = 1;
    int found_free_slot = 0;
    for (int i = 0; i < 16; i++) {
	sprintf(log_out, "Slot %d Free: %d", i, user_db.users[i].free);
	log(log_out, "SHELL");
    }
    for (int i = 0; i < 16; i++) {
	    if (user_db.users[i].free == 1) {
		sprintf(log_out, "User Slot %d is free for %s", i, username);
		log(log_out, "SHELL");
		user_db.users[i].free = 0;
		strncpy(user_db.users[i].user, username, strlen((char*)username));
		user_db.users[i].password_hash = hash_password(username, password);
		found_free_slot = 1;
		break;
	    } else {
		    continue;
	    }
    }

    if (user_db_save() != 1) {
	log("Failed to save User DB", "SHELL");
        return 0;
    }

    if (found_free_slot == 0) {
	    log("Couldn't find free User Slot", "SHELL");
    }

    str_copy(g_current_user, sizeof(g_current_user), username);
    g_logged_in = 1;
    return 1;
}

static int try_login(const char *username, const char *password) {
    user_db_load();
    uint32_t h;

    if (!user_db.has_users) {
        return 0;
    }
    int user_exists = 0;
    for (int i = 0; i < 16; i++) {
	    if (streq(user_db.users[i].user, username)) {
		    user_exists = 1;
	    }
    }

    if (user_exists == 0) {
	    console_writeln("User doesn't exist");
	    return 0;
    }

    h = hash_password(username, password);
    for (int i = 0; i < 16; i++) {
	    if (!streq(user_db.users[i].user, username)) {
		    continue;
	    }
	    if (user_db.users[i].password_hash != h) {
		    console_writeln("Wrong password");
		    return 0;
	    }
    }

    str_copy(g_current_user, sizeof(g_current_user), username);
    g_logged_in = 1;
    return 1;
}

static int change_password(const char* username, const char *old_password, const char *new_password) {
	int user_exists = 0;
	int user_id = -1;
	for (int i = 0; i < 16; i++) {
		if (streq(user_db.users[i].user, username)) {
			user_exists = 1;
			user_id = i;
		}
	} 

	if (user_exists == 0) {
		return 0;
	}

	if (hash_password(username, old_password) == user_db.users[user_id].password_hash) {
		user_db.users[user_id].password_hash = hash_password(username, new_password);
	}

    if (!user_db_save()) {
        return 0;
    }
    return 1;
}

static void auth_boot_flow(void) {
    char username[USERNAME_MAX + 1];
    char password[PASSWORD_MAX + 1];

    user_db_load();

    if (user_db.has_users) {
	console_writeln("--Login--");
	for (int logins = 0; logins < 3; logins ++) {
            read_line_prompt("Username: ", username, sizeof(username), 0);
	    read_line_prompt("Password: ", password, sizeof(password), 1);
	    int success = try_login(username, password);
	    if (success == 1) {
		current_dir_id = find_entry(g_current_user, find_entry("home", 0));
		console_writeln("Success!");
		return;
	    } else {
		console_writeln("Failed!");
		continue;
	    }
	}
	reboot();
    }

    console_writeln("Account");
    for (;;) {
        read_line_prompt("Username: ", username, sizeof(username), 0);
        read_line_prompt("Password: ", password, sizeof(password), 1);

        if (create_user(username, password)) {
            console_write("Account created. Logged in as ");
            console_writeln(g_current_user);
	    current_dir_id = find_entry(g_current_user, find_entry("home", 0));
            break;
        }

        console_writeln("Failed to create account.");
    }
}

void reboot(void) {
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);
}

char* help_cmds[] = {"help", "clear", "echo", "date", "calc", "useradd", "login", "whoami", "passwd", "sysinfo", "reboot", "halt", "Turtle talk", "color", "wm", "ls", "write", "mkdir", "rm", "touch", "crash"};

#define CMD_COUNT 21

static void print_help(void) {
	for (int i = 0; i < CMD_COUNT; i++) {
		console_writefln("  %s", help_cmds[i]);
	}
}

static void print_uint2(unsigned int n) {
    console_putc((char)('0' + (n / 10) % 10));
    console_putc((char)('0' + n % 10));
}

static void print_uint(unsigned int n) {
    char buf[12];
    int len = 0;
    if (n == 0) { console_putc('0'); return; }
    while (n > 0) { buf[len++] = (char)('0' + n % 10); n /= 10; }
    for (int i = len - 1; i >= 0; i--) console_putc(buf[i]);
}

static void print_int(int n) {
    if (n < 0) { console_putc('-'); print_uint((unsigned int)-n); }
    else print_uint((unsigned int)n);
}

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    io_wait();
    return inb(0x71);
}

static uint8_t bcd2bin(uint8_t v) {
    return (uint8_t)((v & 0x0F) + ((v >> 4) * 10));
}

static void cmd_date(void) {
    while (cmos_read(0x0A) & 0x80) {}
    uint8_t sec  = bcd2bin(cmos_read(0x00));
    uint8_t min  = bcd2bin(cmos_read(0x02));
    uint8_t hour = bcd2bin(cmos_read(0x04));
    uint8_t day  = bcd2bin(cmos_read(0x07));
    uint8_t mon  = bcd2bin(cmos_read(0x08));
    uint8_t year = bcd2bin(cmos_read(0x09));
    console_write("Date: 20");
    print_uint2(year); console_putc('-');
    print_uint2(mon);  console_putc('-');
    print_uint2(day);
    console_write("  Time: ");
    print_uint2(hour); console_putc(':');
    print_uint2(min);  console_putc(':');
    print_uint2(sec);  console_putc('\n');
}

static const char *calc_pos;

static void calc_skip(void) {
    while (*calc_pos == ' ') calc_pos++;
}

static int calc_expr(int *out);

static int calc_factor(int *out) {
    calc_skip();
    if (*calc_pos == '(') {
        calc_pos++;
        if (!calc_expr(out)) return 0;
        calc_skip();
        if (*calc_pos == ')') calc_pos++;
        return 1;
    }
    int neg = 0;
    if (*calc_pos == '-') { neg = 1; calc_pos++; }
    if (*calc_pos < '0' || *calc_pos > '9') return 0;
    int n = 0;
    while (*calc_pos >= '0' && *calc_pos <= '9') { n = n * 10 + (*calc_pos++ - '0'); }
    *out = neg ? -n : n;
    return 1;
}

static int calc_term(int *out) {
    int left;
    if (!calc_factor(&left)) return 0;
    calc_skip();
    while (*calc_pos == '*' || *calc_pos == '/') {
        char op = *calc_pos++;
        int right;
        if (!calc_factor(&right)) return 0;
        if (op == '/') {
            if (right == 0) { console_writeln("Error: division by zero"); return 0; }
            left /= right;
        } else { left *= right; }
        calc_skip();
    }
    *out = left;
    return 1;
}

static int calc_expr(int *out) {
    int left;
    if (!calc_term(&left)) return 0;
    calc_skip();
    while (*calc_pos == '+' || *calc_pos == '-') {
        char op = *calc_pos++;
        int right;
        if (!calc_term(&right)) return 0;
        left = (op == '+') ? left + right : left - right;
        calc_skip();
    }
    *out = left;
    return 1;
}

static void cmd_calc(const char *expr) {
    if (expr[0] == '\0') { console_writeln("Calculator."); return; }
    calc_pos = expr;
    int result;
    if (!calc_expr(&result)) { console_writeln("Error: invalid expression"); return; }
    calc_skip();
    if (*calc_pos != '\0') { console_writeln("Error: unexpected character"); return; }
    console_write("= ");
    print_int(result);
    console_putc('\n');
}

static uint32_t swamp_seed = 0;

static uint32_t swamp_rand(void) {
    if (swamp_seed == 0) {
        uint8_t sec = bcd2bin(cmos_read(0x00));
        uint8_t min = bcd2bin(cmos_read(0x02));
        uint8_t hour = bcd2bin(cmos_read(0x04));
        swamp_seed = ((uint32_t)hour << 16) ^ ((uint32_t)min << 8) ^ sec ^ 0xA5A5u;
        if (swamp_seed == 0) {
            swamp_seed = 1;
        }
    }

    swamp_seed = swamp_seed * 1664525u + 1013904223u;
    return swamp_seed;
}

static int parse_uint(const char *s, unsigned int *out) {
    unsigned int n = 0;
    int saw_digit = 0;

    while (*s == ' ') {
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        saw_digit = 1;
        n = (n * 10u) + (unsigned int)(*s - '0');
        s++;
    }

    while (*s == ' ') {
        s++;
    }

    if (!saw_digit || *s != '\0') {
        return 0;
    }

    *out = n;
    return 1;
}

static void cmd_swamp(const char *arg) {
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "!@#$%^&*()_+-=[]{};:'\",.<>/?\\|`~";
    const unsigned int charset_len = (unsigned int)(sizeof(chars) - 1);
    unsigned int lines = 10;
    unsigned int width = 60;

    if (arg[0] != '\0') {
        if (!parse_uint(arg, &lines)) {
            console_writeln("I am not gonna put anything here yet since im lazy :/");
            return;
        }
    }

    if (lines == 0) {
        lines = 1;
    }
    if (lines > 40) {
        lines = 40;
    }

    console_writeln("  ");
    for (unsigned int y = 0; y < lines; y++) {
        for (unsigned int x = 0; x < width; x++) {
            uint32_t r = swamp_rand();
            console_putc(chars[r % charset_len]);
        }
        console_putc('\n');
    }
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

void cmd_ptop() {
	task_info_t* tasks = (task_info_t*)malloc(sizeof(task_info_t) * get_task_count());
	tasks_get_info(tasks, get_task_count());
	for (int task = 0; task < get_task_count(); task++) {
		console_writefln("PID: %d; Name: %s, Active: %d", tasks[task].pid, tasks[task].name, tasks[task].active);	
	}	
}

static void run_command(const char *cmd) {
    char a[USERNAME_MAX + 1];
    char b[PASSWORD_MAX + 1];

    if (cmd[0] == '\0') {
        return;
    }

    if (streq(cmd, "help")) {
        print_help();
        return;
    }

    if (streq(cmd, "crash")) {
	    asm volatile("int $0");
	    return;
    }

    if (streq(cmd, "ptop")) {
	    cmd_ptop();
	    return;
    }

    if (streq(cmd, "log")) {
	    log_print();
	    return;
    }

    if (streq(cmd, "cpu")) {
	    char buffer[49];
	    get_cpu_name(buffer);
	    console_writeln(buffer);
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

    if (streq(cmd, "lines")) {
	    draw_line(0, 0, 1920, 1080, 0x00FF00);
	    draw_line(1920, 0, 0, 1080, 0x0000FF);
	    return;
    }

    if (streq(cmd, "date")) {
        cmd_date();
        return;
    }

    if (starts_with(cmd, "calc ")) {
        cmd_calc(cmd + 5);
        return;
    }

    if (streq(cmd, "rect")) {
	    draw_rect(200, 200, 200, 200, 0xFFFFFF);
	    return;
    }
	
    if (streq(cmd, "rect2")) {
	    draw_rect(300, 300, 300, 300, 0xFFFFFF);
	    return;
    }

    if (streq(cmd, "calc")) {
        cmd_calc("");
        return;
    }

    if (streq(cmd, "swamp")) {
        cmd_swamp("");
        return;
    }

    if (starts_with(cmd, "swamp ")) {
        cmd_swamp(cmd + 10);
        return;
    }

    if (streq(cmd, "clear")) {
        console_clear();
        return;
    }

    if (starts_with(cmd, "color ")) {
        char* color_str = cmd + 9;
	uint32_t color_hex = string_to_hex(color_str);
	if (cmd[6] == 'f' && cmd[7] == 'g') {
            set_console_fg_color(color_hex);
	} else if (cmd[6] == 'b' && cmd[7] == 'g') {
            set_console_bg_color(color_hex);
	}
	return;
    }

    if (starts_with(cmd, "echo ")) {
        console_writeln(cmd + 5);
        return;
    }

    if (starts_with(cmd, "useradd ")) {
        if (!parse_two_args(cmd + 8, a, sizeof(a), b, sizeof(b))) {
            console_writeln("Usage: useradd user password");
            return;
        }
        if (create_user(a, b)) {
            console_writeln("User created");
        } else {
            console_writeln("Failed to create user.");
        }
        return;
    }

    if (starts_with(cmd, "login ")) {
        if (!parse_two_args(cmd + 6, a, sizeof(a), b, sizeof(b))) {
            console_writeln("Usage: login");
            return;
        }
        if (try_login(a, b)) {
            console_write("Logged in as ");
            console_writeln(g_current_user);
        } else {
            console_writeln("Login failed.");
        }
        return;
    }

    if (streq(cmd, "logout")) {
        if (!g_logged_in) {
            console_writeln("Not logged in.");
            return;
        }
        g_logged_in = 0;
        g_current_user[0] = '\0';
        console_writeln("Logged out.");
        reboot();
        return;
    }

    if (starts_with(cmd, "wm ")) {
	const char* color_str = cmd + 3;
	uint32_t color_int = string_to_hex(color_str);
	wm_background_color = color_int;
        int window = create_window("Test", 100, 100, 1);
	window_pixel_chessboard(window);
	int window2 = create_window("Test2", 100, 100, 1);
	window_printf(window2, 0, 0, 1, 0x0000FF, 0x00FF00, "Hi!");
	//int window3 = create_window("Test3", 100, 100);
	//window_printf(window3, 0, 0, 1, 0x0000FF, 0x00FF00, "Current User: %s", g_current_user);
        add_task(wm_render, "Window Manager", 8096);
        return;
    }

	if (streq(cmd, "ls")) {

	   fs_entry_t entries[64];

	   int count = list_dir(
		current_dir_id,
		entries,
		64
	   );

	   for (int i = 0; i < count; i++) {
                char* entry = entries[i].name;
			

		console_writefln(
		   "%s %s",
		   entries[i].type == 2
			? "<DIR>"
			: "<FILE>",
		   entries[i].name
		);
	   }

	   return;
	}

	if (starts_with(cmd, "cd ")) {
	   const char* name = cmd + 3;

	   if (name[0] == '-') {
                fs_entry_t entry;
                get_entry_by_id(current_dir_id, &entry, NULL, NULL);
		current_dir_id = entry.parent;
		return;
	   }

	   int id = find_entry(
		(char*)name,
		current_dir_id
	   );

	   if (id < 0) {
		console_writefln("Directory not found", true, 0, 0);
		return;
	   }

	   fs_entry_t entry;

	   if (get_entry_by_id(
		id,
		&entry,
		NULL,
		NULL
	   ) < 0) {
		return;
	   }

	   if (entry.type != 2) {
		console_writefln("Not a directory", true, 0, 0);
		return;
	   }

	   current_dir_id = id;
	   return;
	}

	if (starts_with(cmd, "mkdir ")) {

	   const char* name = cmd + 6;

	   int id = make_dir(
		(char*)name,
		current_dir_id
	   );

	   console_writefln(
		"Directory created (%d)",
		id
	   );

	   return;
	}

	if (starts_with(cmd, "touch ")) {

	   const char* name = cmd + 6;

	   int id = make_file(
		(char*)name,
		current_dir_id,
		1024
	   );

	   console_writefln(
		"File created (%d)",
		id
	   );

	   return;
	}

	if (starts_with(cmd, "cat ")) {

	   const char* name = cmd + 4;

	   int id = find_entry(
		(char*)name,
		current_dir_id
	   );

	   if (id < 0) {
		console_writefln("File not found");
		return;
	   }

	   int size = file_get_size(id);

	   char* buffer = malloc(size + 1);

	   file_read(id, buffer);

	   buffer[size] = '\0';

	   console_writefln("%s", buffer);

	   free(buffer);

	   return;
	}

	if (starts_with(cmd, "write ")) {

	   char* filename = cmd + 6;

	   char* text = strchr(filename, ' ');

	   if (!text) {
		console_writefln("Usage: write <file> <text>");
		return;
	   }

	   *text = '\0';
	   text++;

	   int id = find_entry(
		filename,
		current_dir_id
	   );

	   if (id < 0) {
		console_writefln("File not found");
		return;
	   }

	   file_write(
		id,
		text,
		strlen(text)
	   );

	   console_writefln("Written");

	   return;
	}

	if (starts_with(cmd, "rm ")) {
	   const char* name = cmd + 3;

	   int id = find_entry(
		(char*)name,
		current_dir_id
	   );

	   if (id < 0) {
		console_writefln("Not found");
		return;
	   }

	   fs_entry_t entry;

	   get_entry_by_id(
		id,
		&entry,
		NULL,
		NULL
	   );

	   if (entry.type == 1) {
		file_delete(id);
	   } else {
		dir_delete(id);
	   }

	   console_writefln("Deleted");

	   return;
	}

    if (streq(cmd, "whoami")) {
        if (g_logged_in) {
            console_writeln(g_current_user);
        } else {
            console_writeln("Error");
        }
        return;
    }

    if (starts_with(cmd, "passwd")) {
	char username[USERNAME_MAX + 1];
	char old_password[PASSWORD_MAX + 1];
	char new_password[PASSWORD_MAX + 1];

	read_line_prompt("Username: ", username, sizeof(username), 0);
	read_line_prompt("Current password: ", old_password, sizeof(old_password), 1);
	read_line_prompt("New password: ", new_password, sizeof(new_password), 1);

        if (change_password(username, old_password, new_password)) {
            console_writeln("Password changed.");
        } else {
            console_writeln("Password change failed.");
        }
        return;
    }

    if (streq(cmd, "reboot")) {
        console_writeln("Rebooting...");
        reboot();
        return;
    }

    if (streq(cmd, "sysinfo")) {
	    set_console_fg_color(0x00FF00);
            console_writeln("\033[32m                             ___-------___\033[0m");
            console_writeln("\033[32m                         _-~~             ~~-_\033[0m");
            console_writeln("\033[32m                      _-~                    /~-_\033[0m");
            console_writeln("\033[32m   /^\\__/^\\         /~  \\                   /    \\\033[0m");
            console_writeln("\033[32m /|  O|| O|        /      \\_______________/        \\\033[0m");
            console_writeln("\033[32m| |___||__|      /       /                \\          \\   \033[36mOS: TurtleOS\033[0m");
            console_writeln("\033[32m|          \\    /      /                    \\          \\    \033[36mKernel: x86_64\033[0m");
            console_writeln("\033[32m|   (_______) /______/                        \\_________ \\   \033[36mVersion: 0.6\033[0m");
            console_writeln("\033[32m|         / /         \\                      /            \\\033[0m");
            console_writeln("\033[32m \\         \\^\\\\         \\                  /               \\     /\033[0m");
            console_writeln("\033[32m   \\         ||           \\______________/      _-_       //\\__//\033[0m");
            console_writeln("\033[32m     \\       ||------_-~~-_ ------------- \\ --/~   ~\\    || __/\033[0m");
            console_writeln("\033[32m       ~-----||====/~     |==================|       |/~~~~~\033[0m");
            console_writeln("\033[32m        (_(__/  ./     /                    \\_\\      \\\033[0m");
            console_writeln("\033[32m               (_(___/                         \\_____)_)\033[0m");
	    set_console_fg_color(0xFFFFFF);
        return;
    }

    if (streq(cmd, "halt")) {
        console_writeln("Halting...");
        __asm__ volatile("cli");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    console_writeln("command not found");
}

void print_current_dir_complete() {
    int dir_id = current_dir_id;

    fs_entry_t path[64];
    int depth = 0;

    while (dir_id != -1 && dir_id != 0 && depth < 64) {
        if (get_entry_by_id(dir_id, &path[depth], NULL, NULL) < 0) {
            break;
        }
        dir_id = path[depth].parent;
        depth++;
    }

    console_write("/");

    for (int i = depth - 1; i >= 0; i--) {
        console_write(path[i].name);
        console_write("/");
    }
}

void write_prompt() {
	print_current_dir_complete();
	console_write(" TurtleOS> ");
}

void shell() {
    log_init();
    log("Successfully inited logging", "SHELL");
    char cmd_buf[CMD_BUF_SIZE];
    size_t cmd_len = 0;
    set_console_fg_color(0x00FF00);
    console_writeln("       ");
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
    console_writeln("\033[32mHome Computer System\033[0m");
    console_writeln("  ");
    set_console_fg_color(0xFFFFFF);
    console_writeln("----PCI----");
    pci_enumerate(&g_pci_bus);
    console_writeln("----DISK----");
    if (!tfs_mount()) {
	    console_writeln("Seems like you're not formated to TFS!");
	    tfs_format();
	    log("Making Directories for new formated disk", "SHELL");
	    int etc_id = make_dir("etc", 0);
	    make_dir("home", 0);
	    make_file("users", etc_id, sizeof(user_db_t));
	    user_db_reset();
    }
    int disk_size_bytes = ata_get_sector_count() * 512;
    console_writefln("Disk size(sectors): %d", ata_get_sector_count());
    console_writefln("Disk size(KiB): %d", disk_size_bytes / 1024);
    console_writefln("Disk size(MiB): %d", disk_size_bytes / (1024 * 1024));
    char disk_log_out[128];
    sprintf(disk_log_out, "Disk size in MB: %d", disk_size_bytes / (1024 * 1024));
    log(disk_log_out, "SHELL");
    console_writeln("----HEAP----");
    heap_stats_t heap_stats = heap_get_stats();
    console_writefln("HEAP: %d", heap_stats.total);
    console_writefln("USED: %d", heap_stats.used);
    console_writefln("FREE: %d", heap_stats.free);
    console_writefln("----LOGIN----");
    user_db_load();
    auth_boot_flow();
    //console_writeln("  ");
    int new_prompt = 1;
    cmd_len = 0;
    while (1) {
	if (new_prompt == 1) {
	    write_prompt();
	    new_prompt = 0;
	}
        if (kb_available()) {
            char c = getchar();
            if (c == '\r' || c == '\n') {
                console_putc('\n');
                cmd_buf[cmd_len] = '\0';
		cmd_len = 0;
                run_command(cmd_buf);
		new_prompt = 1;
            }

            if (c == '\b' || c == 127) {
                if (cmd_len > 0) {
                    cmd_len--;
                    console_backspace();		    
                }
            }

            if (c >= 32 && c <= 126) {
                if (cmd_len < CMD_BUF_SIZE - 1) {
                    cmd_buf[cmd_len++] = c;
                    console_putc(c);
                }
            }
	}
	render_console();
	__asm__ volatile("hlt");
    }
}
