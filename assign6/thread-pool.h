/**
 * File: thread-pool.h
 * -------------------
 * This class defines the ThreadPool class, which accepts a collection
 * of thunks (which are zero-argument functions that don't return a value)
 * and schedules them in a FIFO manner to be executed by a constant number
 * of child threads that exist solely to invoke previously scheduled thunks.
 */

#ifndef _thread_pool_
#define _thread_pool_

#include <cstddef>     // for size_t
#include <functional>  // for the function template used in the schedule signature
#include <thread>      // for thread
#include <vector>      // for vector
// zgoz introduces these
#include <list>		   // for list
#include <mutex>
#include <condition_variable>
#include <map>
#include "semaphore.h"

struct workStruct {
  workStruct() : id(0), available(false), startExecution(false) {}
  workStruct(size_t workerID) : id(workerID), available(false), startExecution(false) {}
  size_t id;
  bool available;
  bool startExecution;
  std::function<void(void)> thunker; // like thumper from bambi
};

class ThreadPool {
 public:

/**
 * Constructs a ThreadPool configured to spawn up to the specified
 * number of threads.
 */
  ThreadPool(size_t numThreadsIn);

/**
 * Schedules the provided thunk (which is something that can
 * be invoked as a zero-argument function without a return value)
 * to be executed by one of the ThreadPool's threads as soon as
 * all previously scheduled thunks have been handled.
 */
  void schedule(const std::function<void(void)>& thunk);

/**
 * Blocks and waits until all previously scheduled thunks
 * have been executed in full.
 */
  void wait();

/**
 * Waits for all previously scheduled thunks to execute, and then
 * properly brings down the ThreadPool and any resources tapped
 * over the course of its lifetime.
 */
  ~ThreadPool();
  
 private:
  size_t numThreads;
  std::thread dt;                // dispatcher thread handle
  std::vector<std::thread> wts;  // worker thread handles
  bool killProgram;              // Becomes TRUE when desctructor is called. Kills all threads
  semaphore permits;             // Permits for thread availability

  /* Structs and thread-safety variables for each worker thread (at index workerID) */
  std::vector<struct workStruct> worker_structs;
  std::vector<std::mutex> worker_mutexs;
  std::vector<std::condition_variable_any> worker_cvs;

  /* Thunk queue and thread-safety variables */
  std::list<std::function<void(void)>> thunks;
  std::mutex thunksLock;
  std::condition_variable_any cv_thunks;

  /* Thread-safety variables for keeping track of when all threads complete */
  std::mutex threadsDoneLock;
  std::condition_variable_any cv_threadsDone;

  /* True if all threads are available (i.e. done with execution) */
  bool threadsFinished();

  /* Returns workerID of first available thread */
  size_t getAvailableID();

  void dispatcher();
  void worker(size_t workerID);

  /* Populates workerID's struct */
  void incrementAvailableWorkers(size_t workerID);

/**
 * ThreadPools are the type of thing that shouldn't be cloneable, since it's
 * not clear what it means to clone a ThreadPool (should copies of all outstanding
 * functions to be executed be copied?).
 *
 * In order to prevent cloning, we remove the copy constructor and the
 * assignment operator.  By doing so, the compiler will ensure we never clone
 * a ThreadPool.
 */
  ThreadPool(const ThreadPool& original) = delete;
  ThreadPool& operator=(const ThreadPool& rhs) = delete;
};

#endif
