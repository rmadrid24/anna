#ifndef NVMMIDDLEWARE_H
#define NVMMIDDLEWARE_H

#include <mutex>
#include <future>
#include <atomic>
#include <chrono>
#include "ThreadPool.h"
#include "timer.h"
#include <hdr/hdr_histogram.h>
#include <hdr/hdr_interval_recorder.h>
#include <stdint.h>
#include <tbb/concurrent_queue.h>
#include <math.h>
#include <fstream>

#define REWARD_SAMPLE 200

template <uint16_t N, class input_t = uint32_t, class sum_t = uint32_t>
class SMA {
        public:
                input_t operator()(input_t input) {
			sum -= previousInputs[index];
                        sum += input;
                        previousInputs[index] = input;
                        if (++index == N)
                                index = 0;
                        return (sum + (N / 2)) / N;
                }

                static_assert(
                                sum_t(0) < sum_t(-1),  // Check that `sum_t` is an unsigned type
                                "Error: sum data type should be an unsigned integer, otherwise, "
                                "the rounding operation in the return statement is invalid.");

        private:
                uint8_t index             = 0;
                input_t previousInputs[N] = {};
                sum_t sum                 = 0;
};

template <uint8_t K, class uint_t = uint16_t>
class EMA {
  public:
    /// Update the filter with the given input and return the filtered output.
    uint_t operator()(uint_t input) {
        state += input;
        uint_t output = (state + half) >> K;
        state -= output;
        return output;
    }

    static_assert(
        uint_t(0) < uint_t(-1),  // Check that `uint_t` is an unsigned type
        "The `uint_t` type should be an unsigned integer, otherwise, "
        "the division using bit shifts is invalid.");

    /// Fixed point representation of one half, used for rounding.
    constexpr static uint_t half = 1 << (K - 1);

  private:
    uint_t state = 0;
};

namespace nvmmiddleware
{
    enum Mode { INTERACTIVE, BATCH };
    enum EventType { READ, WRITE };
    enum Status { OK, KEY_NOT_FOUND, ERROR };
    
    struct AppState {
	    int id;
	    int blockSize;
	    int readProportion;
	    int writeProportion;
    };

    struct Stats {
	    double tailLatency;
	    double throughput;
    };

    struct MiddlewareState {
    	int interactiveThreads = 0;
	int batchThreads = 0;
	double interactiveQueueSize = 0.0;
	double batchQueueSize = 0.0;
	int interactiveSize = 0;
	int batchSize = 0;
	double interactiveWriteRatio = 0.0;
	double batchWriteRatio = 0.0;
	std::vector<AppState> apps;
    };

    class Request {
	    public:
		    std::string appId;
		    std::string funcId;
		    std::string key;
		    int blockSize;
		    Mode mode;
		    EventType type;
		    std::string value;
		    Request(std::string appid, std::string funcid, std::string k, int bs, Mode m, EventType t, std::string val)
			    : appId(appid), funcId(funcid), key(k), blockSize(bs), mode(m), type(t), value(val) {}
		    Request(const Request &r)
			    : appId(r.appId), funcId(r.funcId), key(r.key), blockSize(r.blockSize), mode(r.mode), type(r.type), value(r.value) {}
    };

    class Reply {
	    public:
		    Status status;
		    std::string value;
		    Reply() {};
    };

    class NvmMiddleware
    {
    public:
        NvmMiddleware(std::string dbPath, int interactiveThreads, int batchThreads);
	std::future<Status> enqueue_put(const std::string *k, const std::string *value, Mode mode);
	std::future<Status> enqueue_get(const std::string *k, std::string *reply, Mode mode);
	//Status direct_put(const std::string *k, const std::string *value);
	//Status direct_get(const std::string *k, std::string *reply);
	void close();
        void incrementInteractiveThreads();
	void decreaseInteractiveThreads();
	void performNoAction();
	void getCurrentState(MiddlewareState* st);
	int getBatchThreadCount();
	int getInteractiveThreadCount();
	void getStats(Stats* stat);
	void incrementBatchThreads();
	void decreaseBatchThreads();
	void jointAction(int a1, int a2);
	void singleAction(int a);
	void reset_stats();
	void track_interactive_size();
	void track_batch_size();
	void get_reward_and_state(nvmmiddleware::Stats* stat, nvmmiddleware::MiddlewareState* st);
	~NvmMiddleware();

    private:
	Status put(const std::string *k, const std::string *value);
	Status get(const std::string *k, std::string *reply);
	void resetRewardCycle();
        void init_hdr();
	void register_call(nvmmiddleware::Mode mode);
	ThreadPool *interactive_pool_ = NULL;
        ThreadPool *batch_pool_ = NULL;
	std::atomic<long> avg_get_pmemkv{0};
	std::atomic<long> avg_put_pmemkv{0};
	std::atomic<int> total_writes_interactive{0};
	std::atomic<int> total_writes_batch{0};
	std::atomic<int> completed_interactive_ops {0};
	std::atomic<int> completed_batch_ops {0};
	// remove this after test
	std::atomic<int> total_batch_ops {0};
	std::atomic<int> received_interactive {0};
	std::atomic<int> received_batch {0};
	Timer<std::chrono::microseconds> batchTimer;
	Timer<std::chrono::microseconds> arrivalTimer;
	struct hdr_interval_recorder recorder;
	struct hdr_histogram* inactive_hist = NULL;
	SMA<5> lat_ma;
	SMA<5> tp_ma;
	SMA<10> int_size_ma;
	SMA<10> batch_size_ma;
	tbb::concurrent_bounded_queue<double> int_sizes;
	tbb::concurrent_bounded_queue<double> batch_sizes;
	std::atomic<double> int_avg_size{0.0};
	std::atomic<double> batch_avg_size{0.0};
	std::atomic<bool> stop{false};
	std::thread int_tracker;
	std::thread batch_tracker;
	std::string path;
    };
}
#endif
