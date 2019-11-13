#include <iostream>
#include <fstream>
#include <sstream>
#include <time.h>
#include "tar.h"
#include "DecodeGzip.h"
using namespace std;

int main(int argc, char *argv[])
{
	const char default_infi[] = "test.tar.gz";
	const char *infi = default_infi;
	if(argc > 1)
		infi = argv[1];

	time_t seed = time( NULL );
	srand( seed );

	std::filebuf testIn;
	testIn.open(infi, std::ios::in | std::ios::binary );
	if (!testIn.is_open())
	{
		cout << "Error opening input file" << endl;
		exit(-1);
	}

	DecodeGzipIndex index;
	CreateDecodeGzipIndex(testIn, index);
	testIn.pubseekpos(0);
	class DecodeGzipFastSeek decodeGzip(testIn, index);

	class SeekableTarRead seekableTarRead(decodeGzip);

	cout << "Building Index" << endl;
	int ret = seekableTarRead.BuildIndex();
	if(ret != 0)
	{
		cout << "Failed" << endl; exit(0);
	}
	cout << "Done!" << endl;
	
	//Extract a random file from archive
	if(seekableTarRead.fileList.size()>0)
	{
		for(int i=0;i<100;i++)
		{
			int index = rand()%seekableTarRead.fileList.size();
			cout << "ExtractByIndex " << index << "," << seekableTarRead.fileList[index].name << endl;
			stringbuf buffWrap;
			cout << "ret " << seekableTarRead.ExtractByIndex(index, buffWrap) << endl;
			cout << "size " << buffWrap.str().size() << endl;
		}
	}
}
