/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include <iostream>
#include <unistd.h>
#include "ostreamlock.h"
#include "thread-pool.h"
using namespace std;

size_t ThreadPool::getAvailableID() {
  for (size_t i = 0; i < numThreads; i++) {
    if (worker_structs[i].available)
      return i;
  }
  return -1;
}

bool ThreadPool::threadsFinished() {
  for (size_t i = 0; i < numThreads; i++) {
    if (!worker_structs[i].available)
      return false;
  }
  return true;
}

void ThreadPool::incrementAvailableWorkers(size_t workerID) {
  permits.wait(); // Wait until thread signals it's ready for execution
  worker_structs[workerID].id = workerID;
  worker_structs[workerID].available = true;
  worker_structs[workerID].startExecution = false;
}

ThreadPool::ThreadPool(size_t numThreadsIn) : 
          numThreads(numThreadsIn), wts(numThreadsIn), killProgram(false), permits(numThreadsIn), worker_structs(numThreadsIn),
          worker_mutexs(numThreadsIn), worker_cvs(numThreadsIn) {

  dt = thread([this]() {
    dispatcher();
  });

  for (size_t workerID = 0; workerID < numThreads; workerID++) {
    wts[workerID] = thread([this](size_t workerID) {
      worker(workerID);
    }, workerID);
    incrementAvailableWorkers(workerID);
  }
}

void ThreadPool::dispatcher() {
  while (!killProgram) {
    thunksLock.lock();
    cv_thunks.wait(thunksLock, [&](){ return !thunks.empty() || killProgram;});

    if (killProgram) {
      thunksLock.unlock();
      break;
    }

    function<void(void)> workingThunk = thunks.front();
    thunks.pop_front();
    cv_thunks.notify_all();

    permits.wait(); // Wait for available Thread

    size_t wid = getAvailableID();

    // This should never be true, just felt wrong not putting it in here
    if (wid < 0) {
      throw("getAvailableID: no available threads - bad implementation");
    }

    worker_mutexs[wid].lock();

    worker_structs[wid].available = false;
    worker_structs[wid].thunker = workingThunk;
    worker_structs[wid].startExecution = true;

    worker_mutexs[wid].unlock();
    thunksLock.unlock();

    worker_cvs[wid].notify_all(); // Notify thread to begin
  }
}

void ThreadPool::worker(size_t workerID) {
  permits.signal(); // Signal that this thread is created and ready to execute

  while (!killProgram) {
    worker_mutexs[workerID].lock();
    worker_cvs[workerID].wait(worker_mutexs[workerID], [&](){
      return worker_structs[workerID].startExecution || killProgram;
    });
    worker_mutexs[workerID].unlock();

    if (killProgram) {
      break;
    }

    try {
        worker_structs[workerID].thunker();
    } catch (bad_function_call& e) {
      // Hopefully never hits... but just in case :)
      cout << oslock << "Bad implementation on my part..." << "workID: " << workerID << endl << osunlock;
    }

    worker_structs[workerID].available = true;
    worker_structs[workerID].startExecution = false;
    permits.signal();

    threadsDoneLock.lock();
    cv_threadsDone.notify_all(); // Notify wait() to check if all threads finished
    threadsDoneLock.unlock();
  }
}

void ThreadPool::schedule(const function<void(void)>& thunk) {
  lock_guard<mutex> lg(thunksLock);
  thunks.push_back(thunk);
  cv_thunks.notify_all();
}

void ThreadPool::wait() {
  thunksLock.lock();
  cv_thunks.wait(thunksLock, [&](){ return thunks.empty();});
  thunksLock.unlock();

  threadsDoneLock.lock();
  cv_threadsDone.wait(threadsDoneLock, [&](){ return threadsFinished(); });
  threadsDoneLock.unlock();
}

ThreadPool::~ThreadPool() {
  wait();

  for (size_t workerID = 0; workerID < numThreads; workerID++) {
    permits.wait();
  }

  // Signals all threads to exit
  killProgram = true;
  thunksLock.lock();
  cv_thunks.notify_all();
  thunksLock.unlock();

  dt.join();

  for (size_t workerID = 0; workerID < numThreads; workerID++) {
    worker_mutexs[workerID].lock();
    worker_cvs[workerID].notify_all();
    worker_mutexs[workerID].unlock();
  }

  for (size_t workerID = 0; workerID < numThreads; workerID++) {
    wts[workerID].join();
  }
}
