
all: testgzip testgzip testgzipseek testtar testtargz

%.o: %.cpp
	g++ -fPIC -Wall -c -g -std=c++11 -o $@ $<
%.o: %.c
	gcc -fPIC -Wall -c -g -o $@ $<

testgzip: DecodeGzip.o EncodeGzip.o TestGzip.cpp
	g++ $^ -Wall -std=c++11 -lz -o $@
testdeflate: DecodeGzip.o EncodeGzip.o TestDeflate.cpp
	g++ $^ -Wall -std=c++11 -lz -o $@
testgzipseek: DecodeGzip.o EncodeGzip.o TestGzipSeek.cpp
	g++ $^ -Wall -std=c++11 -lz -o $@
testtar: tar.o libtarmod.o libtarmod_hash.o libtarmod_list.o TestTar.o
	g++ $^ -std=c++11 -lbsd -Wall -o $@
testtargz: tar.o libtarmod.o libtarmod_hash.o libtarmod_list.o TestTarGz.o DecodeGzip.o
	g++ $^ -std=c++11 -lbsd -lz -g -Wall -o $@

