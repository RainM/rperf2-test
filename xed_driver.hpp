#pragma once

#include <cstdlib>
#include <vector>
#include <ostream>

void disassemble(void* addr, size_t len);

struct ret_location {
    void* addr;
    uint8_t size;
};

static std::ostream& operator << (std::ostream& stm, const ret_location& ret) {
    return stm << "Ret at " << std::hex << ret.addr << "/" << (int)ret.size;
}

std::vector<ret_location> get_all_rets(void* addr, size_t len);
