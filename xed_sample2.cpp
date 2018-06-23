#include <signal.h>
#include <math.h>
#include <unordered_map>
#include <unordered_set>
#include <sys/mman.h>
#include <iostream>

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

void doo() {

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


std::unordered_map<const void*, void*> handler_by_addr;
std::unordered_set<const void*> function_begins;

void handler(int signal, siginfo_t* si, void* arg) {

    const void* sigill_addr = (unsigned char*)si->si_addr;
    const void* sigill_handler = handler_by_addr[sigill_addr];
    
    if (sigill_handler == nullptr) {
	printf("???\n");
	::exit(1);
    }

    printf("SIGILL handler\n");
    
    ucontext_t* ctx = reinterpret_cast<ucontext_t*>(arg);
    ctx->uc_mcontext.gregs[REG_RIP] = (greg_t)sigill_handler;
}


int main() {
  auto rets = get_all_rets((void*)&foo, (intptr_t)&doo - (intptr_t)&foo);

    enable_read_write_for_routine(rets);

    void* handlers = ::mmap(
	nullptr,
	4096,
	PROT_WRITE | PROT_EXEC,
	MAP_PRIVATE | MAP_ANONYMOUS,
	0,
	0);

    char* it = (char*)handlers;
    
    for (const auto& ret : rets) {
	std::cout << ret << std::endl;
	auto len = install_sigill_trampouline(ret.addr, it);
	handler_by_addr[ret.addr] = it;
	it += len;
    }

    auto tr_sz = install_sigill_trampouline((char*)&foo, it);
    handler_by_addr[(char*)&foo] = it;
    it += tr_sz;
    
    struct sigaction sa = {};

    ::sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = &handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGILL, &sa, NULL);

    
    std::cout << "foo: " << foo(1) << std::endl;

    std::cout << "Somewhere in the end" << std::endl;
    
    return 44;
}
