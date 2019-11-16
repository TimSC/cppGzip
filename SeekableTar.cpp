#include <iostream>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <string.h>
#include "SeekableTar.h"

using namespace std;

// ***********************************

SeekableTarEntry::SeekableTarEntry(class SeekableTarRead *parentTar, size_t entryIndex):
	std::streambuf(),
	parentTar(parentTar),
	entryIndex(entryIndex)
{
	cursorPos = 0;
	header = &parentTar->fileList[entryIndex];
	entrySize = th_get_size2(parentTar->fileList[entryIndex]);
	readaheadBlocks = (10 * 1024) / T_BLOCKSIZE;
}

SeekableTarEntry::~SeekableTarEntry()
{

}

std::streamsize SeekableTarEntry::xsgetn (char* s, std::streamsize n)
{
	std::streamsize avail = entrySize - cursorPos;
	if(avail == 0) return 0;
	if(n == 0) return 0;

	//See if read buffer can help, so we don't need to get more data
	streamoff readBufferEndPos = readBuffPos + readBuff.size();
	if(readBuff.size() > 0 && cursorPos >= readBuffPos && (streamoff)cursorPos < readBufferEndPos - 1)
	{
		size_t posInBuffer = cursorPos - this->readBuffPos;
		size_t bytesRemaining = readBuff.size() - posInBuffer;
		size_t bytesToCopy = n;
		if(bytesToCopy > bytesRemaining)
			bytesToCopy = bytesRemaining;		

		memcpy(s, &this->readBuff[posInBuffer], bytesToCopy);
		this->cursorPos += bytesToCopy;
		return bytesToCopy;
	}

	//Read more into read buffer
	size_t startBlock = cursorPos / T_BLOCKSIZE;
	size_t endBlock = startBlock + readaheadBlocks;
	std::stringbuf blocks;
	parentTar->ExtractBlocks(this->entryIndex, startBlock, endBlock, blocks);
	this->readBuff = blocks.str();
	this->readBuffPos = startBlock * T_BLOCKSIZE;
	if(cursorPos < this->readBuffPos)
		throw runtime_error("Internal error seeking tar");

	//Get subset of read buffer for output
	size_t posInBuffer = cursorPos - this->readBuffPos;
	size_t bytesRemaining = this->readBuff.size() - posInBuffer;	
	size_t bytesToCopy = n;
	if(bytesToCopy > bytesRemaining)
		bytesToCopy = bytesRemaining;
	
	memcpy(s, &this->readBuff[posInBuffer], bytesToCopy);
	this->cursorPos += bytesToCopy;
	return bytesToCopy;
}

int SeekableTarEntry::uflow()
{
	streamsize inputReady = showmanyc();
	if(inputReady==0) return EOF;
	char buff[1];
	xsgetn(buff, 1);
	return *(unsigned char *)&(buff[0]);
}

std::streamsize SeekableTarEntry::showmanyc()
{
	return entrySize - cursorPos;
}

std::streampos SeekableTarEntry::seekpos (std::streampos sp, std::ios_base::openmode which)
{
	if(sp > entrySize)
		sp = entrySize;
	this->cursorPos = sp;
	return this->cursorPos;
}

std::streampos SeekableTarEntry::seekoff (std::streamoff off, std::ios_base::seekdir way,
               std::ios_base::openmode which)
{
	if(way == ios_base::beg)
	{
		if(off < 0)
			return this->seekpos (0, which);
		return this->seekpos (off, which);
	}
	if(way == ios_base::cur)
	{
		if(off == 0)
			return this->cursorPos;
		std::streamoff pos = (std::streamoff)this->cursorPos + off;
		if(pos < 0)
			pos = 0;
		return this->seekpos (pos, which);
	}
	if(way == ios_base::end)
	{
		std::streamoff pos = (std::streamoff)this->entrySize + off;
		if(pos < 0)
			pos = 0;
		return this->seekpos (pos, which);
	}

	return -1;
}

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

uint64_t seektar_lseekfunc(void *ptr, void *handle, uint64_t offset, int whence)
{
	class SeekableTarRead *obj = (class SeekableTarRead *)(ptr);
	uint64_t ret = obj->_SeekInFunc(offset, whence);
	return ret;
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

ssize_t seektar_stringwritefunc(void *ptr, void *handle, const void *buffer, size_t size)
{
	std::streambuf *outStream = (std::streambuf *)handle;
	return outStream->sputn((char *)buffer, size);
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
	funcs.seekfunc = seektar_lseekfunc;

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

int SeekableTarRead::ExtractBlocks(size_t index, size_t startBlock, size_t endBlock, std::streambuf &outStream)
{
	this->outSt = &outStream;
	const tar_header &th = fileList[index];
	pTar->th_buf = th;
	size_t startPos = startBlock * T_BLOCKSIZE;
	st.pubseekpos(fileInPos[index] + startPos);

	unsigned int fileSize = th_get_size(pTar);
	if (startPos >= fileSize)
		return -1; //Position is after end of file

	size_t bytesRemain = fileSize - startPos;
	size_t bytesToGet = (endBlock - startBlock) * T_BLOCKSIZE;
	if(bytesToGet > bytesRemain)
		bytesToGet = bytesRemain;

	int ok = tar_extract_regfile_blocks(pTar, (void *)&outStream, 
		bytesToGet, seektar_stringwritefunc);

	outSt = nullptr;
	return ok;
}

size_t SeekableTarRead::GetEntrySize(size_t index)
{
	return th_get_size2(fileList[index]);
}

std::shared_ptr<class SeekableTarEntry> SeekableTarRead::GetEntry(size_t index)
{
	return make_shared<class SeekableTarEntry>(this, index);
}

int SeekableTarRead::_ReadInFunc(const void *buffer, size_t size)
{
	return st.sgetn((char*)buffer, size);
}

uint64_t SeekableTarRead::_SeekInFunc(uint64_t pos, int whence)
{
	streampos posOut = st.pubseekoff(pos, (ios_base::seekdir)whence);
	return posOut;
}

int SeekableTarRead::_WriteOutFunc(const void *buffer, size_t size)
{
	if(outSt == nullptr)
		throw logic_error("out string is null");
	return outSt->sputn((const char*)buffer, size);
}

