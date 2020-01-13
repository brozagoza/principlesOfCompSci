#include <stdio.h>
#include<stdlib.h>
#include<unistd.h> // fork, getpid, getppid
//#include "exit-utils.h"

static const int kForkFailed = 1;
static const char * const kTrail = "abcd"; //cdecl.org

int main(int argc, char **argv) {
	printf("Let the forking begin\n");
	size_t trailLength = strlen(kTrail);
	for (size_t i = 0; i < trailLength; i++) {
		printf("%c\n", kTrail[i]);
		pid_t pid = fork();

		if (pid == -1) {
			printf("Fork failed\n");
			return -1;
		}
		
	}
	return 0;
}