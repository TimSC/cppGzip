all:
	g++ DecodeGzip.cpp EncodeGzip.cpp TestGzip.cpp -Wall -lz -o testgzip

