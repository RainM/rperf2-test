#include "hw_bp.hpp"

#include <iostream>

int foo() {
    return 42;
}

void exec_handler(int) {
    std::cout << "exec handler" << std::endl;
}

int main() {
    enable_ptrace_for_all();
  
    install_hw_bp_on_exec((void*)&foo, 0, exec_handler);

    int result = 0;
    for (int i = 0; i < 10; ++i) {
	result += ::foo();
    }
    return result;
}
