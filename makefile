
all: testgzip testgzip testgzipseek testtar

%.o: %.cpp
	g++ -fPIC -Wall -c -std=c++11 -o $@ $<
%.o: %.c
	gcc -fPIC -Wall -c -o $@ $<

testgzip: DecodeGzip.o EncodeGzip.o TestGzip.cpp
	g++ $^ -Wall -std=c++11 -lz -o $@
testdeflate: DecodeGzip.o EncodeGzip.o TestDeflate.cpp
	g++ $^ -Wall -std=c++11 -lz -o $@
testgzipseek: DecodeGzip.o EncodeGzip.o TestGzipSeek.cpp
	g++ $^ -Wall -std=c++11 -lz -o $@
testtar: tar.o libtarmod.o libtarmod_hash.o libtarmod_list.o TestTar.o
	g++ $^ -std=c++11 -lbsd -Wall -o $@

