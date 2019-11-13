#ifndef _TAR_H_
#define _TAR_H_

#include <vector>
#include <memory>

#include "libtarmod.h"

class SeekableTarEntry : public std::streambuf
{
protected:

	//Override streambuf virtual methods
	std::streamsize xsgetn (char* s, std::streamsize n);
	int uflow();
	std::streamsize showmanyc();
	std::streampos seekpos (std::streampos sp, std::ios_base::openmode which);
	std::streampos seekoff (std::streamoff off, std::ios_base::seekdir way,
                   std::ios_base::openmode which);

	class SeekableTarRead *parentTar;
	size_t entryIndex;
	size_t cursorPos;
	size_t entrySize;
	tar_header *header;

	std::string readBuff;
	size_t readBuffPos;

public:
	SeekableTarEntry(class SeekableTarRead *parentTar, size_t entryIndex);
	virtual ~SeekableTarEntry();

};

class SeekableTarRead
{
public:
	SeekableTarRead(std::streambuf &inStream);
	virtual ~SeekableTarRead();

	int BuildIndex();
	int ExtractByIndex(size_t index, std::streambuf &outStream);
	int ExtractBlocks(size_t index, size_t startBlock, size_t endBlock, std::streambuf &outStream);
	size_t GetEntrySize(size_t index);

	std::shared_ptr<class SeekableTarEntry> GetEntry(size_t index);

	//For internal use
	int _ReadInFunc(const void *buffer, size_t size);
	uint64_t _SeekInFunc(uint64_t pos, int whence);
	int _WriteOutFunc(const void *buffer, size_t size);

	std::streambuf &st;
	std::streambuf *outSt;
	std::vector<tar_header> fileList;
private:

	std::vector<std::streampos> fileInPos;
	TAR *pTar;
	std::string readBuf;
	tartype_t funcs;
};

#endif //_TAR_H_
