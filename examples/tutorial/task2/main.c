#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "shell.h"
#include "thread.h"
#include "sched.h"
#include "algorithm.h"

#define MAX_THREAD 3
#define PRINT_MAX 100000000

struct test_args {
	int threadID;
};

struct thread_resources {
	struct test_args args;
	char test_thread_stack[THREAD_STACKSIZE_DEFAULT];
#ifdef DEVELHELP
	char test_thread_name[32];
#endif
	kernel_pid_t pid;
};
static struct thread_resources _thr_res[MAX_THREAD];
static void *_test_thread(void *args)
{
	(void)args;
	struct test_args* tst_args;
	uint32_t print;
	tst_args = (struct test_args*)args;
	print = 0;
	printf("Thread %d: running\n", tst_args->threadID);
	while (1) {
		print++;
		if ((print % 0xfffffff) == 0) {
			thread_t *me = (thread_t *)sched_active_thread;
			printf("\tInside Thread %d - pid[ %d]\n", tst_args->threadID, (int)me->pid);
			thread_yield();
		}
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
struct dining_args {
	int times;
	int yield;
	int lock;
	int silent;
};
static void *_dining_wrapper(void *args)
{
	(void)args;
	struct dining_args* dining_arg;
	dining_arg = (struct dining_args *)args;
	dining_phylosophy(dining_arg->times,
		dining_arg->yield,
		dining_arg->lock,
		dining_arg->silent);
	return NULL;
}

static char dining_thread_stack[THREAD_STACKSIZE_DEFAULT];
static struct dining_args dining_arg;
static int __cmd_dining_handler(int argc, char **args)
{
	(void)argc;
	(void)args;
	dining_arg.times = 5;
	dining_arg.yield = 1;
	dining_arg.lock = 1;
	dining_arg.silent = 0;
	printf("Dining phylosophy algorithm\n");
#if 0
	switch (argc) {
		case 5:
			silent = atoi(args[4]);
		case 4:
			lock = atoi(args[3]);
		case 3:
			yield = atoi(args[2]);
		case 2:
			times = atoi(args[1]);
		default:
			break;
	}
#else

	if (argc >= 5)
		dining_arg.silent = atoi(args[4]);
	if (argc >= 4)
		dining_arg.lock = atoi(args[3]);
	if (argc >= 3)
		dining_arg.yield = atoi(args[2]);
	if (argc >= 2)
		dining_arg.times = atoi(args[1]);
#endif
	/* start thread */
	if ((thread_create(dining_thread_stack, sizeof(dining_thread_stack), THREAD_PRIORITY_MAIN,
		THREAD_CREATE_STACKTEST,
		_dining_wrapper, &dining_arg, "Dining")) <= KERNEL_PID_UNDEF) {
		printf("error initializing thread\n");
		return 1;
    	}
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
	_thr_res[idx].args.threadID = idx;
#ifdef DEVELHELP
	memset(_thr_res[idx].test_thread_name, 0, 32);
	sprintf(_thr_res[idx].test_thread_name, "Thread %d", idx);
#endif
	/* start thread */
	if ((_thr_res[idx].pid = thread_create(_thr_res[idx].test_thread_stack, sizeof(_thr_res[idx].test_thread_stack), THREAD_PRIORITY_MAIN + 1,
		THREAD_CREATE_STACKTEST,
#ifdef DEVELHELP
		_test_thread, &_thr_res[idx].args, _thr_res[idx].test_thread_name)) <= KERNEL_PID_UNDEF) {
#else
		_test_thread, &_thr_res[idx].args, NULL)) <= KERNEL_PID_UNDEF) {
#endif
		printf("error initializing thread\n");
		return 1;
    	}

	return 0;	
}
 

static const shell_command_t usr_cmds[] = {
    { "info", "Print system information", __cmd_info_handler },
    { "thread", "Create new test thread: thread <ID>", __cmd_thread_handler },
    { "dining", "Running dining phylosophy", __cmd_dining_handler },
    { NULL, NULL, NULL }
};




int main(void)
{
	int i;
	printf("Task02: Threading and scheduling\n");
	for (i = 0; i < MAX_THREAD; i++) {
		_thr_res[i].pid = KERNEL_PID_UNDEF;
	}
	char linebuf[SHELL_DEFAULT_BUFSIZE];
	/* shell_run is using while loop, so the code behind it can not be executed */
	shell_run(usr_cmds, linebuf, SHELL_DEFAULT_BUFSIZE);
	return 0;
}
