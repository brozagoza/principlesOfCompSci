#include <stdio.h>
#include<stdlib.h>
#include<unistd.h> // fork, getpid, getppid
//#include "exit-utils.h"

static const int kForkFailed = 1;

int main(int argc, char **argv) {
	printf("Greetings from process %d! (parent: %d)\n", getpid(), getppid());
	pid_t pid = fork();
	//exitIf(pid == -1, kForkFailed, stderr, "fork function failed.\n");
	printf("Bye-bye from process %d! (parent: %d)\n", getpid(), getppid());

	return 0;
}