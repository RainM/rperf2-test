#include "xed_driver.hpp"

#include <iostream>

extern "C" {
#include "xed/xed-interface.h"
}

void disassemble(void* addr, size_t len) {

}

std::vector<ret_location> get_all_rets(void* addr, size_t len) {

    xed_machine_mode_enum_t machine_mode;
    xed_address_width_enum_t addr_mode;

    machine_mode = XED_MACHINE_MODE_LONG_64;
    addr_mode = XED_ADDRESS_WIDTH_64b;

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
		//char buffer[10000] = {};
		//xed_decoded_inst_dump(&xinstr, buffer, sizeof(buffer));
		std::cout << "Instr for " << bytes << " bytes" << std::endl;
		//std::cout << "    " << buffer << std::endl;

		auto iclass = xed_decoded_inst_get_iclass(&xinstr);

		if (iclass == XED_ICLASS_RET_FAR || iclass == XED_ICLASS_RET_NEAR) {
		    ret_location ret;
		    ret.addr = it;
		    ret.size = bytes;
		    result.push_back(ret);

		    char buffer[10000] = {};
		    xed_decoded_inst_dump(&xinstr, buffer, sizeof(buffer));
		    std::cout << "Return! " << buffer << std::endl;
		} else {
		    std::cout << "iclass = " << iclass << std::endl;
		}
		
		break;
	    }
	}


	
	it += bytes;
    }
    
    return result;
}
