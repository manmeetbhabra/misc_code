/**
 * File: semaphore.cc
 * ---------------------
 * Implementation of methods for the Semaphore class
 */

#include "semaphore.h"

#include <boost/thread/thread.hpp>

void Semaphore::wait()
{
  // Lock the mutex before checking on the status on the member
  // variable value_
  mtx_.lock();

  // The condition_variable_any atomically puts the thread to sleep and
  // unlocks the mutex so other threads can use it. When a signal comes
  // in, we wake up and only return if we grabbed the mutex (which we
  // call lock() on) and the lambda function is true.
  cv_.wait(mtx_, [this]{return value_ > 0;});
  
  // Decrement the number of available permits since we got one. Recall
  // that we are safe in modifying value_ here since the mutex is locked.
  value_--;

  // We are done updating the permit so unlock the mutex
  mtx_.unlock();
}

void Semaphore::signal()
{
  // Lock the mutex before playing with the member variable value_
  mtx_.lock();

  // Return one of the permits
  value_++;

  // A permit is now available, so notify the threads
  if(value_ == 1)
  {
    cv_.notify_all();
  }

  // We are done working with value_, so unlock the mutex
  mtx_.unlock();
}

void Semaphore::signal(signal_condition)
{
  boost::this_thread::at_thread_exit([this] {
      this->signal();
  });
}