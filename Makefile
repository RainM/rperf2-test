all: reader parser


LIBIPT_INCLUDE=-I/home/ruamnsa/dev/ipt/processor-trace/libipt/internal/include/ -I/home/ruamnsa/dev/ipt/processor-trace/build/libipt/include/ 
LIBXED_INCLUDE=-I/home/ruamnsa/dev/ipt/xed/obj/

LIBXED_LIBRARY=-L/home/ruamnsa/dev/ipt/xed/obj/ -lxed -L/home/ruamnsa/dev/ipt/processor-trace/build/lib/ -lipt

reader:
	g++ -std=c++11 -pedantic -O0 -g perf_event_reader.cpp -c -o perf_event_reader.o
	g++ -std=c++11 -pedantic -O0 -g pt_parser.cpp -c -o pt_parser.o $(LIBIPT_INCLUDE) $(LIBXED_INCLUDE)
	g++ -o parser perf_event_reader.o pt_parser.o $(LIBXED_LIBRARY)
