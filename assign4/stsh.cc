/**
 * File: stsh.cc
 * -------------
 * Defines the entry point of the stsh executable.
 */

#include "stsh-parser/stsh-parse.h"
#include "stsh-parser/stsh-readline.h"
#include "stsh-parser/stsh-parse-exception.h"
#include "stsh-signal.h"
#include "stsh-job-list.h"
#include "stsh-job.h"
#include "stsh-process.h"
#include <cstring>
#include <iostream>
#include <string>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>  // for fork
#include <signal.h>  // for kill
#include <sys/wait.h>
#include <sys/types.h> // added by zgoz
using namespace std;

static STSHJobList joblist; // the one piece of global data we need so signal handlers can access it
static const unsigned int PIPE_READ_END = 0;
static const unsigned int PIPE_WRITE_END = 1;

/**
 * SIGCHILD toggle functions
 * --------------------------
 * Functions used to block/unblock SIGCHILD signals. (stolen from lecture code teehee)
 */
static void toggleSIGCHLDBlock(int how) {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(how, &mask, NULL);
}

static void blockSIGCHLD() {
  toggleSIGCHLDBlock(SIG_BLOCK);
}

static void unblockSIGCHLD() {
  toggleSIGCHLDBlock(SIG_UNBLOCK);
}

/**
 * Function: waitForFg
 * -----------------------
 * Private helper function that waits for a process to finish executing. Process is set to kRunning and
 * job is set to kForeground. If process doens't exist, just returns. Assumes blockSIGCHILD() is called
 * prior.
 *
 * @param pid: pid of the process to wait in foreground for
 */
static void waitForFg(pid_t pid) {
  sigset_t empty;
  sigemptyset(&empty);

  if (joblist.containsProcess(pid)){
    STSHJob& job = joblist.getJobWithProcess(pid);
    STSHProcess& process = job.getProcess(pid);

    process.setState(kRunning);
    job.setState(kForeground);
    joblist.synchronize(job);

    while (joblist.hasForegroundJob()) {
      sigsuspend(&empty);
    }
  }

  unblockSIGCHLD();
}

/**
 * Function: getSignalString
 * -----------------------
 * Private helper function used in `slay_halt_cont`. Function returns the builtin command name as a string depending
 * on the signal passed in.
 *
 * @param signal: SIGKILL | SIGTSTP | SIGCONT
 * @return string
 */
static std::string getSignalString(int signal) {
  switch (signal) {
    case SIGKILL:
      return "slay";
    case SIGTSTP:
      return "halt";
    case SIGCONT:
      return "cont";
    default:
      return "";
  }
}

/**
 * Function: fg_bg
 * -----------------------
 * Private helper function that executes either builtin commands `fg` or `bg` depending on the param `isFg`.
 * @param tokens: executing command's tokens 
 * @param isFg:   true for `fg`, false for `bg`
 */
static void fg_bg(char* const* tokens, bool isFg) {
  if (tokens[0] == NULL || tokens[1] != NULL) {
    throw (isFg) ? STSHException("Usage: fg <jobid>.") : STSHException("Usage: bg <jobid>.");
  }

  size_t endpos;
  long long num;

  try {
    num = stoll(tokens[0], &endpos);
  } catch (...) {
    throw (isFg) ? STSHException("Usage: fg <jobid>."): STSHException("Usage: bg <jobid>.");
  }

  if (!joblist.containsJob(num)) {
    throw (isFg) ? STSHException("fg " + std::to_string(num) + ": No such job.") : 
      STSHException("bg " + std::to_string(num) + ": No such job.");
  }

  STSHJob& job = joblist.getJob(num);
  pid_t pid = job.getGroupID();
  kill(-pid, SIGCONT);

  if (isFg) {
    blockSIGCHLD();
    waitForFg(pid);
  }
}

/**
 * Function: slay_halt_cont
 * -----------------------
 * Private helper function that executes either builtin commands `slay`, `halt`, or `cont` depending on the signal
 * parameter. Since most of the "setup" code is the same, they share this function only differentiating with the
 * signal that gets sent in kill(). Calls `getSignalString()` for matching builtin name for string when throwing
 * exceptions.
 *
 * @param tokens: executing command's tokens 
 * @param signal: SIGKILL | SIGTSTP | SIGCONT
 */
static void slay_halt_cont(char* const* tokens, int signal) {
  if (tokens[0] == NULL || tokens[2] != NULL) {
    throw STSHException("Usage: " + getSignalString(signal) + " <jobid> <index> | <pid>.");
  }

  size_t endpos;
  long long num;
  long long num2;

  // Two Args: <jobid> <index>
  if (tokens[1] != NULL) {
    try {
      num = stoll(tokens[0], &endpos);
      num2 = stoll(tokens[1], &endpos);
    } catch (...) {
      throw STSHException("Usage: " + getSignalString(signal) + " <jobid> <index> | <pid>.");
    }

    if (!joblist.containsJob(num)) {
      throw STSHException("No job with id of " + std::to_string(num) + ".");
    }

    blockSIGCHLD();
    STSHJob& job = joblist.getJob(num);
    std::vector<STSHProcess>& processes = job.getProcesses();

    if (num2 >= processes.size()) {
      throw STSHException("Job " + std::to_string(job.getNum()) + " doesn't have a process at index " + std::to_string(num2) + ".");
    }

    STSHProcess& process = processes.at(num2);
    kill(process.getID(), signal);
    unblockSIGCHLD();
    return;
  }

  // One Arg: <pid>
  try {
    num = stoll(tokens[0], &endpos);
  } catch (...) {
    throw STSHException("Usage: " + getSignalString(signal) + " <jobid> <index> | <pid>.");
  }

  if (!joblist.containsProcess(num)) {
    throw STSHException("No process with pid " + std::to_string(num) + ".");
  }

  blockSIGCHLD();
  kill(num, signal);
  unblockSIGCHLD();
}

/**
 * Function: handleBuiltin
 * -----------------------
 * Examines the leading command of the provided pipeline to see if
 * it's a shell builtin, and if so, handles and executes it.  handleBuiltin
 * returns true if the command is a builtin, and false otherwise.
 */
static const string kSupportedBuiltins[] = {"quit", "exit", "fg", "bg", "slay", "halt", "cont", "jobs"};
static const size_t kNumSupportedBuiltins = sizeof(kSupportedBuiltins)/sizeof(kSupportedBuiltins[0]);
static bool handleBuiltin(const pipeline& pipeline) {
  const string& command = pipeline.commands[0].command;
  auto iter = find(kSupportedBuiltins, kSupportedBuiltins + kNumSupportedBuiltins, command);
  if (iter == kSupportedBuiltins + kNumSupportedBuiltins) return false;
  size_t index = iter - kSupportedBuiltins;

  switch (index) {
  case 0:
  case 1: exit(0);

  // fg
  case 2:
    fg_bg(pipeline.commands[0].tokens, true); // true means command is fg
    break;

  // bg
  case 3:
    fg_bg(pipeline.commands[0].tokens, false); // false means command is NOT fg and is bg
    break;

  // slay
  case 4:
    slay_halt_cont(pipeline.commands[0].tokens, SIGKILL);
    break;

  // halt
  case 5:
    slay_halt_cont(pipeline.commands[0].tokens, SIGTSTP);
    break;

  // cont
  case 6:
    slay_halt_cont(pipeline.commands[0].tokens, SIGCONT);
    break;

  case 7: cout << joblist; break;
  default: throw STSHException("Internal Error: Builtin command not supported.");
  }

  return true;
}

/**
 * Function: installSignalHandlers
 * -------------------------------
 * Installs user-defined signals handlers for four signals
 * (once you've implemented signal handlers for SIGCHLD, 
 * SIGINT, and SIGTSTP, you'll add more installSignalHandler calls) and 
 * ignores two others.
 *
 * installSignalHandler is a wrapper around a more robust version of the
 * signal function we've been using all quarter.  Check out stsh-signal.cc
 * to see how it works.
 */
static void installSignalHandlers() {
  installSignalHandler(SIGQUIT, [](int sig) { exit(0); });
  installSignalHandler(SIGTTIN, SIG_IGN);
  installSignalHandler(SIGTTOU, SIG_IGN);
}

/**
 * Function: reaper
 * -------------------------------
 * Child handler function. Function is called everytime a child process changes it's
 * status to stopped, continued, or terminated.
 */
static void reaper(int sig) {
  pid_t pid;
  int status;
  while (true) {
    pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);

    if (pid <= 0){
      break;
    } else {
      if (!joblist.containsProcess(pid)) {
        throw STSHException("Joblist doesn't contain pid: " + pid);
        return;
      }

      STSHJob& job = joblist.getJobWithProcess(pid);

      if (!job.containsProcess(pid)) {
        throw STSHException("Job doesn't contain pid: " + pid);
        return;
      }

      STSHProcess& process = job.getProcess(pid);

      if (WIFEXITED(status) || WIFSIGNALED(status)) {
        process.setState(kTerminated);
      }

      if (WIFCONTINUED(status)) {
        process.setState(kRunning); 
      }

      if (WIFSTOPPED(status)) {
        process.setState(kStopped);
      }

      joblist.synchronize(job);
    }
 
  }
}

// triggered with Ctrl-C. sends SIGINT to grouppid
static void ctrlC(int sig) {
  if (joblist.hasForegroundJob()) {
    STSHJob& job = joblist.getForegroundJob();
    pid_t pid = job.getGroupID();
    kill(-pid, SIGINT);
  } 
}

// triggered with Ctrl-Z. sends SIGSTP to grouppid
static void ctrlZ(int sig) {
  if (joblist.hasForegroundJob()) {
    STSHJob& job = joblist.getForegroundJob();
    pid_t pid = job.getGroupID();
    kill(-pid, SIGTSTP);
  }
}

// close all pipes in pipes array
static void closePipes(size_t size, int pipes[][2]) {
  for (size_t i = 0; i < size; i++) {
    close(pipes[i][PIPE_READ_END]);
    close(pipes[i][PIPE_WRITE_END]);
  }
}

/**
 * Function: getExecvpArgs
 * -------------------
 * Helper function that setsup the arguments for execvp
 */
static void getExecvpArgs(char* args[], const command& c) {
  args[0] = (char*)c.command;

  size_t j = 0;
  for (j = 0; j <= kMaxArguments && c.tokens[j] != NULL; j++) {
    args[j+1] = c.tokens[j];
  }
  args[j+1] = NULL;
}

/**
 * Function: setup_io_pipeline
 * -------------------
 * Setsup File I/O direction and piping for the given command at index commandIndex.
 * @return bool: only false if input file descriptor is 0(input file wasn't found), true otherwise
 */
static bool setup_io_pipeline(size_t amountOfPipes, int pipes[][2], int commandIndex, int inputFd, int outputFd) {

  // Input Redirection
  if (inputFd != 0 && commandIndex == 0) {
    if (inputFd < 0) {
      return false;
    }
    dup2(inputFd, STDIN_FILENO);
    close(inputFd);
  }

  // Output Redirection
  if (outputFd != 0 && (amountOfPipes == 0 || commandIndex > 0)) {
    dup2(outputFd, STDOUT_FILENO);
    close(outputFd);
  }

  // First Command
  if (amountOfPipes > 0 && commandIndex == 0) {
    dup2(pipes[commandIndex][PIPE_WRITE_END], STDOUT_FILENO);
    closePipes(amountOfPipes, pipes);
  }
  // Final Command
  else if (amountOfPipes > 0 && commandIndex == amountOfPipes) {
    dup2(pipes[commandIndex-1][PIPE_READ_END], STDIN_FILENO);
    closePipes(amountOfPipes, pipes);
  }
  // Middle Commands
  else if (amountOfPipes > 0) {
    dup2(pipes[commandIndex-1][PIPE_READ_END], STDIN_FILENO);
    dup2(pipes[commandIndex][PIPE_WRITE_END], STDOUT_FILENO);
    closePipes(amountOfPipes, pipes);
  }
  return true;
}

/**
 * Function: createJob
 * -------------------
 * Creates a new job on behalf of the provided pipeline.
 */
static void createJob(const pipeline& p) {
  STSHJob& job = joblist.addJob(kForeground);

  // Piping Setup
  bool firstCommand = true; // Used to check if command is first in pipeline or not
  pid_t groupID = 0;        // Leading process pid
  size_t amountOfPipes = p.commands.size() - 1; // amount of pipes needed
  int pipes[amountOfPipes][2];
  
  for (size_t i = 0; i < amountOfPipes; i++) {
    pipe2(pipes[i], O_CLOEXEC);
  }

  // File IO Redirection Setup
  int inputFd = 0;
  int outputFd = 0;

  if (!p.input.empty()) {
    inputFd = open(p.input.c_str(), O_RDONLY);
  }

  if (!p.output.empty()) {
    outputFd = open(p.output.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  }

  // Begin execution of commands
  for (size_t i = 0; i < p.commands.size(); i++) {
    char* args[kMaxArguments + 2];
    getExecvpArgs(args, p.commands[i]); // Populates args for execvp in CHILD

    blockSIGCHLD();
    pid_t pid = fork();

    // Parent
    if (pid != 0) {
      STSHProcess process(pid, p.commands[i]);
      job.addProcess(process);

      // First process's pid becomes groupID for all processes
      if (firstCommand) {
        setpgid(pid, 0);
        groupID = pid;
        firstCommand = false;
      }
      else {
        setpgid(pid, groupID);
      }
    }

    // CHILD
    if (pid == 0) {
      // First process's pid becomes groupID for all processes (here to avoid race conditions)
      if (firstCommand) {
        setpgid(getpid(), 0);
        groupID = getpid();
      }
      else {
        setpgid(getpid(), groupID);
      }
      unblockSIGCHLD();

      if (!setup_io_pipeline(amountOfPipes, pipes, i, inputFd, outputFd)) {
        throw STSHException("Could not open \"" + p.input + "\".");
      }

      // Only hand off terminal if not performing input redirection
      if (!p.background && inputFd == 0){
        int returnValue = tcsetpgrp(STDIN_FILENO, getpgid(getpid()));

        if (returnValue == -1 && errno != ENOTTY) {
          throw STSHException("tcsetpgrp control was not handed off correctly");
        }
      }

      firstCommand = false;
      execvp(args[0], args);
      cout << p.commands[i].command << ": Command not found." << endl;
      exit(-1);
    }

    unblockSIGCHLD();
  }

  closePipes(amountOfPipes, pipes);

  if (joblist.containsProcess(job.getGroupID())) {
    blockSIGCHLD();

    if (!p.background){
      waitForFg(job.getGroupID());
      int returnValue = tcsetpgrp(STDIN_FILENO, getpid());

      if (returnValue != 0 && errno != ENOTTY) {
        throw STSHException("tcsetpgrp control was not handed off correctly");
      }
    } else {
      job.setState(kBackground);
      cout << "[" << job.getNum() << "]";
      std::vector<STSHProcess>& jobP = job.getProcesses();
      for (size_t i = 0; i < jobP.size(); i++) {
        cout << " " << jobP.at(i).getID();
      }
      cout << endl;
      unblockSIGCHLD();
    }

  }

  joblist.synchronize(job);
}

/**
 * Function: main
 * --------------
 * Defines the entry point for a process running stsh.
 * The main function is little more than a read-eval-print
 * loop (i.e. a repl).  
 */
int main(int argc, char *argv[]) {
  signal(SIGCHLD, reaper); // child reaper
  signal(SIGINT, ctrlC);   // handle that Ctrl-C button
  signal(SIGTSTP, ctrlZ);  // handle that Ctrl-Z button

  pid_t stshpid = getpid();
  installSignalHandlers();
  rlinit(argc, argv);
  while (true) {
    string line;
    if (!readline(line)) break;
    if (line.empty()) continue;
    try {
      pipeline p(line);
      bool builtin = handleBuiltin(p);
      if (!builtin) createJob(p);
    } catch (const STSHException& e) {
      cerr << e.what() << endl;
      if (getpid() != stshpid) exit(0); // if exception is thrown from child process, kill it
    }
  }

  return 0;
}