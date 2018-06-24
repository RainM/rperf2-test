#include "xed_driver.hpp"

#include <iostream>

extern "C" {
#include "xed/xed-interface.h"
}

xed_machine_mode_enum_t machine_mode = XED_MACHINE_MODE_LONG_64;
xed_address_width_enum_t addr_mode = XED_ADDRESS_WIDTH_64b;


std::ostream& disassemble(std::ostream& stm, const void* addr, size_t len) {
    auto it = (const char*)addr, it_end = (const char*)addr + len;
    while (it < it_end) {
	unsigned char ret_len = 0;
	stm << disassemble(it, &ret_len) << std::endl;
	it += ret_len;
    }

    return stm;
}

std::string disassemble(const void* addr, unsigned char len) {
    xed_error_enum_t xed_error;
    xed_decoded_inst_t xinstr;
    xed_decoded_inst_zero(&xinstr);
    xed_decoded_inst_set_mode(&xinstr, machine_mode, addr_mode);

    xed_error = xed_decode(
	&xinstr,
	XED_STATIC_CAST(const xed_uint8_t*, addr),
	len);

    char buffer[10000] = {};

    xed_print_info_t pi = {};
    xed_init_print_info(&pi);

    pi.p = &xinstr;
    pi.blen = sizeof(buffer);
    pi.buf = buffer;
    pi.runtime_address = (uint64_t)addr;
    pi.syntax = XED_SYNTAX_ATT;

    int ok = xed_format_generic(&pi);
    if (!ok) {
	std::cerr << "can't disassemble" << std::endl;
    }

    return buffer;
}

std::string disassemble(const void* addr, unsigned char* ret_len) {
    for (unsigned char bytes = 1; bytes <= 15; ++bytes) {
	xed_error_enum_t xed_error;
	xed_decoded_inst_t xinstr;
	xed_decoded_inst_zero(&xinstr);
	xed_decoded_inst_set_mode(&xinstr, machine_mode, addr_mode);

	xed_error = xed_decode(
	    &xinstr,
	    XED_STATIC_CAST(const xed_uint8_t*, addr),
	    bytes);


	if (xed_error == XED_ERROR_NONE) {
	    char buffer[10000] = {};

	    xed_print_info_t pi = {};
	    xed_init_print_info(&pi);

	    pi.p = &xinstr;
	    pi.blen = sizeof(buffer);
	    pi.buf = buffer;
	    pi.runtime_address = (uint64_t)addr;
	    pi.syntax = XED_SYNTAX_ATT;

	    int ok = xed_format_generic(&pi);
	    if (!ok) {
		std::cerr << "can't disassemble" << std::endl;
	    }

	    if (ret_len) {
		*ret_len = bytes;
	    }

	    return buffer;
	}
    }
    return "";
}

std::vector<ret_location> get_all_rets(const void* addr, size_t len) {
    xed_tables_init();

    std::vector<ret_location> result;

    auto it = (char*)addr, it_end = (char*)addr + len;
    while (it < it_end) {
	int bytes;
	for (bytes = 1; bytes <= 15; ++bytes) {
	    xed_error_enum_t xed_error;
	    xed_decoded_inst_t xinstr;
	    xed_decoded_inst_zero(&xinstr);
	    xed_decoded_inst_set_mode(&xinstr, machine_mode, addr_mode);

	    xed_error = xed_decode(
		&xinstr,
		XED_STATIC_CAST(const xed_uint8_t*, it),
		bytes);
	    //printf("%d %s\n",(int)bytes, xed_error_enum_t2str(xed_error));

	    if (xed_error == XED_ERROR_NONE) {
		char buffer[10000] = {};
		//xed_decoded_inst_dump(&xinstr, buffer, sizeof(buffer));

		xed_print_info_t pi = {};
		xed_init_print_info(&pi);

		pi.p = &xinstr;
		pi.blen = sizeof(buffer);
		pi.buf = buffer;
		pi.runtime_address = (uint64_t)it;
		pi.syntax = XED_SYNTAX_ATT;

		int ok = xed_format_generic(&pi);
		if (!ok) {
		    std::cerr << "can't disassemble" << std::endl;
		}
		//std::cout << "Instr for " << bytes << " bytes" << std::endl;
		std::cout << std::hex << (uintptr_t)it << "\t" << buffer << std::endl;

		auto iclass = xed_decoded_inst_get_iclass(&xinstr);

		if (iclass == XED_ICLASS_RET_FAR || iclass == XED_ICLASS_RET_NEAR) {
		    ret_location ret;
		    ret.addr = it;
		    ret.size = bytes;
		    result.push_back(ret);

		    //char buffer[10000] = {};
		    //xed_decoded_inst_dump(&xinstr, buffer, sizeof(buffer));
		    std::cout << "Return! " << buffer << std::endl;
		} else {
		    //std::cout << "iclass = " << iclass << std::endl;
		}

		break;
	    }
	}



	it += bytes;
    }

    return result;
}
