#ifndef MIDDLEWARE_C_TIMER_H_
#define MIDDLEWARE_C_TIMER_H_

#include <chrono>

template <typename T>
class Timer {
       	public:
	      	void Start() {
		    	time_ = Clock::now();
	      	}

	      	double End() {
		    	T span;
		    	Clock::time_point t = Clock::now();
		    	span = std::chrono::duration_cast<T>(t - time_);
		    	return span.count();
	      	}

       	private:
		typedef std::chrono::high_resolution_clock Clock;
	      	Clock::time_point time_;
};

#endif // MIDDLEWARE_C_TIMER_H_

