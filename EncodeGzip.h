#ifndef _ENCODE_GZIP_H
#define _ENCODE_GZIP_H

#include <zlib.h>
#include <streambuf>
#define ENC_MAGIC_NUM_FOR_GZIP 16

class EncodeGzip : public std::streambuf
{
protected:
	char *readBuff;
	char *encodeBuff;
	std::streambuf &inStream;
	z_stream d_stream;
	bool encodeDone;
	char *encodeBuffCursor;
	std::streamsize readBuffSize, encodeBuffSize;

	void Encode();

	//Override streambuf virtual methods
	std::streamsize xsgetn (char* s, std::streamsize n);
	int uflow();
	std::streamsize showmanyc();

public:
	EncodeGzip(std::streambuf &inStream,
		int compressionLevel = Z_DEFAULT_COMPRESSION, 
		std::streamsize readBuffSize = 1024*128, std::streamsize encodeBuffSize = 1024*128,
		int windowBits = MAX_WBITS+ENC_MAGIC_NUM_FOR_GZIP);
	virtual ~EncodeGzip();
};

#endif //_ENCODE_GZIP_H

