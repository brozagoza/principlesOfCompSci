#include <stdio.h>
#include<stdlib.h>
#include<unistd.h> // fork, getpid, getppid
//#include "exit-utils.h"
#include<time.h>
#include<stdlib.h>
#include<sys/wait.h>

static const int kForkFailed = 1;

int main(int argc, char **argv) {
	srandom( time(NULL) );
	printf("I'm unique and get printed once!\n");

	pid_t pid = fork();
	if (pid == -1) {
		printf("Fork failed: %d\n", kForkFailed);
	}
	int parent = pid != 0;
	if ((random() % 2 == 0) == parent) {
		sleep(1);
	}
	if (parent) {
		waitpid(pid, NULL, 0);
	}
	printf("I get printed twice (this one is from the %s).\n", parent ? "parent" : "child");

	return 0;
}