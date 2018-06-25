#include <iostream>

#include <signal.h>
#include <sys/mman.h>


__attribute__((naked))
void test() {
    printf("handler\n");
    ::exit(11);
}

void handler(int signal, siginfo_t* si, void* arg) {

    intptr_t instr_addr = reinterpret_cast<intptr_t>(si->si_addr);
    intptr_t page_addr = instr_addr & 0xFFFFFFFFFFFFF000;
    
    auto status = ::mprotect((void*)page_addr, 4096, PROT_WRITE | PROT_READ | PROT_EXEC);
    if (status) {
	::perror("mprotect");
    }

    unsigned char* instruction_ptr = (unsigned char*)si->si_addr;

    printf("instruction: %x\n", *instruction_ptr);

    *instruction_ptr = 0x90;

    status = ::mprotect((void*)page_addr, 4096, PROT_EXEC);
    if (status) {
	::perror("mprotect");
    }

    printf("SEGFAULT at %p!\n", si->si_addr);
    ucontext_t* ctx = reinterpret_cast<ucontext_t*>(arg);
    ctx->uc_mcontext.gregs[REG_RIP] = (greg_t)instruction_ptr;
}

int main() {

    struct sigaction sa = {};

    ::sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = &handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGILL, &sa, NULL);

    asm(".byte 0x06");

    std::cout << "So, we continue execution successfully" << std::endl;

    return 43;
}
