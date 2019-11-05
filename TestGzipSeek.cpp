//g++ ReadGzip.cpp TestGzip.cpp -lz -o readgzip
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include "DecodeGzip.h"
#include "EncodeGzip.h"

using namespace std;

void RunGzipSeekTests()
{
	time_t seed = time( NULL );

	cout << "seed " << seed << endl;
	srand( seed );
	const int testReadSize = 64 + rand() % 1024;

	size_t si = testReadSize + rand() % (20*1024*1024);;
	string randStr;
	randStr.resize(si);
	if((rand() % 2) == 0)
	{
		for(size_t i=0; i<randStr.size()-1; i++)
			randStr[i] = rand() % 94 + 32; //Random ascii char
		randStr[randStr.size()-1] = '\0';
	}
	else
	{
		for(size_t i=0; i<randStr.size()-1; i++)
			randStr[i] = rand() % 256; //Random byte
	}
	cout << "Random string size " << randStr.size() << endl << endl;

	cout << "Encode large random string in one shot" << endl;
	stringbuf encbuff;
	{
		class EncodeGzip encodeGzip(encbuff);
		encodeGzip.sputn(randStr.c_str(), randStr.size());
	}
	string encString = encbuff.str();
	
	cout << "Enc size " << encString.size() << endl;
/*
	std::filebuf fb;
	fb.open("output.gz", std::ios::out | std::ios::binary);
	fb.sputn(encString.c_str(), encString.size());

	std::filebuf testIn;
	testIn.open("output.gz", std::ios::in | std::ios::binary );
*/
	cout << "Decode large random string in small chunks" << endl;
	stringbuf encStringBuff(encString);

	class DecodeGzip decodegzip2(encStringBuff);
	int tmpBuffSize = 1 + (rand() % 5000);
	int tmpBuffSize2 = 1 + (rand() % 5000);
	char tmpbuff[tmpBuffSize], tmpbuff2[tmpBuffSize2];
	string decBuff2;

	while(decodegzip2.in_avail()>0)
	{
		size_t decLen2 = decodegzip2.sgetn(tmpbuff, rand() % (sizeof(tmpbuff)+1));
		decBuff2.append(tmpbuff, decLen2);
	}
	cout << "dec size " << decBuff2.size() << endl;
	if(decBuff2 != randStr)
	{
		cout << "Decoded data doesn't match" << endl;
		exit(0);
	}

	if(rand() % 2 == 0)
	{
		//Do random seek to test functionality
		int randSeekPos = rand() % si;
		decodegzip2.pubseekpos(randSeekPos);
	}

	//Try naive seek
	int randSeekPos = rand() % si;
	if(randSeekPos >= si - testReadSize)
		randSeekPos = si - testReadSize;

	cout << "seek pos " << decodegzip2.pubseekpos(randSeekPos) << endl;

	string decBuff3;
	while(decodegzip2.in_avail()>0 and decBuff3.length()<testReadSize)
	{
		size_t decLen2 = decodegzip2.sgetn(tmpbuff, rand() % (sizeof(tmpbuff)+1));
		decBuff3.append(tmpbuff, decLen2);
	}
	cout << "dec size " << decBuff3.size() << endl;

	int curPos = decodegzip2.pubseekoff(0, ios_base::cur);
	if (curPos != randSeekPos + decBuff3.size()) cout << "Seek ended in wrong position" << endl;

	encStringBuff.pubseekpos(0);
	DecodeGzipIndex index;
	CreateDecodeGzipIndex(encStringBuff, index);
	cout << "index size " << index.size() << endl;

	//Try fast seek
	encStringBuff.pubseekpos(0);
	class DecodeGzipFastSeek decfs(encStringBuff, index);

	if(rand() % 2 == 0)
	{
		//Do random seek to test functionality
		int randSeekPos = rand() % si;
		decfs.pubseekpos(randSeekPos);
	}

	cout << "seek pos " << decfs.pubseekpos(randSeekPos) << endl;

	string decBuff4;
	while(decfs.in_avail()>0 and decBuff4.length()<testReadSize)
	{
		size_t decLen2 = decfs.sgetn(tmpbuff2, rand() % (sizeof(tmpbuff2)+1));
		decBuff4.append(tmpbuff2, decLen2);
	}
	cout << "dec size " << decBuff4.size() << endl;
	
	curPos = decfs.pubseekoff(0, ios_base::cur);
	if (curPos != randSeekPos + decBuff4.size()) cout << "Seek ended in wrong position" << endl;

	//Cut random sections to common size and compare
	decBuff3 = decBuff3.substr(0, testReadSize);
	decBuff4 = decBuff4.substr(0, testReadSize);
	int compareResult = (decBuff3 == decBuff4);
	cout << "Compare sections decoded using two methods: " << compareResult << endl;
	if(!compareResult) exit(0);
}
	
int main()
{
	while(1) RunGzipSeekTests();
}

