#include "NvmMiddleware.h"

#define ASSERT(expr)                                     \
    do                                                   \
    {                                                    \
        if (!(expr))                                     \
            std::cout << pmemkv_errormsg() << std::endl; \
        assert(expr);                                    \
    } while (0)
#define LOG(msg) std::cout << msg << std::endl

using namespace std;

typedef nvmmiddleware::Request request_t;
typedef nvmmiddleware::Reply reply_t;

std::mutex t_mut_mw;

long calculate_time_ms(chrono::time_point<chrono::system_clock> start_time, chrono::time_point<chrono::system_clock> end_time)
{
    return chrono::duration_cast<chrono::microseconds>(end_time - start_time).count();
}

nvmmiddleware::NvmMiddleware::NvmMiddleware(std::string dbPath, int interactiveThreads, int batchThreads) : path(dbPath)
{
    cout << "Starting middleware with " << interactiveThreads << " interactive threads and " << batchThreads << " batch threads\n";
    interactive_pool_ = new ThreadPool("interactive", interactiveThreads);
    batch_pool_ = new ThreadPool("batch", batchThreads);
    init_hdr();
    int_sizes.set_capacity(200);
    batch_sizes.set_capacity(200);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int i = 24; i < 48; i++) {
	    CPU_SET(i, &cpuset);
    }
    for (int j = 72; j < 96; j++) {
	    CPU_SET(j, &cpuset);
    }
    int_tracker = std::thread(&nvmmiddleware::NvmMiddleware::track_interactive_size, this);
    pthread_setaffinity_np(int_tracker.native_handle(), sizeof(cpu_set_t), &cpuset); 
    batch_tracker = std::thread(&nvmmiddleware::NvmMiddleware::track_batch_size, this);
    pthread_setaffinity_np(batch_tracker.native_handle(), sizeof(cpu_set_t), &cpuset);
}

nvmmiddleware::NvmMiddleware::~NvmMiddleware()
{
    std::cout << "Destroying middleware\n";
    delete interactive_pool_;
    delete batch_pool_;
    std::cout << "Middleware destroyed\n";

    hdr_interval_recorder_destroy(&recorder);
    stop = true;
    int_sizes.abort();
    batch_sizes.abort();
    int_tracker.join();
    batch_tracker.join();
    //cout << "Total Reads: " << total_reads << endl;
    //cout << "Total Writes: " << total_writes << endl;
    //cout << "Avg. GET PMEMKV (us): " << avg_get_pmemkv / total_reads << endl;
    //cout << "Avg. PUT PMEMKV (us): " << avg_put_pmemkv / total_writes << endl;
    
}

void nvmmiddleware::NvmMiddleware::track_interactive_size() {
	while (!this->stop.load()) {
		double size_;
		try {
			int_sizes.pop(size_);
			int_avg_size = int_size_ma(size_);
		} catch (const tbb::user_abort& e) {
			continue;
		}
	}
}

void nvmmiddleware::NvmMiddleware::track_batch_size() {
	while (!this->stop.load()) {
		double size_;
		try{ 
			batch_sizes.pop(size_);
			auto temp = batch_size_ma(size_);
			batch_avg_size = temp;
		} catch (const tbb::user_abort& e) {
			continue;
		}
	}
}

void nvmmiddleware::NvmMiddleware::init_hdr() {
        hdr_interval_recorder_init_all(
                &recorder,
                1,                   // Minimum value
                INT64_C(3600000000), // Maximum value
                3                   // Number of significant figures
                );
}

nvmmiddleware::Status nvmmiddleware::NvmMiddleware::put(const string *k, const string *value)
{
    nvmmiddleware::Status status;
    string fname = path + "/" + *k;
    auto start_time = chrono::system_clock::now();
    std::fstream file(fname, std::ios::out | std::ios::trunc | std::ios::binary);
    if (file.is_open()) {
      file << *value;
      file.close();
      status = nvmmiddleware::Status::OK;
    } else {
      status = nvmmiddleware::Status::ERROR;
    }
    auto end_time = chrono::system_clock::now();
    avg_put_pmemkv += calculate_time_ms(start_time, end_time);
    return status;

}

nvmmiddleware::Status nvmmiddleware::NvmMiddleware::get(const string *k, string *reply)
{
    nvmmiddleware::Status status;
    string fname = path + "/" + *k;
    auto start_time = chrono::system_clock::now();
    std::fstream file(fname, std::ios::in | std::ios::binary);
    if (!file) {
      status = nvmmiddleware::Status::KEY_NOT_FOUND;
    } else {
      std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
      reply = new std::string(fileContent);
      status = nvmmiddleware::Status::OK;
    }
    auto end_time = chrono::system_clock::now();
    avg_get_pmemkv += calculate_time_ms(start_time, end_time);
    return status;
}

future<nvmmiddleware::Status> nvmmiddleware::NvmMiddleware::enqueue_put(const string *k, const string *value, nvmmiddleware::Mode mode)
{
    future<nvmmiddleware::Status> ft;
    register_call(mode);
    auto put_task = make_shared<std::packaged_task<nvmmiddleware::Status()>>(
	std::bind(&nvmmiddleware::NvmMiddleware::put, this, k, value)
	);
    //auto shared_task = make_shared<std::function<void()>>([put_task]() { (*put_task)();  });
    auto timer = make_shared<Timer<std::chrono::microseconds>>();
    auto shared_task = make_shared<std::function<void()>>([put_task, timer, mode, this, k, value]() {
                    (*put_task)();
                    if (mode == nvmmiddleware::Mode::INTERACTIVE) {
		    	hdr_interval_recorder_record_value_atomic(&recorder, timer->End());
			this->completed_interactive_ops++;
			int_sizes.push(value->size());
                    } else {
                        this->completed_batch_ops++;
			//this->completed_batch_ops += value->size();
			this->total_batch_ops++;
			batch_sizes.push(value->size());
                    }
                    });
    ft = put_task->get_future();
    timer->Start();

    if (mode == INTERACTIVE)
    {
    	interactive_pool_->enqueue(shared_task);
	received_interactive++;
	total_writes_interactive += 1;
    }
    else
    {
	batch_pool_->enqueue(shared_task);
	received_batch++;
	total_writes_batch += 1;
    }
    //total_writes += 1;
    //cout << "Write: key " << *k << " and value " << *value << endl;
    return ft;
}

/*future<pmem::kv::status> nvmmiddleware::NvmMiddleware::enqueue_put(const string *k, const string *value, nvmmiddleware::Mode mode)
{
	future<pmem::kv::status> ft;
	register_call(mode);
	auto put_task = make_shared<std::packaged_task<pmem::kv::status()>>(
			std::bind(&nvmmiddleware::NvmMiddleware::pmemkv_put, this, kv_, k, value)
			);
	auto timer = make_shared<Timer<std::chrono::microseconds>>();
	std::function<void()> task = [put_task, timer, mode, this, k, value]() {
			(*put_task)();
			if (mode == nvmmiddleware::Mode::INTERACTIVE) {
			hdr_interval_recorder_record_value_atomic(&recorder, timer->End());
			this->completed_interactive_ops++;
			int_sizes.push(value->size());
			} else {
			this->completed_batch_ops++;
			this->total_batch_ops++;
			batch_sizes.push(value->size());
			}
		};
	ft = put_task->get_future();
	timer->Start();

	if (mode == INTERACTIVE)
	{
		received_interactive++;
		total_writes_interactive += 1;
	}
	else
	{
		received_batch++;
		total_writes_batch += 1;
	}
	task();
	return ft;
}

future<pmem::kv::status> nvmmiddleware::NvmMiddleware::enqueue_get(const string *k, string *reply, nvmmiddleware::Mode mode)
{
	future<pmem::kv::status> ft;
	register_call(mode);
	auto get_task = make_shared<std::packaged_task<pmem::kv::status()>>(
			std::bind(&nvmmiddleware::NvmMiddleware::pmemkv_get, this, kv_, k, reply)
			);
	auto timer = make_shared<Timer<std::chrono::microseconds>>();
	std::function<void()> task = [get_task, timer, mode, this, k, reply]() {
			(*get_task)();
			if (mode == nvmmiddleware::Mode::INTERACTIVE) {
			hdr_interval_recorder_record_value_atomic(&recorder, timer->End());
			this->completed_interactive_ops++;
			int_sizes.push(reply->size());
			} else {
			this->completed_batch_ops++;
			this->total_batch_ops++;
			batch_sizes.push(reply->size());
			}
		};
	ft = get_task->get_future();
	timer->Start();
	if (mode == INTERACTIVE)
	{
		received_interactive++;
	}
	else
	{
		received_batch++;
	}
	task();
	return ft;
}*/

void nvmmiddleware::NvmMiddleware::register_call(nvmmiddleware::Mode mode) {
	if (mode == nvmmiddleware::Mode::INTERACTIVE) {
		this->received_interactive++;
	} else {
		this->received_batch++;
	}
}

future<nvmmiddleware::Status> nvmmiddleware::NvmMiddleware::enqueue_get(const string *k, string *reply, nvmmiddleware::Mode mode)
{
    future<nvmmiddleware::Status> ft;
    register_call(mode);
    auto get_task = make_shared<std::packaged_task<nvmmiddleware::Status()>>(
	std::bind(&nvmmiddleware::NvmMiddleware::get, this, k, reply)
	);
    //auto shared_task = make_shared<std::function<void()>>([get_task]() { (*get_task)();  });
    auto timer = make_shared<Timer<std::chrono::microseconds>>();
    auto shared_task = make_shared<std::function<void()>>([get_task, timer, mode, this, k, reply]() {
                    (*get_task)();
                    if (mode == nvmmiddleware::Mode::INTERACTIVE) {
		    	hdr_interval_recorder_record_value_atomic(&recorder, timer->End());
			this->completed_interactive_ops++;
			int_sizes.push(reply->size());
                    } else {
                        this->completed_batch_ops++;
			//this->completed_batch_ops += reply->size();
		    	this->total_batch_ops++;
			batch_sizes.push(reply->size());
                    }
                    });
    ft = get_task->get_future();
    timer->Start();
    if (mode == INTERACTIVE)
    {
	interactive_pool_->enqueue(shared_task);
	received_interactive++;
    }
    else
    {
	batch_pool_->enqueue(shared_task);
	received_batch++;
    }
    //total_reads += 1;
    //cout << "GET: key " << *k << endl;
    return ft;
}

void nvmmiddleware::NvmMiddleware::incrementInteractiveThreads()
{
	interactive_pool_->addThread();
	resetRewardCycle();
}

void nvmmiddleware::NvmMiddleware::decreaseInteractiveThreads()
{
	interactive_pool_->removeThread();
	resetRewardCycle();
}

void nvmmiddleware::NvmMiddleware::incrementBatchThreads()
{
        batch_pool_->addThread();
        resetRewardCycle();
}

void nvmmiddleware::NvmMiddleware::decreaseBatchThreads()
{
        batch_pool_->removeThread();
        resetRewardCycle();
}

void nvmmiddleware::NvmMiddleware::singleAction(int a) {
	if (a == 0) {
		// no action
	} else if (a == 1) {
		interactive_pool_->addThread();
	} else if (a == 2) {
		interactive_pool_->removeThread();
	} else if (a == 3) {
		batch_pool_->addThread();
	} else if (a == 4) {
		batch_pool_->removeThread();
	} else if (a == 5) {
		interactive_pool_->addThread();
		batch_pool_->addThread();
	} else if (a == 6) {
		interactive_pool_->addThread();
		batch_pool_->removeThread();
	} else if (a == 7) {
		interactive_pool_->removeThread();
		batch_pool_->addThread();
	} else if (a == 8) {
		interactive_pool_->removeThread();
		batch_pool_->removeThread();
	}
	resetRewardCycle();
}

void nvmmiddleware::NvmMiddleware::jointAction(int a1, int a2) {
	if (a1 == 0 && a2 == 0) {
		// no action
	} else if (a1 == 0 && a2 == 1) {
		batch_pool_->addThread();
	} else if (a1 == 0 && a2 == 2) {
		batch_pool_->removeThread();
	} else if (a1 == 1 && a2 == 0) {
		interactive_pool_->addThread();
	} else if (a1 == 1 && a2 == 1) {
		interactive_pool_->addThread();
		batch_pool_->addThread();
	} else if (a1 == 1 && a2 == 2) {
		interactive_pool_->addThread();
		batch_pool_->removeThread();
	} else if (a1 == 2 && a2 == 0) {
                interactive_pool_->removeThread();
        } else if (a1 == 2 && a2 == 1) {
                interactive_pool_->removeThread();
                batch_pool_->addThread();
        } else if (a1 == 2 && a2 == 2) {
                interactive_pool_->removeThread();
                batch_pool_->removeThread();
        }
	resetRewardCycle();
}

void nvmmiddleware::NvmMiddleware::getCurrentState(nvmmiddleware::MiddlewareState* st)
{
	st->interactiveThreads = getInteractiveThreadCount();
	st->batchThreads = getBatchThreadCount();
	/*received_interactive = 0;
	received_batch = 0;
	//int_avg_size = 0;
	//batch_avg_size = 0;
	total_writes_interactive = 0;
	total_writes_batch = 0;
	int INTERACTIVE_SAMPLE = 20;
        int BATCH_SAMPLE = 20;
	auto start = chrono::steady_clock::now();
        while (received_interactive.load(std::memory_order_relaxed) < INTERACTIVE_SAMPLE || received_batch.load(std::memory_order_relaxed) < BATCH_SAMPLE)
        {
                std::this_thread::yield();
        }
        //std::this_thread::sleep_for (std::chrono::seconds(1));
	auto time_us = batchTimer.End();
        double time_s = time_us / 1000000;
	//st->interactiveQueueSize = interactive_pool_->getQueueSize();
	//st->batchQueueSize = batch_pool_->getQueueSize();*/
	auto time_us = batchTimer.End();
	double time_s = time_us / 1000000;
	st->interactiveQueueSize = received_interactive.load() / time_s;
	st->batchQueueSize = received_batch.load() / time_s;
	//std::cout << "test: " << received_batch.load() << std::endl;
	st->interactiveSize = int_avg_size.load();
    	st->batchSize = batch_avg_size.load();
	if (received_interactive.load() > 0) {
		st->interactiveWriteRatio = ((double)total_writes_interactive / (double)received_interactive) * 100.0;
	}
	if (received_batch.load() > 0) {	
		st->batchWriteRatio = ((double)total_writes_batch / (double)received_batch) * 100.0;
	}
	//std::cout << "write interactive: " << st->interactiveWriteRatio << " write batch: " << st->batchWriteRatio << std::endl;
}

int nvmmiddleware::NvmMiddleware::getInteractiveThreadCount() {
	return interactive_pool_->getThreadCount();
}

int nvmmiddleware::NvmMiddleware::getBatchThreadCount() {
	return batch_pool_->getThreadCount();
}

void nvmmiddleware::NvmMiddleware::resetRewardCycle() {
	completed_interactive_ops = 0;
	completed_batch_ops = 0;
	batchTimer.Start();
}

void nvmmiddleware::NvmMiddleware::performNoAction() {
	resetRewardCycle();
}

void nvmmiddleware::NvmMiddleware::get_reward_and_state(nvmmiddleware::Stats* stat, nvmmiddleware::MiddlewareState* st)
{
	auto time_us = batchTimer.End();
        double time_s = time_us / 1000000;
	//reward
        double tp = 1.0*completed_batch_ops.load(std::memory_order_relaxed) / time_s;
	//std::cout << "ops " << completed_batch_ops.load(std::memory_order_relaxed) << " time " << time_s << std::endl;
	inactive_hist = hdr_interval_recorder_sample_and_recycle(&recorder, inactive_hist);
        double lat = hdr_value_at_percentile(inactive_hist, 99.0);
        stat->tailLatency = lat_ma(lat);
        stat->throughput = isnan(tp) ? 0 : tp_ma(tp);
        //stat->tailLatency = lat;
        //stat->throughput = isnan(tp) ? 0 : tp;
	//state
	/*st->interactiveThreads = getInteractiveThreadCount();
        st->batchThreads = getBatchThreadCount();
        st->interactiveQueueSize = received_interactive.load() / time_s;
        st->batchQueueSize = received_batch.load() / time_s;
        st->interactiveSize = int_avg_size.load();
        st->batchSize = batch_avg_size.load();
	st->interactiveWriteRatio = ((double)total_writes_interactive / (double)received_interactive) * 100.0;
	st->batchWriteRatio = ((double)total_writes_batch / (double)received_batch) * 100.0;
	*/
}

//combine single funciton that updates reward and state. Keep single timer for both.
void nvmmiddleware::NvmMiddleware::getStats(nvmmiddleware::Stats* stat) {
	/*int INTERACTIVE_SAMPLE = 20;
	int BATCH_SAMPLE = 20;
	//temporal hard limit to avoid hanging middleware. Need different way to stop waiting.
	if (stop) {
		return;
        }
	auto start = chrono::steady_clock::now();
	while (completed_interactive_ops.load(std::memory_order_relaxed) < INTERACTIVE_SAMPLE || completed_batch_ops.load(std::memory_order_relaxed) < BATCH_SAMPLE)
	{
		std::this_thread::yield();
	}
	auto time_us = batchTimer.End();
	double time_s = time_us / 1000000;
	//std::cout << "time_s " << time_s << " batch ops " << completed_batch_ops.load(std::memory_order_relaxed) <<std::endl;
	//std::cout << "interactive ops " << completed_interactive_ops.load(std::memory_order_relaxed) << std::endl;*/
	auto time_us = batchTimer.End();
	double time_s = time_us / 1000000;
	std::cout << "ops " << completed_batch_ops.load(std::memory_order_relaxed) << " time " << time_s << std::endl;
	double tp = 1.0*completed_batch_ops.load(std::memory_order_relaxed) / time_s;
	inactive_hist = hdr_interval_recorder_sample_and_recycle(&recorder, inactive_hist);
	double lat = hdr_value_at_percentile(inactive_hist, 99.0);
	//stat->tailLatency = lat_ma(lat);
	//stat->throughput = isnan(tp) ? 0 : tp_ma(tp);
	stat->tailLatency = lat;
	stat->throughput = isnan(tp) ? 0 : tp;
}

void nvmmiddleware::NvmMiddleware::reset_stats() {
	hdr_interval_recorder_destroy(&recorder);
	init_hdr();
	// possibly remove this
	received_interactive = 0;
        received_batch = 0;
        int_avg_size = 0;
        batch_avg_size = 0;
        total_writes_interactive = 0;
        total_writes_batch = 0;
	batchTimer.Start();
}
