#include <cstdio>
#include <cstddef>
#include <csignal>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/prctl.h>
#include <cstdint>
#include <errno.h>
#include <iostream>
#include <cstring>

#include "xed_driver.hpp"

enum {
	BREAK_EXEC = 0x0,
	BREAK_WRITE = 0x1,
	BREAK_READWRITE = 0x3,
};

enum {
	BREAK_ONE = 0x0,
	BREAK_TWO = 0x1,
	BREAK_FOUR = 0x3,
	BREAK_EIGHT = 0x2,
};

#define ENABLE_BREAKPOINT(x) (0x1<<(x*2))
#define ENABLE_BREAK_EXEC(x) (BREAK_EXEC<<(16+(x*4)))
#define ENABLE_BREAK_WRITE(x) (BREAK_WRITE<<(16+(x*4)))
#define ENABLE_BREAK_READWRITE(x) (BREAK_READWRITE<<(16+(x*4)))

bool enable_ptrace_for_all() {
    auto status = ::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0,0,0);
    return !status;
}

bool install_hw_bp_on_exec(void* exec_bp_addr, int bpno, void (*handler)(int)) {
    pid_t child_pid = 0;
    pid_t parent_pid = ::getpid();
    uint32_t exec_pt_mask = ENABLE_BREAK_EXEC(bpno);


    if ((child_pid = ::fork()) != 0) {
	int child_status = 0;

	::waitpid(child_pid, &child_status, 0);
	::signal(SIGTRAP, handler);

	return WIFEXITED(child_status) && !WEXITSTATUS(child_status);
    } else {
	if (::ptrace(PTRACE_ATTACH, parent_pid, nullptr, nullptr)) {
	    std::cerr << "Can't attach ptrace" << std::endl;
	    ::exit(1);
	}

	int parent_status;
	while(!WIFSTOPPED(parent_status)) {
	    ::waitpid(parent_pid, &parent_status, 0);
	}

	if (::ptrace(
		PTRACE_POKEUSER,
		parent_pid,
		offsetof(struct user, u_debugreg[bpno]),
		exec_bp_addr))
	{
	    std::cerr << "Can't affect debug reg" << std::endl;
	    ::exit(2);
	}

	if (::ptrace(
		PTRACE_POKEUSER,
		parent_pid,
		offsetof(struct user, u_debugreg[7]),
		ENABLE_BREAKPOINT(bpno) | exec_pt_mask
		))
	{
	    std::cerr << "Can't enable HW breakpoint" << std::endl;
	    ::exit(3);
	}

	::exit(0);
    }
}

unsigned char install_sigill_trampouline(char* inst_addr, char* implementation_loc) {
    unsigned char inst_len = 0;
    auto instruction = disassemble(inst_addr, &inst_len);

    std::cout << "Create trampouline:\n";
    std::cout << std::hex << (uintptr_t)inst_addr << "/" << (int)inst_len << "\t" << instruction << std::endl;

    ::memcpy(implementation_loc, inst_addr, inst_len);

    unsigned char result = inst_len + 6 + 8; // instruction len + 2 byte as 'jmp' encoding + 4 bytes as jump target reg + 8 as target

    implementation_loc[inst_len + 0] = (unsigned char)0xFF;
    implementation_loc[inst_len + 1] = (unsigned char)0x25;
    implementation_loc[inst_len + 2] = (unsigned char)0x00;
    implementation_loc[inst_len + 3] = (unsigned char)0x00;
    implementation_loc[inst_len + 4] = (unsigned char)0x00;
    implementation_loc[inst_len + 5] = (unsigned char)0x00;
    union {
	char* c;
	uint64_t* addr;
    } u = {implementation_loc + inst_len + 6};

    *u.addr = ((uintptr_t)inst_addr) + inst_len;

    std::cout << "Trampouline: "; disassemble(std::cout, implementation_loc, inst_len + 6) << std::endl;

    *inst_addr = 0x06;

    return result;
}
