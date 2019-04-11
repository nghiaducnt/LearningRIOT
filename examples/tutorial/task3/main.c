#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stddef.h>
#include "shell.h"
#include "thread.h"
#include "xtimer.h"

#define PRINT_VAL(id) printf("The value of " #id " is %d\n", id);
char thread_stack[5][THREAD_STACKSIZE_MAIN];
int current_stack = 0;
static void *_timer_thread(void *args)
{
	(void)args;
	while (1) {
		xtimer_ticks32_t t = xtimer_now();
		printf("\t %d: Thread start: %lu\n", sched_active_thread->pid, t.ticks32);
		/* sleep  */
		xtimer_sleep(10);
		t = xtimer_now();
		printf("\t %d: Thread stop: %lu\n", sched_active_thread->pid, t.ticks32);
		break;
	}
	return NULL;
}

static int __cmd_info_handler(int argc, char **args)
{
	(void)argc;
	(void)args;
	printf("System information\n");
	printf("CPU[%s] MCU[%s] BOARD[%s]\n", RIOT_CPU, RIOT_MCU, RIOT_BOARD);
	return 0;	
}

static int __cmd_time_handler(int argc, char **args)
{
	(void)argc;
	(void)args;
	int i = 0;
	/* start thread */
	for (i = 0; i < 5; i++) {
		if (thread_create(
			thread_stack[current_stack++ % 5],
			sizeof(thread_stack[0]),
			THREAD_PRIORITY_MAIN + 1,
			THREAD_CREATE_STACKTEST,
			_timer_thread, NULL, "Heartbeat") <= KERNEL_PID_UNDEF) {
			printf("error initializing thread\n");
			return 1;
    		}
		xtimer_sleep(1);
	}

	return 0;	
}
 

static const shell_command_t usr_cmds[] = {
    { "info", "Print system information", __cmd_info_handler },
    { "time", "Print current time", __cmd_time_handler },
    { NULL, NULL, NULL }
};




int main(void)
{
	printf("Task03: Create timer\n");
	char linebuf[SHELL_DEFAULT_BUFSIZE];
	/* shell_run is using while loop, so the code behind it can not be executed */
	shell_run(usr_cmds, linebuf, SHELL_DEFAULT_BUFSIZE);
	return 0;
}
