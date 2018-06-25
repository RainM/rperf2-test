#include <sys/syscall.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <vector>
#include <atomic>
#include <unordered_set>
#include <unordered_map>

#include <sys/types.h>

#include "ru_raiffeisen_PerfPtProf.h"
#include "jni.h"

std::vector<uint64_t> timings;
std::atomic<uint32_t> skip_counter;
uint32_t number_of_iterations;
uint64_t threshold_timing;

pid_t gettid() {
    return syscall(SYS_gettid);
}

/*
 * Class:     ru_raiffeisen_PerfPtProf
 * Method:    init
 * Signature: (ID)V
 */
JNIEXPORT void JNICALL Java_ru_raiffeisen_PerfPtProf_init
  (JNIEnv *, jclass, jint skip_n, jdouble percentile) {
    double one_minus_n = (double)1 - percentile;
    number_of_iterations = 1 + (double)1 / one_minus_n;
    std::cout << "waiting " << number_of_iterations << " iterations" <<std::endl;

    timings.reserve(number_of_iterations);

    skip_counter.store(skip_n);
}

struct pid_hasher {
    std::size_t operator() (pid_t p) {
	return (std::size_t)p;
    }
};

std::unordered_map<pid_t, uint64_t> start_time_by_thread;
bool is_start = false;

/*
 * Class:     ru_raiffeisen_PerfPtProf
 * Method:    start
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_ru_raiffeisen_PerfPtProf_start(JNIEnv *, jclass) {
    auto old_val = --skip_counter;

    if (old_val <= 0) {
	is_start = true;
	//std::cout << "start" << std::endl;
	pid_t tid = ::gettid();
	start_time_by_thread[tid] = __builtin_ia32_rdtsc();
    }
}

/*
 * Class:     ru_raiffeisen_PerfPtProf
 * Method:    stop
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_ru_raiffeisen_PerfPtProf_stop(JNIEnv *, jclass) {
    if (is_start) {

	//std::cout << "stop" << std::endl;
	auto end_time = __builtin_ia32_rdtsc();
	pid_t tid = ::gettid();
	auto start_time = start_time_by_thread[tid];
	auto duration = end_time - start_time;

	if (duration > threshold_timing) {
	    std::cout << "Timing " << duration << " is longer than " << threshold_timing << std::endl;

	    ::exit(1);
	}
	
	timings.push_back(duration);

	if (timings.size() == number_of_iterations) {
	    std::sort(std::begin(timings), std::end(timings));
	    threshold_timing = timings.back();
	}
    }
}
