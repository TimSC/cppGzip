#ifndef _ENCODE_GZIP_H
#define _ENCODE_GZIP_H

#include <zlib.h>
#include <streambuf>

class EncodeGzip : public std::streambuf
{
protected:
	char *readBuff;
	char *decodeBuff;
	std::streambuf &inStream;
	z_stream d_stream;
	bool decodeDone;
	char *decodeBuffCursor;
	std::streamsize readBuffSize, decodeBuffSize;

	void Encode();

	//Override streambuf virtual methods
	std::streamsize xsgetn (char* s, std::streamsize n);
	int uflow();
	std::streamsize showmanyc();

public:
	EncodeGzip(std::streambuf &inStream, std::streamsize readBuffSize = 1024*128, std::streamsize decodeBuffSize = 1024*128);
	virtual ~EncodeGzip();
};

#endif //_ENCODE_GZIP_H

