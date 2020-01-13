/**
 * File: pipeline.c
 * ----------------
 * Presents the implementation of the pipeline routine.
 */

#include "pipeline.h"
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

const int READ_END = 0;
const int WRITE_END = 1;

void pipeline(char *argv1[], char *argv2[], pid_t pids[]) {
  int fd[2];
  pipe(fd);

  pid_t pid = fork();

  // First Child
  if (pid == 0) {
    dup2(fd[WRITE_END], STDOUT_FILENO);
    close(fd[READ_END]);
    close(fd[WRITE_END]);
    execvp(argv1[0], argv1);
  }

  pids[0] = pid;
  pid = fork();

  // Second Child
  if (pid == 0) {
    dup2(fd[READ_END], STDIN_FILENO);
    close(fd[WRITE_END]);
    close(fd[READ_END]);
    execvp(argv2[0], argv2);
  }

  pids[1] = pid;

  close(fd[READ_END]);
  close(fd[WRITE_END]);
}
