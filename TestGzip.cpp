//g++ ReadGzip.cpp TestGzip.cpp -lz -o readgzip
#include <fstream>
#include <iostream>
#include "DecodeGzip.h"
#include "EncodeGzip.h"

using namespace std;

void TestDec(streambuf &st)
{
	int testBuffSize = 1024*88;
	char buff[testBuffSize];
	ofstream testOut("testout.txt", ios::binary);
	while(st.in_avail()>0)
	{
		//cout << st.in_avail() << endl;
		int len = st.sgetn(buff, testBuffSize-1);
		buff[len] = '\0';
		cout << len << ", " << testBuffSize-1 << endl;
		
		//cout << buff;
		testOut.write(buff, len);
	}
	testOut.flush();
}

void TestEnc(streambuf &st)
{
	int testBuffSize = 1024*77;
	char buff[testBuffSize];
	ofstream testOut("output.txt.gz", ios::binary);
	unsigned outlen = 0;
	while(st.in_avail()>0)
	{
		//cout << st.in_avail() << endl;
		int len = st.sgetn(buff, testBuffSize-1);
		buff[len] = '\0';

		//cout << buff;
		testOut.write(buff, len);
		outlen += len;
		cout << len << "\t" << testBuffSize-1 << "\t" << outlen << endl;
		
	}
	testOut.flush();
}

int main()
{
	//Perform decoding
	std::filebuf fb;
	fb.open("test.txt.gz", std::ios::in | std::ios::binary);
	
	class DecodeGzip decodeGzip(fb);
	TestDec(decodeGzip);

	//Perform encoding
	std::filebuf fb2;
	fb2.open("input.txt", std::ios::in | std::ios::binary);
	
	class EncodeGzip encodeGzip(fb2, Z_DEFAULT_COMPRESSION);
	TestEnc(encodeGzip);
}

