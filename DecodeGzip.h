#ifndef _DECODE_GZIP_H
#define _DECODE_GZIP_H

#include <zlib.h>
#include <streambuf>
#include <vector>
#define DEC_MAGIC_NUM_FOR_GZIP 16

typedef std::vector<class DecodeGzipPoint> DecodeGzipIndex;

class DecodeGzipPoint
{
public:
	size_t bytesDecodedIn;
	size_t bytesDecodedOut;
	int bits;
	std::string window;
	bool fileEnd;

	DecodeGzipPoint();
	DecodeGzipPoint(const DecodeGzipPoint &obj);
	virtual ~DecodeGzipPoint();
};

class DecodeGzip : public std::streambuf
{
protected:
	char *readBuff;
	char *decodeBuff;
	std::streambuf &inStream;
	z_stream d_stream;
	bool decodeDone;
	char *decodeBuffCursor;
	std::streamsize readBuffSize, decodeBuffSize;
	int windowBits;

	//Index building parameters
	size_t bytesDecodedIn;
	size_t bytesDecodedOut;
	size_t lastAccessBytes;

	std::string decodeHistoryBuff;
	size_t historyBytesToStore;
	size_t maxHistoryBuffSize;

	void Decode();
	std::streampos SkimToStreamPos(std::streampos start, std::streampos sp);

	//Override streambuf virtual methods
	std::streamsize xsgetn (char* s, std::streamsize n);
	int uflow();
	std::streamsize showmanyc();
	std::streampos seekpos (std::streampos sp, std::ios_base::openmode which);
	std::streampos seekoff (std::streamoff off, std::ios_base::seekdir way,
                   std::ios_base::openmode which);

public:
	DecodeGzip(std::streambuf &inStream, 
		std::streamsize readBuffSize = 1024*128, std::streamsize decodeBuffSize = 1024*128,
		int windowBits = MAX_WBITS+DEC_MAGIC_NUM_FOR_GZIP);
	virtual ~DecodeGzip();

	//These should be ignored except while building an index for random access
	bool buildIndex;
	DecodeGzipIndex *indexOut;
	size_t spanBetweenAccess;

};

class DecodeGzipFastSeek : public DecodeGzip
{
public:
	DecodeGzipFastSeek(std::streambuf &inStream, 
		const DecodeGzipIndex &index,
		std::streamsize readBuffSize = 1024*128, std::streamsize decodeBuffSize = 1024*128,
		int windowBits = MAX_WBITS+DEC_MAGIC_NUM_FOR_GZIP);
	virtual ~DecodeGzipFastSeek();

protected:
	const DecodeGzipIndex index;
	
	std::streampos seekpos (std::streampos sp, std::ios_base::openmode which);
	std::streampos seekoff (std::streamoff off, std::ios_base::seekdir way,
                   std::ios_base::openmode which);
};

///One shot decoding of small files
void DecodeGzipQuick(std::streambuf &fb, std::streambuf &out);
void DecodeGzipQuick(std::string &data, std::string &out);
void DecodeGzipQuick(std::string &data, std::streambuf &outBuff);
void DecodeGzipQuick(std::streambuf &inBuff, std::string &out);
void DecodeGzipQuickFromFilename(const std::string &fina, std::string &out);

std::streamsize CreateDecodeGzipIndex(std::streambuf &inStream, 
	DecodeGzipIndex &out,
	std::streamsize readBuffSize = 1024*128, std::streamsize decodeBuffSize = 1024*128,
			int windowBits = MAX_WBITS+DEC_MAGIC_NUM_FOR_GZIP);

#endif //_DECODE_GZIP_H

