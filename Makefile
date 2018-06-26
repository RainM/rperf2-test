JNI_INCLUDE=-I/usr/lib/jvm/java-8-openjdk/include/ -I/usr/lib/jvm/java-8-openjdk/include/linux/

build-java-agent:
	(cd javaagent; mvn package)

javaagent/target/classes: build-java-agent

generate-jni: javaagent/target/classes
	javah -cp javaagent/target/classes/ ru.raiffeisen.PerfPtProf

build-jni:
	g++ -std=c++11 -O0 -g -pedantic jni_impl.cpp jvmti_agent.cpp -shared $(JNI_INCLUDE) -o librperf2.so -fpic

java-app-agent:
	javac JTest.java
	-LD_LIBRARY_PATH=.  java  \
		-DTRIGGER_METHOD=__do \
		-DTRIGGER_CLASS=JTest \
		-DTRIGGER_COUNTDOWN=10000 \
		-DDUMP_OUT_CLASSES=true \
		-DOUTPUT_INSTRUMENTED_CLASSES=1 \
		-DPERCENTILE=0.999999 \
		-agentlib:rperf2 \
		-javaagent:javaagent/target/instrumenter-1.2-SNAPSHOT-jar-with-dependencies.jar \
		-XX:+UnlockDiagnosticVMOptions \
		-cp . JTest
