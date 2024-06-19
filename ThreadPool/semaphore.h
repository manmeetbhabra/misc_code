/**
 * File: semaphore.h
 * ---------------------
 * An implementation of a semaphore using mutexes and conditional variables
 */

#pragma once

#include <mutex>
#include <condition_variable>

/**
 * Enum for specifying we want the condition variable to signal on the thread's exit
 */
enum signal_condition {
    on_thread_exit,
};

class Semaphore
{
  public:

    /**
     * Constructor
     * 
     * \param value   The total number of permits to use.
     */
    Semaphore(int value = 0) : value_(value){}

    /**
     * Wait until a permit is available before proceeding.
     */
    void wait();

    /**
     * Signal when a permit is now available.
     */
    void signal();

    /**
     * Overloaded function for signaling on thread exit. 
     */
    void signal(signal_condition);

  private:

    /**
     * Variable storing the number of permits currently available.
     */
    int value_;

    /**
     * Member variable for the mutex for the semaphore.
     */
    std::mutex mtx_;

    /**
     * The condition variable used for signaling between threads. 
     */
    std::condition_variable_any cv_;
};
