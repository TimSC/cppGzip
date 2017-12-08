//g++ ReadGzip.cpp TestGzip.cpp -lz -o readgzip
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include "DecodeGzip.h"
#include "EncodeGzip.h"

using namespace std;

int main()
{
	time_t seed = time( NULL );
	cout << "seed " << seed << endl;
	srand( seed );

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

	cout << "Decode from file" << endl;
	std::string testOut2;
	DecodeGzipQuickFromFilename("test.txt.gz", testOut2);
	cout << testOut2 << endl;

	size_t si = 1024*1024 + rand() % (4*1024*1024);
	string randStr;
	randStr.resize(si);
	for(size_t i=0; i<randStr.size()-1; i++)
		randStr[i] = rand() % 94 + 32; //Random ascii char
	randStr[randStr.size()-1] = '\0';
	cout << "Random string size " << randStr.size() << endl << endl;

	cout << "Encode large random string in one shot" << endl;
	stringbuf encbuff;
	{
		class EncodeGzip encodeGzip(encbuff);
		encodeGzip.sputn(randStr.c_str(), randStr.size());
	}
	string encString = encbuff.str();
	
	cout << "Enc size " << encString.size() << endl;

	cout << "Decode large random string in one shot" << endl;
	stringbuf encString2(encString);
	//encString2.pubseekpos(0);
	class DecodeGzip decodegzip(encString2);	
	char tmpbuff[si+100];
	size_t decLen = decodegzip.sgetn(tmpbuff, sizeof(tmpbuff));
	string decBuff(tmpbuff, decLen);
	cout << "dec size " << decLen << endl;
	if(decBuff == randStr)
		cout << "OK, strings match" << endl << endl;
	else
		cout << "ERROR: strings don't match" << endl << endl;
	
	cout << "Encode large random string in small chunks" << endl;
	stringbuf encbuff2;
	{
		class EncodeGzip encodeGzip(encbuff2);
		size_t i=0;
		while(i<randStr.size())
		{
			size_t chunksize = rand() % 10;
			if(i + chunksize > randStr.size())
				chunksize = randStr.size() - i;

			encodeGzip.sputn(&randStr[i], chunksize);
			i += chunksize;
		}
	}
	string encString3 = encbuff2.str();
	cout << "Enc size " << encString3.size() << endl;
	if(encString == encString3)
		cout << "OK, encoded data matches" << endl;
	else
		cout << "ERROR: encoded data don't match" << endl;

	cout << "Decode large random string in small chunks" << endl;
	stringbuf encString4(encString3);
	//encString4.pubseekpos(0);
	class DecodeGzip decodegzip2(encString4);	
	char tmpbuff2[11];
	string decBuff2;
	while(decodegzip2.in_avail()>0)
	{
		size_t decLen2 = decodegzip2.sgetn(tmpbuff2, rand() % sizeof(tmpbuff2));
		decBuff2.append(tmpbuff2, decLen2);
	}
	cout << "dec size " << decBuff2.size() << endl;
	if(decBuff2 == randStr)
		cout << "OK, strings match" << endl << endl;
	else
		cout << "ERROR: strings don't match" << endl << endl;


}

