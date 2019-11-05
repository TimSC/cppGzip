#include <iostream>
#include <fstream>
#include <sstream>
#include <time.h>
#include "tar.h"
using namespace std;

int main()
{
	time_t seed = time( NULL );
	srand( seed );

	std::filebuf testIn;
	testIn.open("test.tar", std::ios::in | std::ios::binary );

	class SeekableTarRead seekableTarRead(testIn);

	seekableTarRead.BuildIndex();
	
	//Extract a random file from archive
	if(seekableTarRead.fileList.size()>0)
	{
		int index = rand()%seekableTarRead.fileList.size();
		cout << "ExtractByIndex " << index << "," << seekableTarRead.fileList[index].name << endl;
		stringbuf buffWrap;
		cout << "ret " << seekableTarRead.ExtractByIndex(index, buffWrap) << endl;
		cout << "size " << buffWrap.str().size() << endl;
	}
}
