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

static int __cmd_info_handler(int argc, char **args)
{
	(void)argc;
	(void)args;
	printf("System information\n");
	printf("CPU[%s] MCU[%s] BOARD[%s]\n", RIOT_CPU, RIOT_MCU, RIOT_BOARD);
	return 0;	
}


static const shell_command_t usr_cmds[] = {
    { "info", "Print system information", __cmd_info_handler },
    { NULL, NULL, NULL }
};




int main(void)
{
	printf("Task04: Network example\n");
	char linebuf[SHELL_DEFAULT_BUFSIZE];
	/* shell_run is using while loop, so the code behind it can not be executed */
	shell_run(usr_cmds, linebuf, SHELL_DEFAULT_BUFSIZE);
	return 0;
}
