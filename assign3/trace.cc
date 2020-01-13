/**
 * File: trace.cc
 * ----------------
 * Presents the implementation of the trace program, which traces the execution of another
 * program and prints out information about ever single system call it makes.  For each system call,
 * trace prints:
 *
 *    + the name of the system call,
 *    + the values of all of its arguments, and
 *    + the system calls return value
 */

#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <unistd.h> // for fork, execvp
#include <string.h> // for memchr, strerror
#include <errno.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include "trace-options.h"
#include "trace-error-constants.h"
#include "trace-system-calls.h"
#include "trace-exception.h"
using namespace std;

static const int kTerminateLoop = 0;
static const int kContinueLoop = 1;
static const int kRegisterValues[] = { RDI, RSI, RDX, R10, R8, R9 }; // Used in printSyscall for registers

/**
 * readString
 * ---------------------------
 * Helper function to read the string given at a certain address.
 */
static string readString(pid_t pid, unsigned long addr) {
  string str;
  bool getOut = false;
  size_t numBytesRead = 0;
  while (true) {
    long ret = ptrace(PTRACE_PEEKDATA, pid, addr + numBytesRead);
    char* buff;
    buff = (char*)&ret;

    for (size_t i = 0; i < (size_t)sizeof(long); i++) {
      if (buff[i] == '\0') {
        getOut = true;
        break;
      }
      str += buff[i];
    }

    if (getOut) {
      break;
    }
    numBytesRead += sizeof(long);
  }
  return str;
}

/**
 * printSyscall
 * ---------------------------
 * Used for printing the full version of the syscall.
 */
void printSyscall(pid_t pid, bool* brkOrMmap,
        std::map<int, std::string>& systemCallNumbers,std::map<std::string, systemCallSignature>& systemCallSignatures) {
    int syscallNum = ptrace(PTRACE_PEEKUSER, pid, ORIG_RAX * sizeof(long));

    string syscallStr = systemCallNumbers[syscallNum];

    if (syscallStr == "brk" || syscallStr == "mmap") {
        *brkOrMmap = true;
    }

    cout << syscallStr << "(" << flush;

    size_t argumentCount = 0;
    vector<scParamType> arguments = systemCallSignatures[syscallStr];
    for (vector<scParamType>::iterator itr = arguments.begin(); itr != arguments.end(); itr++) {
        if (argumentCount > 0) {
            cout << ", " << flush;
        }

        if (*itr == SYSCALL_INTEGER) {
            int integer = ptrace(PTRACE_PEEKUSER, pid, kRegisterValues[argumentCount] * sizeof(long));
            cout << std::dec << integer << flush;
        } else if (*itr == SYSCALL_POINTER) {
            long pointer = ptrace(PTRACE_PEEKUSER, pid, kRegisterValues[argumentCount] * sizeof(long));
            if (pointer != 0) {
              cout << "0x" << std::hex << pointer;
            } else {
              cout << "NULL";
            }

        } else if (*itr == SYSCALL_STRING) {
            long stringAddress = ptrace(PTRACE_PEEKUSER, pid, kRegisterValues[argumentCount] * sizeof(long));
            cout << "\"" << readString(pid, stringAddress) << "\"";
        }
        ++argumentCount;
    }

    cout << ") = " << flush;
}

/**
 * printReturnValue
 * ---------------------------
 * Used for printing the full version of the return value.
 */
void printReturnValue(pid_t pid, bool* brkOrMmap, std::map<int, string>& errorConstants) {
  long returnValue = ptrace(PTRACE_PEEKUSER, pid, RAX * sizeof(long));

  if (returnValue < 0) {
    string errorString = errorConstants[abs(returnValue)];
    cout << "-1 " << errorString << " " << "(" << strerror(abs(returnValue)) << ")" << endl;
  } else if (*brkOrMmap) {
    cout << "0x" << std::hex << returnValue << endl;
    } else {
    cout << returnValue << endl;
  }
}

/**
 * fullLoop
 * ---------------------------
 * Used for when running trace on full mode. Prints the line as specified on the handout.
 */
int fullLoop(bool syscall, bool* brkOrMmap, pid_t pid, int* status,
    std::map<int, std::string>& systemCallNumbers, std::map<std::string, int>& systemCallNames,
    std::map<std::string, systemCallSignature>& systemCallSignatures, std::map<int, string>& errorConstants) {
  while (true) {
    ptrace(PTRACE_SYSCALL, pid, 0, 0);
    waitpid(pid, status, 0);

    // Process is ongoing
    if (WIFSTOPPED(*status) && (WSTOPSIG(*status) == (SIGTRAP | 0x80))) {

      if (syscall) {
        printSyscall(pid, brkOrMmap, systemCallNumbers, systemCallSignatures);
      } else {
        printReturnValue(pid, brkOrMmap, errorConstants);
      }
      return kContinueLoop;
    }

    // Process is ending
    if (WIFEXITED(*status)) {
      cout << "<no return>\n" << flush;
      return kTerminateLoop;
    }
  }
  return kContinueLoop;
}

/**
 * simpleLoop
 * ---------------------------
 * Used for when running trace on simple mode. Simple prints number values as sepcified on the handout.
 */
int simpleLoop(bool syscall, pid_t pid, int* status) {
  while (true) {
    ptrace(PTRACE_SYSCALL, pid, 0, 0);
    waitpid(pid, status, 0);

    // Process is ongoing
    if (WIFSTOPPED(*status) && (WSTOPSIG(*status) == (SIGTRAP | 0x80))) {
      if (syscall) {
        int syscallNum = ptrace(PTRACE_PEEKUSER, pid, ORIG_RAX * sizeof(long));
        cout << "syscall(" << syscallNum << ") = " << flush;
      } else {
        long returnValue = ptrace(PTRACE_PEEKUSER, pid, RAX * sizeof(long));
        cout << returnValue << endl;
      }
      return kContinueLoop;
    }

    // Process is ending
    if (WIFEXITED(*status)) {
      cout << "<no return>\n" << flush;
      return kTerminateLoop;
    }
  }
  return kContinueLoop;
}


int main(int argc, char *argv[]) {
  bool simple = false, rebuild = false;
  int numFlags = processCommandLineFlags(simple, rebuild, argv);
  if (argc - numFlags == 1) {
    cout << "Nothing to trace... exiting." << endl;
    return 0;
  }

  pid_t pid = fork();
  if (pid == 0) {
    ptrace(PTRACE_TRACEME);
    raise(SIGSTOP);
    execvp(argv[numFlags + 1], argv + numFlags + 1);
    return 0;
  }

  int status = 0;
  waitpid(pid, &status, 0);
  assert(WIFSTOPPED(status));
  ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);

  if (simple) {
    // Simple flag was turned on
    while (true) {
      // SysCall print
      if (simpleLoop(true, pid, &status) == kTerminateLoop) {
        break;
      }
      // Return Value Print
      if (simpleLoop(false, pid, &status) == kTerminateLoop) {
        break;
      }
    }
  } else {
    // Simple flag was left off
    std::map<int, std::string> systemCallNumbers;
    std::map<std::string, int> systemCallNames;
    std::map<std::string, systemCallSignature> systemCallSignatures;
    std::map<int, string> errorConstants;

    // populate the new maps
    compileSystemCallData(systemCallNumbers, systemCallNames, systemCallSignatures, rebuild);
    compileSystemCallErrorStrings(errorConstants);

    while (true) {
      bool brkOrMmap = false; // Used to specify if the call was brk or mmap since the parameters must be printed differently
      // SysCall print
      if (fullLoop(true, &brkOrMmap, pid, &status, systemCallNumbers, systemCallNames, systemCallSignatures, errorConstants) == kTerminateLoop) {
        break;
      }
      // Return Value Print
      if (fullLoop(false, &brkOrMmap, pid, &status, systemCallNumbers, systemCallNames, systemCallSignatures, errorConstants) == kTerminateLoop) {
        break;
      }
    }
  }

  cout << "Program exited normally with status " << WEXITSTATUS(status) << endl;
  return WEXITSTATUS(status);
}
