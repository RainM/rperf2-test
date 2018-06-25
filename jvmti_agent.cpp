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

JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {

    std::cout << "Hello world from JVMTI" << std::endl;
 
    return 0;
}
