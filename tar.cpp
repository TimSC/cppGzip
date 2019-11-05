#include <iostream>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include "tar.h"

using namespace std;

// **** Callbacks ****

void * seektar_openfunc(void *ptr, const char *filename, int flags, mode_t mode, ...)
{
	return (void *)1;
}

int seektar_closefunc(void *ptr, void *handle)
{
	return 0;
}

ssize_t seektar_readfunc(void *ptr, void *handle, void *buffer, size_t size)
{
	class SeekableTarRead *obj = (class SeekableTarRead *)(ptr);
	int ret = obj->_ReadInFunc(buffer, size);
	return ret;
}

int seektar_mkdirfunc(void *opaque, const char *pathname, mode_t mode)
{
	return 0;
}

void * seektar_outopenfunc(void *ptr, const char *filename, int flags, mode_t mode, ...)
{
	return (void *)1;
}

int seektar_outclosefunc(void *ptr, void *handle)
{
	return 0;
}

ssize_t seektar_outwritefunc(void *ptr, void *handle, const void *buffer, size_t size)
{
	class SeekableTarRead *obj = (class SeekableTarRead *)(ptr);
	return obj->_WriteOutFunc(buffer, size);
}

// ***********************************************************

SeekableTarRead::SeekableTarRead(std::streambuf &inStream):st(inStream)
{
	pTar = nullptr;
	outSt = nullptr;

	tar_init_type(&funcs);
	funcs.openfunc = seektar_openfunc;
	funcs.closefunc = seektar_closefunc;
	funcs.readfunc = seektar_readfunc;

	funcs.mkdirfunc = seektar_mkdirfunc;

	funcs.outopenfunc = seektar_outopenfunc;
	funcs.outclosefunc = seektar_outclosefunc;
	funcs.outwritefunc = seektar_outwritefunc;
	
	int ret = tar_open(&pTar, "", &funcs, O_RDONLY, 0, 0, this);
	if(ret != 0)
		throw runtime_error("Error opening tar");
}

SeekableTarRead::~SeekableTarRead()
{
	if(pTar)
		tar_close(pTar);
}

int SeekableTarRead::BuildIndex()
{
	//Build index for quick access
	int i=0;	
	while ((i = th_read(pTar)) == 0)
	{
		if(TH_ISREG(pTar))
		{
			fileList.push_back(pTar->th_buf);

			streampos pos = st.pubseekoff (0, ios_base::cur);

			fileInPos.push_back(pos);
			if (tar_skip_regfile(pTar) != 0) return -1;
		}
	}
	return 0;
}

int SeekableTarRead::ExtractByIndex(size_t index, std::streambuf &outStream)
{
	this->outSt = &outStream;
	const tar_header &th = fileList[index];
	pTar->th_buf = th;
	st.pubseekpos(fileInPos[index]);

	int ret = tar_extract_regfile(pTar, pTar->th_buf.name);
	outSt = nullptr;
	return ret;
}

int SeekableTarRead::_ReadInFunc(const void *buffer, size_t size)
{
	return st.sgetn((char*)buffer, size);
}

int SeekableTarRead::_WriteOutFunc(const void *buffer, size_t size)
{
	if(outSt == nullptr)
		throw logic_error("out string is null");
	return outSt->sputn((const char*)buffer, size);
}

