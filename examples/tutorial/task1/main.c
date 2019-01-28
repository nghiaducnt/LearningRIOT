#include <stdio.h>
#include <string.h>

#include "shell.h"

int main(void)
{
	printf("Task01: Hello from %s\n", RIOT_BOARD);
	char linebuf[SHELL_DEFAULT_BUFSIZE];
	//shell_run is using while loop, so the code behind it can not be executed
	shell_run(NULL, linebuf, SHELL_DEFAULT_BUFSIZE);
	return 0;
}
