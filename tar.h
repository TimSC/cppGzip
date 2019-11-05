#ifndef _TAR_H_
#define _TAR_H_

#include <vector>

#include "libtarmod.h"

class SeekableTarRead
{
public:
	SeekableTarRead(std::streambuf &inStream);
	virtual ~SeekableTarRead();

	int BuildIndex();
	int ExtractByIndex(size_t index, std::streambuf &outStream);

	//For internal use
	int _ReadInFunc(const void *buffer, size_t size);
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
