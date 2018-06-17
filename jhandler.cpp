#include <jni.h>
#include <jvmti.h>
#include <jvmticmlr.h>

#include <iostream>
#include <vector>
#include <unorderred_map>
#include <map>

#include "hw_bp.hpp"

struct method_to_profile {
    void* addr;
};

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

		{
		    const jvmtiCompiledMethodLoadRecordHeader* comp_info = reinterpret_cast<const jvmtiCompiledMethodLoadRecordHeader*>(compile_info);


		    if (comp_info->kind == JVMTI_CMLR_INLINE_INFO) {
			const jvmtiCompiledMethodLoadInlineRecord* inline_info = reinterpret_cast<const jvmtiCompiledMethodLoadInlineRecord*>(compile_info);
			for (int i = 0; i < inline_info->numpcs; ++i) {
			    PCStackInfo* info = &inline_info->pcinfo[i];

			    std::string inlined_method;
			    
			    for (int i = 0; i < info->numstackframes; ++i) {
				jmethodID inline_method = info->methods[i];
				std::cout << "methodID: " << method << std::endl;

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

				std::cout << inline_class_sign << "->" << inline_method_name << inline_method_sign << std::endl;

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
