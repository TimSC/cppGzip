#include <iostream>
#include <fstream>
#include <sstream>
#include <time.h>
#include "SeekableTar.h"
using namespace std;

void TestTar(const char *infi)
{
	time_t seed = time( NULL );
	srand( seed );
	cout << "seed " << seed << endl;

	std::filebuf testIn;
	testIn.open(infi, std::ios::in | std::ios::binary );
	if (!testIn.is_open())
	{
		cout << "Error opening input file" << endl;
		exit(-1);
	}

	class SeekableTarRead seekableTarRead(testIn);

	cout << "Building Index" << endl;
	int ret = seekableTarRead.BuildIndex();
	if(ret != 0)
	{
		cout << "Failed" << endl; exit(0);
	}
	cout << "Done! found " << seekableTarRead.fileList.size() << " regular files" << endl;
	
	//Extract random files from tar archive
	if(seekableTarRead.fileList.size()>0)
	{
		for(int i=0;i<3;i++)
		{
			int index = rand()%seekableTarRead.fileList.size();
			cout << "ExtractByIndex " << index << "," << seekableTarRead.fileList[index].name << endl;
			stringbuf buffWrap;
			cout << "ret " << seekableTarRead.ExtractByIndex(index, buffWrap) << endl;
			string buffWrapStr = buffWrap.str();
			cout << "size " << buffWrapStr.size() << endl;

			size_t fiSize = seekableTarRead.GetEntrySize(index);
			cout << "expected size " << fiSize << endl;			
			if(buffWrapStr.size() != fiSize) exit(0);

			//Test extracting tar blocks
			size_t numBlocks = fiSize / T_BLOCKSIZE;
			size_t startBlock = rand() % numBlocks;
			size_t endBlock = startBlock + (rand() % (numBlocks - startBlock)) + 1;

			stringbuf buffWrap2;
			cout << "ret " << seekableTarRead.ExtractBlocks(index, startBlock, endBlock, buffWrap2) << endl;
			string buffWrap2Str = buffWrap2.str();
			cout << "blocks size " << buffWrap2Str.size() << endl;

			size_t expectedBlocksSize = (endBlock - startBlock) * T_BLOCKSIZE;
			size_t bytesRemaining = fiSize - (startBlock * T_BLOCKSIZE);
			if(expectedBlocksSize > bytesRemaining)
				expectedBlocksSize = bytesRemaining;
			cout << "expected size " << expectedBlocksSize << endl;
			if(buffWrap2Str.size() != expectedBlocksSize) exit(0);

			std::string subs = buffWrapStr.substr(startBlock * T_BLOCKSIZE, expectedBlocksSize);
			int chk = subs == buffWrap2Str;
			cout << "compare extracted blocks " << chk << endl;
			if(buffWrap2Str.size() != expectedBlocksSize) exit(0);

			//Test buffer interface to tar entries
			std::shared_ptr<class SeekableTarEntry> entry = seekableTarRead.GetEntry(index);

			for(int j=0; j<3; j++)
			{
				size_t startPos = rand() % fiSize;
				size_t endPos = startPos + (rand() % (1024 * 1024));
				if(endPos > fiSize)
					endPos = fiSize;
				size_t buffSize = endPos - startPos;
				
				streampos pos = entry->pubseekoff(0, ios_base::cur);

				switch(rand() % 3)
				{
				case 0:
					entry->pubseekpos(startPos);
					break;
				case 1:
					entry->pubseekoff(startPos, ios_base::beg);
					break;
				case 2:
					entry->pubseekoff((streamoff)startPos - (streamoff)pos, ios_base::cur);
					break;
				}
				string readBuff;

				while (readBuff.size() < buffSize)
				{
					char tmpBuff[buffSize];
					int count = entry->sgetn(tmpBuff, buffSize - readBuff.size());
					readBuff.append(tmpBuff, count);
				}

				cout << "Check buffer interface size " << (readBuff.size() == buffSize) << endl;
				if(readBuff.size() != buffSize) exit(0);
				subs = buffWrapStr.substr(startPos, buffSize);
				cout << "Check buffer interface data " << (subs == readBuff) << endl;
				if(subs != readBuff) exit(0);
			}
		
		}
	}
}

int main(int argc, char *argv[])
{
	const char default_infi[] = "test.tar";
	const char *infi = default_infi;
	if(argc > 1)
		infi = argv[1];

	while(1)
		TestTar(infi);
}

