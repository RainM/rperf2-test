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

struct method_to_profile_t {
    void* addr;
    char* trampoulines;
};

std::unordered_map<const void*, void*> handler_by_addr;
std::unordered_set<const void*> routine_begin;
std::unordered_set<const void*> routine_end;

std::atomic<bool> atomic_guard;

void handler(int signal, siginfo_t* si, void* arg) {

    ::exit(11);
    
    const void* sigill_addr = (unsigned char*)si->si_addr;

    void* sigill_handler = nullptr;
    {
	spin_cs cs(atomic_guard);

	sigill_handler = (void*)handler_by_addr[sigill_addr];

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

template <typename T>
struct jvmti_ptr {
    jvmti_ptr(jvmtiEnv* e, T p): _env(e), _ptr(p) { }
    jvmti_ptr(jvmtiEnv* e): _env(e), _ptr(nullptr) { }
    ~jvmti_ptr() {
	_env->Deallocate((unsigned char*)_ptr);
    }
    jvmti_ptr(const jvmti_ptr&) = delete;
    void operator = (const jvmti_ptr&) = delete;
    T& get() {
	return _ptr;
    }
private:
    jvmtiEnv* _env;
    T _ptr;
};

template <typename T>
jvmti_ptr<T>&& ensure_deallocate(jvmtiEnv* e, T t) {
    return jvmti_ptr<T>(e, t);
}

template <typename T>
jvmti_ptr<T>&& ensure_deallocate(jvmtiEnv* e) {
    return jvmti_ptr<T>(e, nullptr);
}

std::string class_to_profile;
std::string method_to_profile;

std::unordered_map<jmethodID, method_to_profile_t> methods_to_profile_by_jmethodid;

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

    bool process_method = false;
    
    jvmti_ptr<char*> method_name(jvmti);
    char* sig = nullptr;
    if (!jvmti->GetMethodName(method, &method_name.get(), &sig, NULL)) {
	jclass clazz;
	jvmti_ptr<char*> clsig(jvmti);
	if (!jvmti->GetMethodDeclaringClass(method, &clazz)) {
	    if (!jvmti->GetClassSignature(clazz, &clsig.get(), nullptr)) {
		std::string full_method_name;
		full_method_name += clsig.get();
		full_method_name += "->";
		full_method_name += method_name.get();
		full_method_name += sig;

		std::cout << "Method: " << full_method_name << std::endl;
		
		if (strcmp(clsig.get(), class_to_profile.c_str()) == 0) {
		    if (strcmp(method_name.get(), method_to_profile.c_str()) == 0) {
			std::cout << "decision!!!" << std::endl;
			process_method = true;
			goto decision_made;
		    }
		}
		const jvmtiCompiledMethodLoadRecordHeader* comp_info =
		    reinterpret_cast<const jvmtiCompiledMethodLoadRecordHeader*>(compile_info);

		if (comp_info->kind == JVMTI_CMLR_INLINE_INFO) {
		    const jvmtiCompiledMethodLoadInlineRecord* inline_info =
			reinterpret_cast<const jvmtiCompiledMethodLoadInlineRecord*>(compile_info);

		    for (int i = 0; i < inline_info->numpcs; ++i) {
			PCStackInfo* info = &inline_info->pcinfo[i];

			std::string inlined_method;

			for (int i = 0; i < info->numstackframes; ++i) {
			    jmethodID inline_method = info->methods[i];

			    jvmti_ptr<char*> inline_method_name(jvmti);
			    jvmti_ptr<char*> inline_method_sign(jvmti);
			    jvmti->GetMethodName(inline_method, &inline_method_name.get(), &inline_method_sign.get(), nullptr);

			    jclass inline_clazz;
			    jvmti->GetMethodDeclaringClass(inline_method, &inline_clazz);
			    jvmti_ptr<char*> inline_class_sign(jvmti);
			    jvmti->GetClassSignature(inline_clazz, &inline_class_sign.get(), nullptr);

			    if (inline_class_sign.get() == nullptr || inline_method_name.get() == nullptr) {
				std::cout << "NULL???" << std::endl;
				::exit(4);
			    }

			    if (strcmp(inline_class_sign.get(), class_to_profile.c_str()) == 0) {
				if (strcmp(inline_method_name.get(), method_to_profile.c_str()) == 0) {
				    std::cout << "decision!!!" << std::endl;
				    process_method = true;
				    goto decision_made;
				}
			    }
			}
		    }
		}
	    }
	}

    decision_made:
	if (process_method) {
	    
	    ::disassemble(std::cout, code_addr, code_size+1);
	    
	    method_to_profile_t method;
	    method.addr = (void*)code_addr;
	    method.trampoulines = (char*)::mmap(
		nullptr,
		4096,
		PROT_WRITE | PROT_EXEC,
		MAP_PRIVATE | MAP_ANONYMOUS,
		0,
		0);

	    auto it = method.trampoulines;

	    spin_cs cs(atomic_guard);

	    /*
	    auto tr_sz = install_sigill_trampouline((char*)code_addr, it);
	    routine_begin.insert((char*)code_addr);
	    handler_by_addr[code_addr] = it;
	    it += tr_sz;
	    */

	    /*
	    auto rets = get_all_rets((char*)code_addr, code_size + 1);

	    for (const auto& ret : rets) {
		auto tr_sz = install_sigill_trampouline(ret.addr, it);
		routine_end.insert(ret.addr);
		handler_by_addr[ret.addr] = it;
		it += tr_sz;
		}*/
	}
    }

    std::cout << "--------------------------------------------" << std::endl;
}

void JNICALL
cbMethodDynamic(jvmtiEnv *jvmti,
            const char* name,
            const void* address,
            jint length)
{
    std::cout << "dynamic method" << name << "@" << address << ":" << length << std::endl;
}

void JNICALL cbMethodUnload
    (jvmtiEnv *jvmti_env,
     jmethodID method,
     const void* code_addr) {
    std::cout << "Unload: " << method << "@" << std::hex << code_addr << std::endl;
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
    }

    {
	jvmtiEventCallbacks callbacks = {};

	callbacks.CompiledMethodLoad = &cbMethodCompiled;
	callbacks.DynamicCodeGenerated = &cbMethodDynamic;
	callbacks.CompiledMethodUnload = &cbMethodUnload;

	auto status = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
    }

    {
 	jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_LOAD, (jthread)NULL);
	jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_UNLOAD, (jthread)NULL);
	jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, (jthread)NULL);
    }

    {
	auto status = jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD);
	status = jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_UNLOAD);
	status = jvmti->GenerateEvents(JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
    }

    if (options == nullptr) {
	std::cerr << "No parameters" << std::endl;
	::exit(1);
    }

    std::string all_options = options;

    auto pos = all_options.find("::");
    method_to_profile = all_options.substr(pos + 2);
    class_to_profile = all_options.substr(0, pos);

    std::cout << "Method to profile: " << method_to_profile << std::endl;
    std::cout << "Class to profile: " << class_to_profile << std::endl;

    return 0;
}
