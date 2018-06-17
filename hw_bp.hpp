#pragma once

bool enable_ptrace_for_all();
bool install_hw_bp_on_exec(void* exec_bp_addr, int bpno, void (*handler)(int));
