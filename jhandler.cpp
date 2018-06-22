#include <jni.h>
#include <jvmti.h>
#include <jvmticmlr.h>

#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdint.h>

#include <iostream>
#include <mutex>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>

extern "C" {
#include "xed/xed-interface.h"
}

#include "hw_bp.hpp"
#include "xed_driver.hpp"


struct method_to_profile {
    void* addr;
};

std::unordered_map<const void*, void*> handler_by_addr;
std::unordered_set<const void*> routine_begin;
std::unordered_set<const void*> routine_end;

std::atomic<bool> atomic_guard;

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

void handler(int signal, siginfo_t* si, void* arg) {

    const void* sigill_addr = (unsigned char*)si->si_addr;
    const void* sigill_handler = handler_by_addr[sigill_addr];

    {
	spin_cs cs(atomic_guard);
	
	auto it_route_begin = routine_begin.find(sigill_addr);
	if (it_route_begin != routine_begin.end()) {
	    //printf("BEGIN/SIGILL: %x -> %x\n", sigill_addr, sigill_handler);
	} else {
	    auto it_route_end = routine_end.find(sigill_addr);
	    if (it_route_end != routine_end.end()) {
		//printf("END/SIGILL: %x -> %x\n", sigill_addr, sigill_handler);
	    } else {
		printf("UNKNOWN/SIGILL: %x -> %x\n", sigill_addr, sigill_handler);
	    }
	}
    }
    
    if (sigill_handler == nullptr) {
	printf("???\n");
	::exit(1);
    }

    ucontext_t* ctx = reinterpret_cast<ucontext_t*>(arg);
    ctx->uc_mcontext.gregs[REG_RIP] = (greg_t)sigill_handler;
}


static void JNICALL
cbMethodCompiled(
            jvmtiEnv *jvmti,
            jmethodID method,
            jint code_size,
            const void* code_addr,
            jint map_length,
            const jvmtiAddrLocationMap* map,
            const void* compile_info)
{
    std::cout << "Method compiled" << std::endl;

    char* method_name = nullptr;
    char* sig = nullptr;
    if (!jvmti->GetMethodName(method, &method_name, &sig, NULL)) {
	//std::cout << "mname: " << method_name << ", sig: " << sig << std::endl;

	jclass clazz;
	char* clsig = nullptr;
	if (!jvmti->GetMethodDeclaringClass(method, &clazz)) {
	    if (!jvmti->GetClassSignature(clazz, &clsig, nullptr)) {

		//std::cout << "clsig: " << clsig << std::endl;

		std::string full_method_name;
		full_method_name += clsig;
		full_method_name += "->";
		full_method_name += method_name;
		full_method_name += sig;

		std::cout << "Method: " << full_method_name << std::endl;

		if (method_name == std::string("foo") /* && sig == "(II)I" */ ) {
		    std::cout << "METHOD!!!" << std::endl;
		    auto rets = get_all_rets(code_addr, code_size+1);

		    void* handlers = ::mmap(
			nullptr,
			4096,
			PROT_WRITE | PROT_EXEC,
			MAP_PRIVATE | MAP_ANONYMOUS,
			0,
			0);

		    void* it = handlers;

		    for (const auto& ret : rets) {
			std::cout << ret << std::endl;
			::memcpy(it, ret.addr, ret.size);
			handler_by_addr[ret.addr] = it;
			it += ret.size;
			*(unsigned char*)ret.addr = 0x06; // incorrect encoding

			std::cout << "RET Handler: " << std::hex << ret.addr << " -> " << std::hex << it << std::endl;
			routine_end.insert(ret.addr);
		    }

		    {

			spin_cs cs(atomic_guard);

			
		    unsigned char first_inst_len = 0;
		    auto first_instruction = disassemble(code_addr, &first_inst_len);
		    std::cout << "first instruction: " << first_instruction << std::endl;
		    
		    //auto routine_begin_handler_ptr = it;
		    handler_by_addr[code_addr] = it;
		    std::cout << "Handler: " << std::hex << code_addr << " -> " << std::hex << it << std::endl;
		    routine_begin.insert(code_addr);
		    ::memcpy(it, code_addr, first_inst_len);
		    it += first_inst_len;
		    
		    uintptr_t jmp_target_diff = ((uintptr_t)code_addr + 2) - (uintptr_t)it;

		    auto inst_begin = it;
		    *(unsigned char*)it++ = 0xe9; // jmp
		    *(uint32_t*)it = jmp_target_diff;
		    it += sizeof(uint32_t);
		    
		    //std::cout << "Instr for " << bytes << " bytes" << std::endl;
		    auto disasm = disassemble(inst_begin, 5);
		    std::cout << "Jump bask: " << std::hex << (uintptr_t)inst_begin << "\t -> " << disasm << std::endl;

		    disassemble(std::cout, inst_begin - first_inst_len, first_inst_len + 5);
		    std::cout << std::endl;

		    unsigned char second_inst_len = 0;
		    const char* second_inst_ptr = (const char*)code_addr + first_inst_len;
		    auto second_inst = disassemble(second_inst_ptr, &second_inst_len);
		    std::cout << "second inst: " << second_inst << std::endl;
		    
		    handler_by_addr[second_inst_ptr] = it;
		    routine_begin.insert(second_inst_ptr);
		    ::memcpy(it, second_inst_ptr, second_inst_len);
		    it += second_inst_len;

		    uintptr_t jmp_target_inst2_diff = ((uintptr_t)second_inst_ptr + 1) - ((uintptr_t)it + 5);
		    std::cout << "!Second instr: " << std::hex << (uintptr_t)second_inst_ptr << "@" << (int)second_inst_len << std::endl;
		    disassemble(std::cout, second_inst_ptr, second_inst_len) << std::endl;

		    auto second_inst_begin = it;
		    *(unsigned char*)it++ = 0xE9;
		    *(uint32_t*)it = jmp_target_inst2_diff;
		    it += sizeof(uint32_t);

		    std::cout << "jump back: ";
		    disassemble(std::cout, second_inst_begin, second_inst_len) << std::endl;
		    
		    //*(unsigned char*)code_addr = 0x06;
		    ::memset((void*)code_addr, 0x06, first_inst_len);
		    //::memset((void*)second_inst_ptr, 0x06, second_inst_len);
		    //*((unsigned char*)code_addr + first_inst_len) = 0x06;
		    
		    std::cout << "SET SIGILL at the beginning" << std::endl;


		    }
		    
		    //asm volatile ("" : : : "memory");
		    
		} else {
		    std::cout << "method = " << method_name << std::endl;
		}
		
		{
		    const jvmtiCompiledMethodLoadRecordHeader* comp_info = reinterpret_cast<const jvmtiCompiledMethodLoadRecordHeader*>(compile_info);


		    if (comp_info->kind == JVMTI_CMLR_INLINE_INFO) {
			const jvmtiCompiledMethodLoadInlineRecord* inline_info = reinterpret_cast<const jvmtiCompiledMethodLoadInlineRecord*>(compile_info);
			for (int i = 0; i < inline_info->numpcs; ++i) {
			    PCStackInfo* info = &inline_info->pcinfo[i];

			    std::string inlined_method;
			    
			    for (int i = 0; i < info->numstackframes; ++i) {
				jmethodID inline_method = info->methods[i];
				//std::cout << "methodID: " << method << std::endl;

				char* inline_method_name = nullptr;
				char* inline_method_sign = nullptr;
				jvmti->GetMethodName(inline_method, &inline_method_name, &inline_method_sign, nullptr);
				jclass inline_clazz;
				jvmti->GetMethodDeclaringClass(inline_method, &inline_clazz);
				char* inline_class_sign = nullptr;
				jvmti->GetClassSignature(inline_clazz, &inline_class_sign, nullptr);
				char* inline_source_file = nullptr;
				jvmti->GetSourceFileName(inline_clazz, &inline_source_file);
				jint inline_entry_cnt = 0;
				jvmtiLineNumberEntry* lines = nullptr;
				jvmti->GetLineNumberTable(inline_method, &inline_entry_cnt, &lines);

				//std::cout << inline_class_sign << "->" << inline_method_name << inline_method_sign << std::endl;

				/*
				std::cout << "Lines: ";
				for (int ii = 0; ii < inline_entry_cnt; ++ii) {
				    std::cout << lines[i].line_number << ", ";
				}
				std::cout << std::endl;


				jvmti->Deallocate((unsigned char*)lines);
				*/

				
				jvmti->Deallocate((unsigned char*)inline_source_file);
				jvmti->Deallocate((unsigned char*)inline_class_sign);
				jvmti->Deallocate((unsigned char*)inline_method_sign);
				jvmti->Deallocate((unsigned char*)inline_method_name);
			    }
			    
			    //jmethodID method = *info->methods;
			    //std::cout << "methodID" << method << std::endl;
			}
		    } else {
			std::cout << "No inline info" << std::endl;
		    }
		}
		jvmti->Deallocate((unsigned char*)clsig);
	    }
	}

	
	jvmti->Deallocate((unsigned char*)method_name);
	jvmti->Deallocate((unsigned char*)sig);
    }

    std::cout << "--------------------------------------------" << std::endl;
}

void JNICALL
cbMethodDynamic(jvmtiEnv *jvmti,
            const char* name,
            const void* address,
            jint length)
{
    //std::cout << "dynamic method" << name << "@" << address << ":" << length << std::endl;
}

JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {

    std::cout << "Hello world from JVMTI" << std::endl;

    struct sigaction sa = {};
    
    ::sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = &handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGILL, &sa, NULL);

    
    jvmtiEnv* jvmti = nullptr;
    {
	auto status = vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1);
	std::cout << "GetEnv: " << status << std::endl;
    }

    {
	jvmtiCapabilities caps = {};

	caps.can_generate_all_class_hook_events  = 1;
	caps.can_tag_objects                     = 1;
	caps.can_generate_object_free_events     = 1;
	caps.can_get_source_file_name            = 1;
	caps.can_get_line_numbers                = 1;
	caps.can_generate_vm_object_alloc_events = 1;
	caps.can_generate_compiled_method_load_events = 1;
	
	auto status = jvmti->AddCapabilities(&caps);
	std::cout << "AddCapabilities: " << status << std::endl;
    }

    {
	jvmtiEventCallbacks callbacks = {};

	callbacks.CompiledMethodLoad = &cbMethodCompiled;
	callbacks.DynamicCodeGenerated = &cbMethodDynamic;

	auto status = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
	std::cout << "SetEventcallbacks: " << status << std::endl;
    }

    {
 	jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_LOAD, (jthread)NULL);
	jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, (jthread)NULL);
    }
    
    {
	auto status = jvmti->GenerateEvents(JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
	std::cout << "GenerateEvents: " << status << std::endl;

	status = jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD);
	std::cout << "GenerateEvents: " << status << std::endl;
    }
    
    return 0;
}
