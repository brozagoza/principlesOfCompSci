/**
 * File: subprocess.cc
 * -------------------
 * Presents the implementation of the subprocess routine.
 */

#include "subprocess.h"
using namespace std;

const int READ_END = 0;
const int WRITE_END = 1;

subprocess_t subprocess(char *argv[], bool supplyChildInput, bool ingestChildOutput) throw (SubprocessException) {
  int fds1[2];
  int fds2[2];
  pipe(fds1);
  pipe(fds2);
  subprocess_t process;

  process.supplyfd = (supplyChildInput) ? fds1[1] : kNotInUse;
  process.ingestfd = (ingestChildOutput) ? fds2[0] : kNotInUse;
  process.pid = fork();

  if (process.pid < 0) {
    throw SubprocessException("fork failed.");
  }

  if (process.pid == 0) {

    if (supplyChildInput) {
      dup2(fds1[READ_END], STDIN_FILENO);

      if (close(fds1[WRITE_END]) < 0) {
        throw SubprocessException("fds1 failed to close");
      }

      if (close(fds1[READ_END]) < 0) {
        throw SubprocessException("fds1 failed to close");
      }
    }

    if (ingestChildOutput) {
      dup2(fds2[WRITE_END], STDOUT_FILENO);

      if (close(fds2[READ_END]) < 0) {
        throw SubprocessException("fds2 failed to close");
      }
      
      if (close(fds2[WRITE_END]) < 0) {
        throw SubprocessException("fds2 failed to close");
      }
    }
    execvp(argv[0], argv);
  }

  close(fds1[0]);
  close(fds2[1]);
  return process;
}
