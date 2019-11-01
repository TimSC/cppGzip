
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include "EncodeGzip.h"
using namespace std;

std::string ConcatStr2(const char *a, const char *b)
{
	string out(a);
	out.append(b);
	return out;
}

EncodeGzip::EncodeGzip(std::streambuf &outStream, int compressionLevel, std::streamsize encodeBuffSize, int windowBits) : 
	outStream(outStream), encodeBuffSize(encodeBuffSize), 
	firstInputData(true), compressionLevel(compressionLevel), windowBits(windowBits)
{
	if(compressionLevel < Z_DEFAULT_COMPRESSION && compressionLevel > Z_BEST_COMPRESSION)
		throw invalid_argument("Invalid compression level");

	this->encodeBuff = new char[encodeBuffSize];

	d_stream.zalloc = (alloc_func)NULL;
	d_stream.zfree = (free_func)NULL;
	d_stream.opaque = (voidpf)NULL;
	d_stream.next_in  = (Bytef*)NULL;
	d_stream.avail_in = (uInt)0; //Input starts as empty
	d_stream.next_out = (Bytef*)this->encodeBuff;
	d_stream.avail_out = (uInt)encodeBuffSize;

	inputBytesEncoded = 0;
	lastInputBytesFlush = 0;
	flushEveryNumBytes = 1*1024*1024;
}

EncodeGzip::~EncodeGzip()
{
	d_stream.next_in  = (Bytef*)NULL;
	d_stream.avail_in = (uInt)0;

	//Handle case of zero length content
	if(firstInputData)
	{
		int err = deflateInit2(&d_stream, compressionLevel, Z_DEFLATED, windowBits, 8, Z_DEFAULT_STRATEGY);
		if(err != Z_OK)
			throw runtime_error(ConcatStr2("deflateInit failed: ", zError(err)));
		firstInputData = false;
	}

	//Flush
	if(!firstInputData)
	{
		int err = deflate(&d_stream, Z_FINISH);
		while(err == Z_OK)
		{
			CopyDataToOutput();
			err = deflate(&d_stream, Z_FINISH);
		}
		if(err != Z_STREAM_END)
			throw runtime_error(ConcatStr2("deflate failed: ", zError(err)));

		CopyDataToOutput();

		//Clean up
		err = deflateEnd(&d_stream);
		if(err != Z_OK)
			throw runtime_error(ConcatStr2("deflateEnd failed: ", zError(err)));
	}

	delete [] this->encodeBuff;
}

void EncodeGzip::CopyDataToOutput()
{
	//Copy result to output
	streamsize sizeToWrite = encodeBuffSize - d_stream.avail_out;
	streamsize i=0;
	while(i<sizeToWrite)
	{
		streamsize written = outStream.sputn(&this->encodeBuff[i], sizeToWrite-i);
		i += written;
	}
	d_stream.next_out = (Bytef*)this->encodeBuff;
	d_stream.avail_out = (uInt)encodeBuffSize;
}

streamsize EncodeGzip::xsputn (const char* s, streamsize n)
{
	d_stream.next_in  = (Bytef*)s;
	d_stream.avail_in = (uInt)n;

	if(firstInputData)
	{
		int err = deflateInit2(&d_stream, compressionLevel, Z_DEFLATED, windowBits, 8, Z_DEFAULT_STRATEGY);
		if(err != Z_OK)
			throw runtime_error(ConcatStr2("deflateInit failed: ", zError(err)));
		firstInputData = false;
		CopyDataToOutput();
	}

	while (d_stream.avail_in > 0)
	{
		//Decide if we need to flush
		bool needToFlush = (flushEveryNumBytes > 0) && (inputBytesEncoded > lastInputBytesFlush + flushEveryNumBytes);
		int flags = Z_NO_FLUSH;
		if(needToFlush)
			flags = Z_BLOCK;

		int err = deflate(&d_stream, flags);
		if (err != Z_OK && err != Z_STREAM_END)
			throw runtime_error("deflate failed");

		inputBytesEncoded += n;
		if(needToFlush)
			lastInputBytesFlush = inputBytesEncoded;

		CopyDataToOutput();
	}

	return n;	
}

int EncodeGzip::overflow (int c)
{
	return EOF;
}

// **********************************************

void EncodeGzipQuick(std::streambuf &fb, std::streambuf &out)
{
	class EncodeGzip encodeGzip(out);	

	int testBuffSize = 1024*100;
	char buff[testBuffSize];
	ifstream testIn("input.txt", ios::binary);
	unsigned outlen = 0;
	while(fb.in_avail()>0)
	{
		testIn.read(buff, testBuffSize);
		streamsize len = testIn.gcount();

		encodeGzip.sputn(buff, len);
		outlen += len;
	}
}

void EncodeGzipQuick(std::string &inStr, std::string &outStr)
{
	stringbuf inBuff(inStr), outBuff;
	EncodeGzipQuick(inBuff, outBuff);
	outStr = outBuff.str();
}

void EncodeGzipQuick(std::streambuf &inBuff, std::string &outStr)
{
	stringbuf outBuff;
	EncodeGzipQuick(inBuff, outBuff);
	outStr = outBuff.str();
}

void EncodeGzipQuick(std::string &inStr, std::streambuf &outBuff)
{
	stringbuf inBuff(inStr);
	EncodeGzipQuick(inBuff, outBuff);
}

