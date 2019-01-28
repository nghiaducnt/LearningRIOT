#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "shell.h"
#include "thread.h"

#define MAX_THREAD 3

struct test_args {
	int threadID;
};
static char test_thread_stack[MAX_THREAD][THREAD_STACKSIZE_DEFAULT];
static struct test_args _test_args;

static void *_test_thread(void *args)
{
	(void)args;
	struct test_args* tst_args;
	tst_args = (struct test_args*)args;
	while (1) {
		printf("Thread %d: running\n", tst_args->threadID);
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


static int __cmd_thread_handler(int argc, char **args)
{
	(void)argc;
	(void)args;
	int idx;
	printf("Create thread %d\n", argc);
	idx = atoi(args[0]);
	/* start server (which means registering pktdump for the chosen port) */
	if (thread_create(test_thread_stack[idx], sizeof(test_thread_stack), THREAD_PRIORITY_MAIN - 1,
		THREAD_CREATE_STACKTEST,
		_test_thread, &_test_thread, "Test thread") <= KERNEL_PID_UNDEF) {
		printf("error initializing thread\n");
		return 1;
    	}

	return 0;	
}
 

static const shell_command_t usr_cmds[] = {
    { "info", "Print system information", __cmd_info_handler },
    { "thread", "Create new test thread", __cmd_thread_handler },
    { NULL, NULL, NULL }
};




int main(void)
{
	printf("Task02: Threading and scheduling\n");
	_test_args.threadID=0;
	char linebuf[SHELL_DEFAULT_BUFSIZE];
	//shell_run is using while loop, so the code behind it can not be executed
	shell_run(usr_cmds, linebuf, SHELL_DEFAULT_BUFSIZE);
	return 0;
}
