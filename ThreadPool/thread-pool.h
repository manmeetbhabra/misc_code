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
#include <thread>
#include <queue>
#include <memory>
#include <mutex>
#include <vector>

#include "semaphore.h"

class ThreadPool 
{

public:

  /**
   * Constructs a ThreadPool configured to spawn up to the specified
   * number of threads.
   */
  ThreadPool(size_t numThreads);

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
  
private:

  /**
   * Method used by the dispatcher thread for sending scheduled 
   * jobs to idle workers. The dispatcher is running this method at
   * all times on a loop.
   */
  void dispatcher();

  /**
   * The primary method used by worker threads. The worker threads are
   * running this method on a loop.
   */
  void worker(std::size_t workerID);

  /**
   * The dispatcher thread that is used for sending jobs that have been
   * passed in to worker threads.
   */
  std::thread dispatcher_thread_;

  /**
   * The thread pool (tp) function (func) queue. This holds all the
   * different scheduled functions that we want worker threads in our
   * pool to eventually execute.
   */
  std::queue<std::function<void(void)>> tp_func_queue_;

  /**
   * Mutex for protecting modifying the tp_func_queue_ member variable.
   */
  std::mutex tp_func_queue_mtx_;

  /**
   * The binary coordination semaphore used for signaling to the dispatcher
   * that new functions are in the queue for dispatching to worker threads. 
   */
  Semaphore dispatcher_binary_spr_;

  /**
   * Vector holding the different worker threads part of the thread pool.
   */
  std::vector<std::thread> worker_threads_;

  /**
   * Semaphore that the dispatcher thread uses for waiting on the worker
   * threads. That is, we will use this semaphore to know when worker
   * threads are done executing their scheduled function.
   */
  Semaphore worker_threads_spr_;

  /**
   * Vector of booleans storing the status of the different worker threads.
   * If worker thread i is available, then the boolean at the index i will
   * be true.
   */
  std::vector<bool> worker_threads_availability_;

  /**
   * Mutex for protecting writing/reading from the worker_threads_availability_ 
   * vector.
   */
  std::mutex worker_threads_availability_mtx_;
  
  /**
   * Vector to hold functions (jobs) for the worker threads to run.
   */
  std::vector<std::shared_ptr<std::function<void(void)>>> worker_threads_jobs_;

  /**
   * A semaphore for each worker thread. This signal is used by a worker thread
   * to know that it has a new job that has been scheduled for it to do.
   */
  std::vector<std::unique_ptr<Semaphore>> worker_threads_binary_spr_;

  /**
   * Boolean to specify that the wait method of the thread pool (tp) has been executed.
   */
  bool tp_wait_executed_;

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
