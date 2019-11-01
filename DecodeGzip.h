#ifndef _DECODE_GZIP_H
#define _DECODE_GZIP_H

#include <zlib.h>
#include <streambuf>
#include <vector>
#define DEC_MAGIC_NUM_FOR_GZIP 16

class DecodeGzipPoint
{
public:
	size_t bytesDecodedIn;
	size_t bytesDecodedOut;
	int bits;
	std::string window;

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

	size_t bytesDecodedIn;
	size_t bytesDecodedOut;
	size_t lastAccessBytes;

	std::string decodeHistoryBuff;
	size_t historyBytesToStore;
	size_t maxHistoryBuffSize;

	void Decode();

	//Override streambuf virtual methods
	std::streamsize xsgetn (char* s, std::streamsize n);
	int uflow();
	std::streamsize showmanyc();

public:
	DecodeGzip(std::streambuf &inStream, 
		std::streamsize readBuffSize = 1024*128, std::streamsize decodeBuffSize = 1024*128,
				int windowBits = MAX_WBITS+DEC_MAGIC_NUM_FOR_GZIP);
	virtual ~DecodeGzip();

	bool buildIndex;
	std::vector<class DecodeGzipPoint> *indexOut;
	size_t spanBetweenAccess;
};

///One shot decoding of small files
void DecodeGzipQuick(std::streambuf &fb, std::streambuf &out);
void DecodeGzipQuick(std::string &data, std::string &out);
void DecodeGzipQuick(std::string &data, std::streambuf &outBuff);
void DecodeGzipQuick(std::streambuf &inBuff, std::string &out);
void DecodeGzipQuickFromFilename(const std::string &fina, std::string &out);

#endif //_DECODE_GZIP_H

