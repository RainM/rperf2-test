#pragma once

#include <cstdlib>
#include <vector>
#include <ostream>
#include <atomic>

std::ostream& disassemble(std::ostream& stm, const void* addr, size_t len);

std::string disassemble(const void* addr, unsigned char len);
std::string disassemble(const void* addr, unsigned char* ret_len);

struct ret_location {
    char* addr;
    unsigned char size;
};

static std::ostream& operator << (std::ostream& stm, const ret_location& ret) {
    return stm << "Ret at " << std::hex << (uintptr_t)ret.addr << "/" << (int)ret.size;
}

std::vector<ret_location> get_all_rets(const void* addr, size_t len);
