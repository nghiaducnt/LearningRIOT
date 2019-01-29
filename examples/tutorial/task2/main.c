#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "shell.h"
#include "thread.h"

#define MAX_THREAD 3
#define PRINT_MAX 100000000

struct test_args {
	int threadID;
};

struct thread_resources {
	char test_thread_stack[THREAD_STACKSIZE_DEFAULT];
#ifdef DEVELHELP
	char test_thread_name[32];
#endif
	kernel_pid_t pid;
};
static struct test_args _test_args;
static struct thread_resources _thr_res[MAX_THREAD];
static void *_test_thread(void *args)
{
	(void)args;
	struct test_args* tst_args;
	tst_args = (struct test_args*)args;
	printf("Thread %d: running\n", tst_args->threadID);
	while (1) {
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
	/* we need thread id */
	if (argc < 2) {
		printf("Need thread ID\n");
		return 1;
	}
	idx = atoi(args[1]);
	if (idx < 0 || idx >= MAX_THREAD) {
		printf("Invalid thread IDi[0-3]\n");
		return 1;
	}
	if (_thr_res[idx].pid != KERNEL_PID_UNDEF) {
		printf("Thread is running with PID[%d]\n", _thr_res[idx].pid);
		return 1;
	}
	_test_args.threadID = idx;
#ifdef DEVELHELP
	memset(_thr_res[idx].test_thread_name, 0, 32);
	sprintf(_thr_res[idx].test_thread_name, "Thread %d", idx);
#endif
	/* start thread */
	if ((_thr_res[idx].pid = thread_create(_thr_res[idx].test_thread_stack, sizeof(_thr_res[idx].test_thread_stack), THREAD_PRIORITY_MAIN + 1,
		THREAD_CREATE_STACKTEST,
#ifdef DEVELHELP
		_test_thread, &_test_args, _thr_res[idx].test_thread_name)) <= KERNEL_PID_UNDEF) {
#else
		_test_thread, &_test_args, NULL)) <= KERNEL_PID_UNDEF) {
#endif
		printf("error initializing thread\n");
		return 1;
    	}

	return 0;	
}
 

static const shell_command_t usr_cmds[] = {
    { "info", "Print system information", __cmd_info_handler },
    { "thread", "Create new test thread: thread <ID>", __cmd_thread_handler },
    { NULL, NULL, NULL }
};




int main(void)
{
	int i;
	printf("Task02: Threading and scheduling\n");
	for (i = 0; i < MAX_THREAD; i++) {
		_thr_res[i].pid = KERNEL_PID_UNDEF;
	}
	_test_args.threadID = 0;
	char linebuf[SHELL_DEFAULT_BUFSIZE];
	//shell_run is using while loop, so the code behind it can not be executed
	shell_run(usr_cmds, linebuf, SHELL_DEFAULT_BUFSIZE);
	return 0;
}
