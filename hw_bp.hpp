#pragma once

#include <atomic>

struct spin_cs {
    spin_cs(std::atomic<bool>& lock): _lock(lock) {
	while (_lock.exchange(true) == true) {}
    }
    ~spin_cs() {
	_lock.store(false);
    }
    spin_cs(const spin_cs&) = delete;
    spin_cs& operator = (const spin_cs&) = delete;
private:
    std::atomic<bool>& _lock;
};


bool enable_ptrace_for_all();
bool install_hw_bp_on_exec(void* exec_bp_addr, int bpno, void (*handler)(int));

unsigned char install_sigill_trampouline(char* inst_addr, char* implementation_loc);
