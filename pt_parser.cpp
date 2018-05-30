

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
  
int decoder_callback(struct pt_packet_unknown* unknown, const struct pt_config* config, const uint8_t* pos, void* context) {

}

int read_memory_callback(uint8_t* buffer, size_t size, const struct pt_asid* asid, uint64_t ip, void* context) {
    std::cout << "read memory callback for IP " << ip << std::endl;
}

void process_pt(char* pt_begin, size_t len) {
  
    struct pt_block_decoder* decoder = nullptr;
    struct pt_config config = {};
    
    int errcode;

    config.size = sizeof(config);
    config.begin = reinterpret_cast<uint8_t*>(pt_begin);
    config.end = reinterpret_cast<uint8_t*>(pt_begin) + len;
    config.decode.callback = decoder_callback;
    config.decode.context = nullptr;

    decoder = pt_blk_alloc_decoder(&config);
    if (!decoder) {
	std::cout << "can't allocate decoder" << std::endl;
    }

    while (true) {
	auto status = pt_blk_sync_forward(decoder);
	if (status < 0) {
	    std::cout << "can't sync" << std::endl;
	}

	for (;;) {
	    struct pt_block block = {};
	    
	    while (status & pts_event_pending) {
		struct pt_event event;
		status = pt_blk_event(decoder, &event, sizeof(event));

		if (status < 0) {
		    std::cout << "can't get pending event" << std::endl;
		    break;
		}
	    }

	    errcode = pt_blk_next(decoder, &block, sizeof(block));

	    if (errcode < 0) {
		auto code = pt_error_code(errcode);
		std::cout << "can't pt_blk_next " << pt_errstr(code) << std::endl;
		break;
	    }
	}
    }

/*
    auto default_image = pt_insn_get_image(decoder);
    pt_image_set_callback(default_image, read_memory_callback, nullptr);

    for (;;) {

    errcode = pt_insn_sync_forward(decoder);
    if (errcode < 0) {
    std::cout << "can't sync decoder" << std::endl;
}

    for (;;) {
    struct pt_insn insn;

    errcode = pt_insn_next(decoder, &insn, sizeof(insn));

    if (insn.iclass != ptic_error) {
    std::cout << "Instruction!" << std::endl;
}

    if (errcode < 0) {
    break;
}
}
}
*/

    pt_blk_free_decoder(decoder);
    
}
