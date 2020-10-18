#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor {

public:

    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no new task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadpool is stopped
        kStopped
    };

    Executor(std::string name, int size) : name(name), size(size), n_active(0), state(State::kRun) {	

        std::lock_guard <std::mutex> lock(mutex);
    	for (int i = 0; i < size; ++i) {
        	std::thread(&Afina::Concurrency::Executor::perform, this).detach();
    	}
    }

    ~Executor() { 
    	Stop(true);
    }

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false) {
    	std::unique_lock <std::mutex> lock(mutex);
    	if (state == State::kStopped) {
        	return;
    	}
    	state = State::kStopping;
    	empty_condition.notify_all();

    	if (await) {
        	while (state != State::kStopped) {
            	empty_condition.wait(lock);
        	}
    	} else if (n_active == 0) {
        	state = State::kStopped;
    	}

    }

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::lock_guard<std::mutex> lock(mutex);
        if (state != State::kRun) {
            return false;
        }

        // Enqueue new task
        tasks.push_back(exec);
        empty_condition.notify_one();
        return true;
    }

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
	void perform() {
    	while (true) {
        	std::unique_lock<std::mutex> lock(mutex);

        	if (tasks.empty()) {

        		//for threads which were executing a task
        		//when Stop was called
        		if (state == State::kStopping) {
        	    	if (n_active == 0) {
            	    	state = State::kStopped;
        	    	    empty_condition.notify_one();
    	        	}
	            	return;
        		}

	        	empty_condition.wait(lock);

        		//for threads which were sleeping
        		//when Stop was called
        		if (state == State::kStopping) {
        	    	if (n_active == 0) {
            	    	state = State::kStopped;
        	    	    empty_condition.notify_one();
    	        	}
	            	return;
        		}
    		}

           	auto task = tasks.front();
            tasks.pop_front();
            n_active++;
            lock.unlock();
            try {
               	task();
            } catch (std::runtime_error& ex) {
               	throw std::runtime_error(std::string("Error while executing task: ") + std::string(ex.what()) + std::string("\n"));
            }
            lock.lock();
            n_active--;
    	}
    }


    std::string name;

    std::size_t size;
    std::size_t n_active;

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
