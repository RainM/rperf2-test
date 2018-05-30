

extern "C" {
#include "pt_cpu.h"
#include "intel-pt.h"
}

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

extern "C" {
#include <xed-interface.h>
}

#include <iostream>
  
void process_pt(char* pt_begin, size_t len) {
  
    struct pt_insn_decoder* decoder = nullptr;
    struct pt_config config = {};
    
    int errcode;

    config.size = sizeof(config);
    config.begin = reinterpret_cast<uint8_t*>(pt_begin);
    config.end = reinterpret_cast<uint8_t*>(pt_begin) + len;
    //config.cpu = 

    decoder = pt_insn_alloc_decoder(&config);
    if (!decoder) {
    std::cout << "can't allocate decoder" << std::endl;
}

    errcode = pt_insn_sync_forward(decoder);
    if (errcode < 0) {
    std::cout << "can't sync decoder" << std::endl;
}

    pt_insn_free_decoder(decoder);
    
}
