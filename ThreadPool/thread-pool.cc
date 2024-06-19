/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"

#include <chrono>
#include <stdexcept>
#include <cassert>

ThreadPool::ThreadPool(size_t numThreads) : 
  dispatcher_binary_spr_(0),
  worker_threads_spr_(numThreads),
  worker_threads_availability_(numThreads, true),
  worker_threads_jobs_(numThreads), 
  tp_wait_executed_(false)
{
  // Create the worker thread signaling semaphores
  for(std::size_t i = 0; i < numThreads; ++i)
  {
    worker_threads_binary_spr_.push_back(std::make_unique<Semaphore>(0));
  }

  // Start the dispatcher thread
  dispatcher_thread_ = std::thread(
    [this]{
      this->dispatcher();
    }
  );
}

void ThreadPool::schedule(const std::function<void(void)>& thunk) 
{
  // Add the new thunk to the queue
  tp_func_queue_mtx_.lock();
  tp_func_queue_.push(thunk);
  tp_func_queue_mtx_.unlock();

  dispatcher_binary_spr_.signal();
}

void ThreadPool::wait() 
{
  tp_wait_executed_ = true;

  // Signal to the dispatcher that the wait() method has been called and
  // that it should exit once all its jobs have been scheduled.
  dispatcher_binary_spr_.signal();
  dispatcher_thread_.join();

  // Once we have reached here, we are certain that the dispatcher thread has
  // finished running and thus all jobs have been allocated to worker threads.
  // That is, we are sure that no new jobs can be allocated to any workers.
  // To stop each worker, signal each worker thread an extra time using the 
  // binary semaphore (but now with the tp_wait_executed_ set to True) and 
  // wait for all worker threads to join.
  for (std::size_t i = 0; i < worker_threads_.size(); ++i)
  {
    worker_threads_binary_spr_.at(i)->signal();
    worker_threads_.at(i).join();
  }
}

void ThreadPool::dispatcher()
{
  while(true)
  {
    // Wait until a signal comes in about new jobs being available
    dispatcher_binary_spr_.wait();

    if (tp_wait_executed_ && tp_func_queue_.empty())
    {
      // Stop looping if the wait method has executed.
      //
      // We use a Semaphore with 0 permits to signal the dispather. Say that
      // N jobs were scheduled. There would then be N total signal calls to the
      // Semaphore. Let's say that all these signals happened together and
      // then the wait was called (so no job was run before the wait() call). Then,
      // in this case, the value in the Semaphore would be N+1 (since we have
      // a signal in the wait method). In this case, we would go through the
      // loop N times still (to keep decrementing the value_). Then, we would go
      // through it the last (N+1)th time and this time the queue would be empty
      // with the tp_wait_executed_ being true, so we will exit the
      // dispather loop.
      // 
      // Note that we will block on the join call in wait so we are sure 
      // that no other schedule method calls could happen.
      break;
    }

    // A new job is available to be scheduled to a worker thread. Wait
    // for a worker thread to be available.
    worker_threads_spr_.wait();

    // A worker thread is available. Find the index of the first available one
    int available_worker_idx = -1;
    worker_threads_availability_mtx_.lock();
    for (std::size_t i = 0; i < worker_threads_availability_.size(); ++i)
    {
      if (worker_threads_availability_.at(i))
      {
        available_worker_idx = i;
        break;
      }
    }
    worker_threads_availability_mtx_.unlock();

    assert(available_worker_idx >= 0);

    if ((available_worker_idx+1) > worker_threads_.size())
    {
      // If we need a new thread to start running, then start that thread.
      // Based on the structure of the dispatcher, there should be one new thread
      // to start if we need to start one.
      assert((available_worker_idx + 1) - worker_threads_.size() == 1);

      // Start the new thread
      worker_threads_.push_back(
        std::thread(
          [this](std::size_t workerID)
          {
            this->worker(workerID);
          }, 
          available_worker_idx
        )
      );
    }

    // Add the job for the worker thread to run
    tp_func_queue_mtx_.lock();
    worker_threads_jobs_.at(available_worker_idx) = 
      std::make_shared<std::function<void(void)>>(tp_func_queue_.front());
    tp_func_queue_.pop();
    tp_func_queue_mtx_.unlock();

    // Set this worker thread as not being available
    worker_threads_availability_mtx_.lock();
    worker_threads_availability_.at(available_worker_idx) = false;
    worker_threads_availability_mtx_.unlock();

    // Signal to the worker thread that it has a new job to execute
    worker_threads_binary_spr_.at(available_worker_idx)->signal();
  }
}

void ThreadPool::worker(std::size_t workerID)
{
  while(true)
  {
    // Wait until a signal comes in about new job to run
    worker_threads_binary_spr_.at(workerID)->wait();

    if (tp_wait_executed_ && worker_threads_jobs_.at(workerID) == nullptr)
    {
      // The worker thread has been signaled. Moreover, the worker has no
      // job to run and we know the wait() method has been executed. So
      // stop running this worker thread.
      break;
    }

    // A new job is available, so grab it and run it
    (*worker_threads_jobs_.at(workerID))();

    // Remove the job that we've finished running
    worker_threads_jobs_.at(workerID).reset();

    // The job has finished. Set this worker thread as being available again
    worker_threads_availability_mtx_.lock();
    worker_threads_availability_.at(workerID) = true;
    worker_threads_availability_mtx_.unlock();

    // Signal that a new permit for a worker thread is available now
    worker_threads_spr_.signal();
  }
}
