#include <iostream>
#include <fstream>
#include <sstream>
#include <time.h>
#include "tar.h"
using namespace std;

int main(int argc, char *argv[])
{
	const char default_infi[] = "test.tar";
	const char *infi = default_infi;
	if(argc > 1)
		infi = argv[1];

	time_t seed = time( NULL );
	srand( seed );

	std::filebuf testIn;
	testIn.open(infi, std::ios::in | std::ios::binary );

	class SeekableTarRead seekableTarRead(testIn);

	cout << "Building Index" << endl;
	seekableTarRead.BuildIndex();
	cout << "Done!" << endl;
	
	//Extract a random file from archive
	if(seekableTarRead.fileList.size()>0)
	{
		for(int i=0;i<3;i++)
		{
			int index = rand()%seekableTarRead.fileList.size();
			cout << "ExtractByIndex " << index << "," << seekableTarRead.fileList[index].name << endl;
			stringbuf buffWrap;
			cout << "ret " << seekableTarRead.ExtractByIndex(index, buffWrap) << endl;
			cout << "size " << buffWrap.str().size() << endl;
		}
	}
}
