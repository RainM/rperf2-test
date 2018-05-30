#if !defined(_GNU_SOURCE)
#  define _GNU_SOURCE
#endif

#include "pt_parser.hpp"

#include <sched.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/mman.h>

#include <sys/ioctl.h>

#include <iostream>
#include <linux/perf_event.h>

int do_ptxed_main(int argc, char *argv[]);

int main() {
    std::cout << "hello" << std::endl;

    struct perf_event_attr attr = {};
    attr.size = sizeof(attr);
    attr.type = 7;

    attr.exclude_kernel = 1;

    auto tid = ::syscall(__NR_gettid);
    
    int fd = syscall(SYS_perf_event_open, &attr, tid, -1, -1, 0);

    std::cout << "fd: " << fd << std::endl;

    struct perf_event_mmap_page* header;
    char* base;
    char* data;
    char* aux;

    base = (char*)::mmap(NULL, (33) * 4096, PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
	std::cerr << "can't allocate mmap" << std::endl;
    }

    header = (perf_event_mmap_page*)base;
    data = base + header->data_offset;
    
    header->aux_offset = header->data_offset + header->data_size;
    header->aux_size = 32 * 4096;

    aux = (char*)::mmap(NULL, header->aux_size, PROT_READ, MAP_SHARED, fd, header->aux_offset);
    if (aux == MAP_FAILED) {
	std::cerr << "failed for mmap" << std::endl;
    }

    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

    std::cout << "head: " << header->aux_head << ", tail: " << header->aux_tail << std::endl;

    {
	auto old_head = header->aux_head;
	volatile auto* head_ptr = &header->aux_head;
	while (*head_ptr == old_head) {
	    auto p = ::syscall(__NR_gettid);
	}
    }

    std::cout << "head: " << header->aux_head << ", tail: " << header->aux_tail << std::endl;

    int result = 0;
    for (int i = 0; i < 100000; ++i) {
	result += i % 123;
    }


    for (int i = 0; i < 10; ++i) {
	std::cout << "Measuring instruction count for this printf\n" << "res = " << result << std::endl;
    }

    std::cout << "head: " << header->aux_head << ", tail: " << header->aux_tail << std::endl;

    {
	auto old_head = header->aux_head;
	volatile auto* head_ptr = &header->aux_head;
	while (*head_ptr == old_head) {
	    auto p = ::syscall(__NR_gettid);
	}
    }

    std::cout << "head: " << header->aux_head << ", tail: " << header->aux_tail << std::endl;

    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);

    FILE* profile = ::fopen("out", "wb");
    if (!profile) {
	std::cerr << "can't open out file" << std::endl;
    }
    
    size_t aux_size = header->aux_head - header->aux_tail;
    std::cout << "aux size = "  << aux_size << std::endl;
    auto written = ::fwrite(aux + header->aux_tail, 1, aux_size, profile);
    if (written != aux_size) {
	std::cerr << "can't write data" << std::endl;
    }

    ::fclose(profile);

    //char* argv[] = {"./ptxed", "--pt", "out"};
    //do_ptxed_main(3, argv);

    ::process_pt(aux + header->aux_tail, aux_size);
}
