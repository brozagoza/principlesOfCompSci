#include <cassert>
#include <ctime>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <sched.h>
#include <ext/stdio_filebuf.h>
#include "subprocess.h"

using namespace std;

struct worker {
  worker() {}
  worker(char *argv[]) : sp(subprocess(argv, true, false)), available(false) {}
  subprocess_t sp;
  bool available;
};

static const size_t kNumCPUs = sysconf(_SC_NPROCESSORS_ONLN);
static vector<worker> workers(kNumCPUs);
static size_t numWorkersAvailable = 0;


static void markWorkersAsAvailable(int sig) {
  pid_t pid;

  while (true) {
    pid = waitpid(-1, NULL, WUNTRACED | WNOHANG);

    if (pid == 0) {
      break;  // No more children
    } else {
        for (size_t i = 0; i < kNumCPUs; i++) {
          if (workers[i].sp.pid == pid) {
            workers[i].available = true;
            numWorkersAvailable++;
          }
        }
    }
  }
}

static const char *kWorkerArguments[] = {"./factor.py",  "--self-halting", NULL};
static void spawnAllWorkers() {
  cout << "There are this many CPUs: " << kNumCPUs << ", numbered 0 through " << kNumCPUs - 1 << "." << endl;
  for (size_t i = 0; i < kNumCPUs; i++) {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(i, &cpu_set);
    workers[i] = worker((char**)kWorkerArguments);
    sched_setaffinity(workers[i].sp.pid, sizeof(cpu_set), &cpu_set);
    cout << "Worker " << workers[i].sp.pid << " is set to run on CPU " << i << "." << endl;
  }
}

static size_t getAvailableWorker() {
  sigset_t empty;
  sigemptyset(&empty);

  while (numWorkersAvailable == 0) {
    sigsuspend(&empty);
  }

  sigprocmask(SIG_BLOCK, &empty, NULL);
  for (size_t i = 0; i < kNumCPUs; i++) {
    if (workers[i].available) {
      --numWorkersAvailable;
      workers[i].available = false;
      sigprocmask(SIG_UNBLOCK, &empty, NULL);
      return i;
    }
  }
  return 0;
}

static void broadcastNumbersToWorkers() {
  sigset_t empty;
  sigemptyset(&empty);

  while (true) {
    string line;
    getline(cin, line);
    if (cin.fail()) break;
    size_t endpos;
    long long num = stoll(line, &endpos);
    if (endpos != line.size()) break;

    size_t workerID = getAvailableWorker();
    kill(workers[workerID].sp.pid, SIGCONT);
    dprintf(workers[workerID].sp.supplyfd, "%llu\n", num);
  }
}

static void waitForAllWorkers() {
  sigset_t empty;
  sigemptyset(&empty);

  while (numWorkersAvailable < kNumCPUs) {
    sigsuspend(&empty);
  }
}

static void closeAllWorkers() {
  signal(SIGCHLD, SIG_DFL);

  // Close the processes
  for (size_t i = 0; i < kNumCPUs; i++) {
    kill(workers[i].sp.pid, SIGCONT);
    close(workers[i].sp.supplyfd);
  }

  // Wait for processes to close
  pid_t pid = waitpid(-1, NULL, WNOHANG);
  while (pid != -1) {
    pid = waitpid(-1, NULL, WNOHANG);
  }

}

int main(int argc, char *argv[]) {
  signal(SIGCHLD, markWorkersAsAvailable);
  spawnAllWorkers();
  broadcastNumbersToWorkers();
  waitForAllWorkers();
  closeAllWorkers();
  return 0;
}
