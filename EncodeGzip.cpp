
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

EncodeGzip::EncodeGzip(std::streambuf &inStream, std::streamsize readBuffSize, std::streamsize decodeBuffSize) : 
	inStream(inStream), decodeDone(false),
	readBuffSize(readBuffSize), decodeBuffSize(decodeBuffSize)
{
	this->readBuff = new char[readBuffSize];
	this->decodeBuff = new char[decodeBuffSize];

	decodeBuffCursor = NULL;
	streamsize len = inStream.sgetn(this->readBuff, readBuffSize);

	d_stream.zalloc = (alloc_func)NULL;
	d_stream.zfree = (free_func)NULL;
	d_stream.opaque = (voidpf)NULL;
	d_stream.next_in  = (Bytef*)this->readBuff;
	d_stream.avail_in = (uInt)len;
	d_stream.next_out = (Bytef*)this->decodeBuff;
	d_stream.avail_out = (uInt)decodeBuffSize;

	//cout << "read " << d_stream.avail_in << endl;
	int err = inflateInit2(&d_stream, 16+MAX_WBITS);
	if(err != Z_OK)
		throw runtime_error(ConcatStr2("inflateInit2 failed: ", zError(err)));
	decodeBuffCursor = decodeBuff;
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
			err = inflate(&d_stream, Z_NO_FLUSH);

			if (err != Z_STREAM_END)
			{
				if(err != Z_OK)
					throw runtime_error(ConcatStr2("inflate failed: ", zError(err)));

				decodeBuffCursor = decodeBuff;
				return;
			}
		}
	}

	//Finish and clean up
	err = inflate(&d_stream, Z_FINISH);
	if(err != Z_OK && err != Z_STREAM_END)
		throw runtime_error(ConcatStr2("inflate failed: ", zError(err)));

	err = inflateEnd(&d_stream);
	if(err != Z_OK)
		throw runtime_error(ConcatStr2("inflateEnd failed: ", zError(err)));
	
	decodeDone = true;
	decodeBuffCursor = decodeBuff;
	return;
}

EncodeGzip::~EncodeGzip()
{
	delete [] this->readBuff;
	delete [] this->decodeBuff;
}

streamsize EncodeGzip::xsgetn (char* s, streamsize n)
{	
	int err = Z_OK;
	char *outputBuffCursor = s;
	streamsize outputTotal = 0;

	while(outputTotal < n && showmanyc() > 0)
	{
		if(!decodeDone && d_stream.avail_out == (uInt)decodeBuffSize)
		{
			//Output buffer is empty, so do more encoding
			Encode();
		}

		streamsize bytesInDecodeBuff = (char *)d_stream.next_out - decodeBuffCursor;
		if(bytesInDecodeBuff > 0)
		{
			//Copy data from decode buffer to output
			streamsize bytesToCopy = n - outputTotal;
			if (bytesToCopy > bytesInDecodeBuff)
				bytesToCopy = bytesInDecodeBuff;
			memcpy(outputBuffCursor, decodeBuffCursor, bytesToCopy);
			outputBuffCursor += bytesToCopy;
			decodeBuffCursor += bytesToCopy;
			outputTotal += bytesToCopy;
		}

		//Check if the decode buffer has been completely copied to output
		bytesInDecodeBuff = (char *)d_stream.next_out - decodeBuffCursor;
		if(bytesInDecodeBuff == 0)
		{
			//Mark buffer as empty
			d_stream.next_out = (Bytef*)this->decodeBuff;
			d_stream.avail_out = (uInt)decodeBuffSize;
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
	streamsize bytesInDecodeBuff = (char *)d_stream.next_out - decodeBuffCursor;
	if(bytesInDecodeBuff > 0)
		return 1;
	if(d_stream.avail_in > 0)
		return 1;
	return inStream.in_avail() > 1;
}


