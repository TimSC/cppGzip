//g++ ReadGzip.cpp TestGzip.cpp -lz -o readgzip
#include <fstream>
#include <iostream>
#include "DecodeGzip.h"
#include "EncodeGzip.h"

using namespace std;

int main()
{
	//Perform decoding
	cout << "Decode" << endl;
	std::filebuf fb;
	fb.open("test.txt.gz", std::ios::in | std::ios::binary);
	std::filebuf testOut;
	testOut.open("testout.txt", std::ios::out);
	
	DecodeGzipQuick(fb, testOut);

	//Perform encoding
	cout << "Encode" << endl;
	std::filebuf testIn;
	testIn.open("input.txt", std::ios::in );
	std::filebuf fb2;
	fb2.open("output.txt.gz", std::ios::out | std::ios::binary);
	
	EncodeGzipQuick(testIn, fb2);
}

