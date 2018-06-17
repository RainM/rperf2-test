#include <iostream>
#include <math.h>
#include <unordered_map>

#include <signal.h>
#include <sys/mman.h>

#include "xed_driver.hpp"
#include "hw_bp.hpp"

double foo(int i) {

    if (i == 123) {
	return 321;
    }

    int qq = i % 1222;
    float f = ::sinf(qq);
    
    return i > 2 ? cosf(43) : exp(f);
}

void enable_read_write_for_routine(const std::vector<ret_location>& rets) {
    uintptr_t begin_addr = (uintptr_t)rets.front().addr;
    uintptr_t end_addr = (uintptr_t)rets.back().addr;

    auto page_begin_addr = begin_addr & 0xFFFFFFFFFFFFF000;
    auto page_end_addr = (end_addr | 0xFFF) + 1;

    auto status = ::mprotect((void*)page_begin_addr, page_end_addr - page_begin_addr, PROT_READ | PROT_WRITE | PROT_EXEC);

    if (status) {
	::perror("Can't mprotect");
    }
}

void method_start(int) {
    std::cout << "Method start!" << std::endl;
}

std::unordered_map<uintptr_t, unsigned char> original_code_for_location;

void handler(int signal, siginfo_t* si, void* arg) {

    unsigned char* instruction_ptr = (unsigned char*)si->si_addr;

    unsigned char original_instruction = original_code_for_location[(uintptr_t)instruction_ptr];
    
    printf("instruction: %x -> %x\n", *instruction_ptr, original_instruction);

    *instruction_ptr = original_instruction;

    
    printf("SEGILL at %p!\n", si->si_addr);
    ucontext_t* ctx = reinterpret_cast<ucontext_t*>(arg);
    ctx->uc_mcontext.gregs[REG_RIP] = (greg_t)instruction_ptr;
}


int main() {
    auto rets = get_all_rets((void*)&foo, 0x61);

    enable_read_write_for_routine(rets);
    
    for (const auto& ret : rets) {
	std::cout << ret << std::endl;
	original_code_for_location[(uintptr_t)ret.addr] = *(unsigned char*)ret.addr;
	*(unsigned char*)ret.addr = 6;
    }

    struct sigaction sa = {};

    ::sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = &handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGILL, &sa, NULL);
    
    ::enable_ptrace_for_all();
    ::install_hw_bp_on_exec((void*)foo, 0, method_start);

    std::cout << "foo: " << foo(1) << std::endl;
    
    return 44;
}
