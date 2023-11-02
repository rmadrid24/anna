#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <chrono>
#include <atomic>
#include <tbb/concurrent_queue.h>

typedef std::shared_ptr<std::function<void()>> task_type;

struct Task {
	task_type t;
	long start;
	long in_ready_q;
	long in_flush_q;
	long out_ready_q;
	long out_flush_q;
	long end;
};

typedef struct {
	bool done;
} ThreadData;

typedef std::shared_ptr<Task> inner_task_type;

class ThreadPool {
	std::mutex t_mutex;
public:
	ThreadPool(std::string name, size_t threads);
	bool enqueue(task_type task);
	void threadTask(int rank);
	void threadReport();
	void addThread();
	void removeThread();
	long getQueueSize();
	int getThreadCount();
	~ThreadPool();
private:
	//need to keep track of threads so we can join them
 	void print_stats();
	std::vector< std::thread > workers;
	std::vector< ThreadData  > workers_data;
	std::thread reporter;
	std::mutex thread_index_mutex;
	std::atomic<int> thread_index{0};
	int num_threads_;
	// the task queue
	tbb::concurrent_queue< inner_task_type > tbb_tasks;
	bool stop;
	std::atomic<long> total_handle_time{0};
	std::atomic<long> total_enqueue_time{0};
	std::atomic<long> total_ops{0};
	std::atomic<long> total_queue_time{0};
	std::atomic<long> total_func_time{0};
	ThreadData getThreadDefaultData();
	cpu_set_t cpuset;
	std::string name_ = "";
};

//inline std::mutex ThreadPool::t_mutex;
inline long get_current_time() {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(std::string name, size_t threads)
    :   stop(false), name_(name)
{
	num_threads_ = threads;
	CPU_ZERO(&cpuset);
	for (int j = 0; j < 24; j++) {
		CPU_SET(j, &cpuset);
	};
	for (int k = 48; k < 72; k++) {
		CPU_SET(k, &cpuset);
	};
	for(size_t i = 0; i < threads; i++) {
		workers_data.push_back(getThreadDefaultData());
		workers.push_back(std::thread(&ThreadPool::threadTask, this, i));
	       	pthread_setaffinity_np(workers[i].native_handle(), sizeof(cpu_set_t), &cpuset);
	}
	//reporter = std::thread(&ThreadPool::threadReport, this);
}

inline void ThreadPool::addThread()
{
	//std::cout << "INCREASE: num threads " << num_threads_ << " workers size " << workers.size() << std::endl;
	if (num_threads_ == 32) return;
	workers_data.push_back(getThreadDefaultData());
	//std::cout << "queue size " << tbb_tasks.unsafe_size() << std::endl;
	workers.push_back(std::thread(&ThreadPool::threadTask, this, num_threads_));
	pthread_setaffinity_np(workers[num_threads_].native_handle(), sizeof(cpu_set_t), &cpuset);
	num_threads_ += 1;
	// pending to pin a thread to a specific cpu.
}

inline void ThreadPool::removeThread()
{
	//std::cout << "DECREASE: num threads " << num_threads_ << " workers size " << workers.size() << std::endl;
	if (num_threads_ == 1) return;
	ThreadData* td = &workers_data[num_threads_ - 1];
	std::thread* t = &workers[num_threads_ - 1];
	td->done = true;
	t->join();
	workers.pop_back();
	workers_data.pop_back();
	num_threads_ -= 1;
}

inline ThreadData ThreadPool::getThreadDefaultData()
{
	ThreadData td;
	td.done = false;
	return td;
}

inline void ThreadPool::threadReport() {
	while (!this->stop) {
		thread_index_mutex.lock();
		std::cout << name_ << " " << tbb_tasks.unsafe_size() << " items\n";
		thread_index_mutex.unlock();
		std::this_thread::sleep_for (std::chrono::seconds(2));
	}
}

inline void ThreadPool::threadTask(int rank)
{
	int count = 0;
	//hpclf_start();
	//long avg_queue_time;
	try {
	while (!this->stop && !this->workers_data[rank].done) {
		inner_task_type task;
		if (tbb_tasks.try_pop(task)) {
			task->out_flush_q = get_current_time();
			count++;
			auto start = get_current_time();
			(*task->t)();
			task->end = get_current_time();
			total_queue_time += task->out_flush_q - task->start;
			total_func_time += task->end - start;
			total_handle_time += task->end - task->start;
		}
	}
	if(count==0) return;
	} catch (std::exception& e) {
		std::cout << "Error occurred with rank " << rank << std::endl;
	}
	//ThreadPool::t_mutex.lock();
	//cout << "Thread " << std::this_thread::get_id() << " did " << count << " ops\n";
	//ThreadPool::t_mutex.unlock();
}

// add new work item to the pool
inline bool ThreadPool::enqueue(task_type task)
{
    {
        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

	total_ops ++;
	Task innerTask;
	innerTask.t = task;
	innerTask.start = get_current_time();
	tbb_tasks.push(std::make_shared<Task>(innerTask));
	
	total_enqueue_time += get_current_time() - innerTask.start;
    }
    return true;
}

inline void ThreadPool::print_stats()
{
	if (total_ops == 0)
		return;

	std::cout << "Total ops: " << total_ops << std::endl;
	std::cout << "Avg enqueue time: " << total_enqueue_time / total_ops << " us\n";
	std::cout << "Avg queue time " << total_queue_time / total_ops << " us\n";
	std::cout << "Avg Func time " << total_func_time / total_ops << " us\n";
	std::cout << "Avg Handle time: " << total_handle_time / total_ops << " us\n";
}

inline ThreadPool::~ThreadPool()
{
    {
        stop = true;
    }

    //reporter.join();

    for(std::thread &worker: workers)
	    worker.join();

    //print_stats();
}

inline long ThreadPool::getQueueSize() {
	return tbb_tasks.unsafe_size();
}

inline int ThreadPool::getThreadCount() {
	return num_threads_;
}

#endif
