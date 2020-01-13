#include<stdio.h>
#include<stdlib.h>
#include<unistd.h> // fork, getpid, getppid
//#include "exit-utils.h"
#include<errno.h>
#include<sys/wait.h>

static const int kForkFailed = 1;
static const int kWaitFailed = 2;
static const int kNumChildren = 8;

int main(int argc, char **argv) {
	for (size_t i = 0; i < kNumChildren; i++) {
		pid_t pid = fork();
		if (pid == -1) {
			printf("Fork failed %d", kForkFailed);
		}
		if (pid == 0) {
			return (110 + i);
		}
	}

	while (1) {
		int status;
		pid_t pid = waitpid(-1, &status, 0);
		if (pid == -1) {
			break;
		}
		if (WIFEXITED(status)) {
			printf("Child %d exited with status %d\n", pid, WEXITSTATUS(status));
		} else {
			printf("Child %d id exited abnormally.\n", pid);
		}
	}

	if (errno == ECHILD) {
		return 0;
	} else {
		printf("waitpid failed\n");
	}

	return 0;
}