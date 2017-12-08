all:
	g++ DecodeGzip.cpp EncodeGzip.cpp TestGzip.cpp -Wall -lz -g -o testgzip
	g++ DecodeGzip.cpp EncodeGzip.cpp TestDeflate.cpp -Wall -lz -g -o testdeflate

