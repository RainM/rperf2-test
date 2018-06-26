#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>

#include <cmath>
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
std::atomic<int64_t> skip_counter;
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
    number_of_iterations = ceil((double)1 / one_minus_n) - 1;
    std::cout << "waiting " << number_of_iterations << " iterations" <<std::endl;

    timings.reserve(number_of_iterations);

    skip_counter.store(skip_n);
}

bool is_start = false;
struct timespec start_time;

/*
 * Class:     ru_raiffeisen_PerfPtProf
 * Method:    start
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_ru_raiffeisen_PerfPtProf_start(JNIEnv *, jclass) {
    auto old_val = --skip_counter;

    if (old_val <= 0) {
	is_start = true;
	pid_t tid = ::gettid();
	::clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
    }
}

/*
 * Class:     ru_raiffeisen_PerfPtProf
 * Method:    stop
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_ru_raiffeisen_PerfPtProf_stop(JNIEnv *, jclass) {
    if (is_start) {
	struct timespec end_time;
	::clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);
	pid_t tid = ::gettid();
	auto duration = (uint64_t)end_time.tv_sec - (uint64_t)start_time.tv_sec;
	duration *= 1000000000;
	duration += end_time.tv_nsec;
	duration -= start_time.tv_nsec;

	if (threshold_timing && duration > threshold_timing) {
	    std::cout << "Timing " << duration << "ns is longer than " << threshold_timing << "ns" << std::endl;

	    ::exit(1);
	}
	
	timings.push_back(duration);

	if (timings.size() == number_of_iterations) {
	    std::sort(std::begin(timings), std::end(timings));
	    threshold_timing = timings.back();

	    std::cout << "mediana = " << timings[timings.size()/2] << "ns" << std::endl;
	}
    }
}
