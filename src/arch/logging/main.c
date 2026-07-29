#include <stdint.h>
#include "../../libc/main.h"
#include "../../kernel.h"
#include "../../multitasking/task.h"

#define ENTRIES 100

typedef struct {
	char log[128];
	int task;
	char name[16]; //that's the [XXX] thing so it can be like [SHELL] or [PCI] or smth
}log_t;

log_t logs[100];

int tail = 0;
int head = 0;
int first_log = 1;

void log_init() {
	memset(logs, 0, sizeof(logs));
}

void log(char* string, char* name) {
	if (first_log == 0) {
		head = (head + 1) % (ENTRIES - 1);
	}
	strncpy(logs[head].log, string, strlen(string));
	logs[head].task = get_current_task();
	strncpy(logs[head].name, name, strlen(name));
	if (!first_log && head == tail) {
		tail++;
	}
	first_log = 0;
}

void log_print() {
	int i = head + 1;
	while (i != tail) {
		console_writefln("[%s][%d] %s", logs[i - 1].name, logs[i - 1].task, logs[i - 1].log);
		i = (i - 1) % (ENTRIES - 1);
	}
}
