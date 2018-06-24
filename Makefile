all: reader parser


LIBIPT_INCLUDE=-I/home/ruamnsa/dev/ipt/processor-trace/libipt/internal/include/ -I/home/ruamnsa/dev/ipt/processor-trace/build/libipt/include/ 
LIBXED_INCLUDE=-I/home/ruamnsa/dev/ipt/xed/obj/ -I/home/sergey/dev/xed/include/public/ -I/home/sergey/dev/xed/obj/ -I/home/sergey/dev/xed/include/public/xed/

LIBXED_LIBRARY=-L/home/sergey/dev/xed/obj/ -lxed
LIBIPT_LIBRARY=-L/home/ruamnsa/dev/ipt/xed/obj/ -lxed -L/home/ruamnsa/dev/ipt/processor-trace/build/lib/ -lipt

reader:
	g++ -std=c++11 -pedantic -O0 -g perf_event_reader.cpp -c -o perf_event_reader.o
	g++ -std=c++11 -pedantic -O0 -g pt_parser.cpp -c -o pt_parser.o $(LIBIPT_INCLUDE) $(LIBXED_INCLUDE)
	g++ -o parser perf_event_reader.o pt_parser.o $(LIBXED_LIBRARY)

JNI_INCLUDE=-I/usr/lib/jvm/java-8-openjdk/include/ -I/usr/lib/jvm/java-8-openjdk/include/linux/

jhandler:
	g++ jhandler.cpp -std=c++11 -Wall -pedantic -O2 -c -o jhandler.o -fpic $(JNI_INCLUDE) $(LIBXED_INCLUDE)
	g++ xed_driver.cpp -std=c++11 -pedantic -O0 -g -c -o xed_driver.o -fpic $(LIBXED_INCLUDE)
	g++ hw_bp.cpp -std=c++11 -pedantic -Wall -O0 -g -c -o hw_bp.o -fpic
	g++ -o libjhandler.so jhandler.o hw_bp.o xed_driver.o $(LIBXED_LIBRARY) -shared 

java-app-agent:
	javac JTest.java
	-LD_LIBRARY_PATH=.:/home/sergey/dev/xed/obj/  java '-agentlib:jhandler=LJTest;::foo' \
		-XX:+UnlockDiagnosticVMOptions \
		-XX:-PrintAssembly \
		-XX:CompileCommand="dontinline JTest::foo" \
		-cp . JTest

java-app:
	-java  -XX:CompileCommand="dontinline JTest::foo" -cp . JTest

hwbp-example:
	g++ hw_bp.cpp -std=c++11 -pedantic -Wall -O0 -g -c -o hw_bp.o -fpic
	g++ hw_bp_sample.cpp -std=c++11 -pedantic -Wall -O0 -g -c -o hw_bp_sample.o -fpic
	g++ -o hw_bp hw_bp.o hw_bp_sample.o

xed-example:
	g++ hw_bp.cpp -std=c++11 -pedantic -Wall -O0 -g -c -o hw_bp.o -fpic
	g++ xed_driver.cpp -std=c++11 -pedantic -O0 -g -c -o xed_driver.o -fpic $(LIBXED_INCLUDE)
	g++ xed_sample.cpp -std=c++11 -pedantic -O0 -g -c -o xed_sample.o -fpic
	g++ xed_sample2.cpp -std=c++11 -pedantic -O0 -g -c -o xed_sample2.o -fpic
	g++ -o xed_sample hw_bp.o xed_driver.o xed_sample.o -lpthread $(LIBXED_LIBRARY)
	g++ -o xed_sample2 hw_bp.o xed_driver.o xed_sample2.o -lpthread $(LIBXED_LIBRARY)

sigill_example:
	g++ sigill_overwrite.cpp -fpic -std=c++11 -pedantic -O0 -g -o sigill_sample
