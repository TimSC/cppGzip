
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include "EncodeGzip.h"
using namespace std;
#define MAGIC_NUM_FOR_GZIP 16

std::string ConcatStr2(const char *a, const char *b)
{
	string out(a);
	out.append(b);
	return out;
}

EncodeGzip::EncodeGzip(std::streambuf &inStream, int compressionLevel, std::streamsize readBuffSize, std::streamsize encodeBuffSize) : 
	inStream(inStream), encodeDone(false),
	readBuffSize(readBuffSize), encodeBuffSize(encodeBuffSize)
{
	if(compressionLevel < Z_DEFAULT_COMPRESSION && compressionLevel > Z_BEST_COMPRESSION)
		throw invalid_argument("Invalid compression level");

	this->readBuff = new char[readBuffSize];
	this->encodeBuff = new char[encodeBuffSize];

	encodeBuffCursor = NULL;
	streamsize len = inStream.sgetn(this->readBuff, readBuffSize);

	d_stream.zalloc = (alloc_func)NULL;
	d_stream.zfree = (free_func)NULL;
	d_stream.opaque = (voidpf)NULL;
	d_stream.next_in  = (Bytef*)this->readBuff;
	d_stream.avail_in = (uInt)len;
	d_stream.next_out = (Bytef*)this->encodeBuff;
	d_stream.avail_out = (uInt)encodeBuffSize;

	//cout << "read " << d_stream.avail_in << endl;
	int err = deflateInit2(&d_stream, compressionLevel, Z_DEFLATED, MAGIC_NUM_FOR_GZIP+MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
	if(err != Z_OK)
		throw runtime_error(ConcatStr2("deflateInit failed: ", zError(err)));
	encodeBuffCursor = encodeBuff;
}

void EncodeGzip::Encode()
{
	int err = Z_OK;
	while(inStream.in_avail() > 1)
	{
		if(d_stream.avail_in == 0 && inStream.in_avail() > 1)
		{
			//Read buffer is empty, so read more from file
			streamsize len = inStream.sgetn(this->readBuff, readBuffSize);
			d_stream.next_in  = (Bytef*)this->readBuff;
			d_stream.avail_in = (uInt)len;
			//cout << "read " << d_stream.avail_in << endl;
		}

		if(d_stream.avail_in > 0)
		{
			//Data is waiting to be decoded
			err = deflate(&d_stream, Z_NO_FLUSH);

			if (err != Z_STREAM_END)
			{
				if(err != Z_OK)
					throw runtime_error(ConcatStr2("deflate failed: ", zError(err)));

				encodeBuffCursor = encodeBuff;
				return;
			}
		}
	}

	//Finish and clean up
	err = deflate(&d_stream, Z_FINISH);
	if(err != Z_OK && err != Z_STREAM_END)
		throw runtime_error(ConcatStr2("deflate failed: ", zError(err)));

	err = deflateEnd(&d_stream);
	if(err != Z_OK)
		throw runtime_error(ConcatStr2("deflateEnd failed: ", zError(err)));
	
	encodeDone = true;
	encodeBuffCursor = encodeBuff;
	return;
}

EncodeGzip::~EncodeGzip()
{
	delete [] this->readBuff;
	delete [] this->encodeBuff;
}

streamsize EncodeGzip::xsgetn (char* s, streamsize n)
{	
	int err = Z_OK;
	char *outputBuffCursor = s;
	streamsize outputTotal = 0;

	while(outputTotal < n && showmanyc() > 0)
	{
		if(!encodeDone && d_stream.avail_out == (uInt)encodeBuffSize)
		{
			//Output buffer is empty, so do more encoding
			Encode();
		}

		streamsize bytesInDecodeBuff = (char *)d_stream.next_out - encodeBuffCursor;
		if(bytesInDecodeBuff > 0)
		{
			//Copy data from decode buffer to output
			streamsize bytesToCopy = n - outputTotal;
			if (bytesToCopy > bytesInDecodeBuff)
				bytesToCopy = bytesInDecodeBuff;
			memcpy(outputBuffCursor, encodeBuffCursor, bytesToCopy);
			outputBuffCursor += bytesToCopy;
			encodeBuffCursor += bytesToCopy;
			outputTotal += bytesToCopy;
		}

		//Check if the decode buffer has been completely copied to output
		bytesInDecodeBuff = (char *)d_stream.next_out - encodeBuffCursor;
		if(bytesInDecodeBuff == 0)
		{
			//Mark buffer as empty
			d_stream.next_out = (Bytef*)this->encodeBuff;
			d_stream.avail_out = (uInt)encodeBuffSize;
		}

	}

	return outputTotal;
}

int EncodeGzip::uflow()
{
	streamsize inputReady = showmanyc();
	if(inputReady==0) return EOF;
	char buff[1];
	xsgetn(buff, 1);
	return *(unsigned char *)&(buff[0]);
}

streamsize EncodeGzip::showmanyc()
{
	streamsize bytesInDecodeBuff = (char *)d_stream.next_out - encodeBuffCursor;
	if(bytesInDecodeBuff > 0)
		return 1;
	if(d_stream.avail_in > 0)
		return 1;
	return inStream.in_avail() > 1;
}


