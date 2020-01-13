#include <stdio.h>
#include<stdlib.h>
#include<unistd.h> // fork, getpid, getppid
//#include "exit-utils.h"
#include<sys/wait.h> // wait

static const int kForkFailed = 1;
static const int kWaitFailed = 2;
static const char * const kTrail = "abcd"; //cdecl.org

int main(int argc, char **argv) {
	printf("Before\n");
	pid_t pid = fork();
	if (pid == -1) {
		printf("Fork failed: %d\n", kForkFailed);
	}

	if (pid == 0) {
		printf("Im the child and the parent will wait up for me.\n");
		printf("%d\n", 1/0);
		return 110;
	} else {
		int status;
		if (waitpid(pid, &status, 0) == pid) {
			return 0;
		} else {
			printf("Parents wait for child process with pid %d failed\n", pid);
		}

		if (WIFEXITED(status)) {
			printf("Child exited withs tatus %d\n", WIFEXITED(status));
		} else {
			printf("Child terminated abnormally.\n");
		}
	}

	return 0;
}