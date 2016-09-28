//g++ ReadGzip.cpp TestGzip.cpp -lz -o readgzip
#include <fstream>
#include <iostream>
#include "ReadGzip.h"

using namespace std;

void Test(streambuf &st)
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

int main()
{
	std::filebuf fb;
	fb.open("test.txt.gz", std::ios::in);
	
	class DecodeGzip decodeGzip(fb);
	
	Test(decodeGzip);
}

