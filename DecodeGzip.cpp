
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include "DecodeGzip.h"
using namespace std;

std::string ConcatStr(const char *a, const char *b)
{
	string out(a);
	out.append(b);
	return out;
}

DecodeGzip::DecodeGzip(std::streambuf &inStream, std::streamsize readBuffSize, std::streamsize decodeBuffSize, int windowBits) : 
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
	int err = inflateInit2(&d_stream, windowBits);
	if(err != Z_OK)
		throw runtime_error(ConcatStr("inflateInit2 failed: ", zError(err)));
	decodeBuffCursor = decodeBuff;
}

void DecodeGzip::Decode()
{
	int err = Z_OK;
	while(inStream.in_avail() > 1 || d_stream.avail_in > 0)
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
					throw runtime_error(ConcatStr("inflate failed: ", zError(err)));

				decodeBuffCursor = decodeBuff;
				return;
			}
		}
	}

	//Finish and clean up
	err = inflate(&d_stream, Z_FINISH);
	if(err != Z_OK && err != Z_STREAM_END)
		throw runtime_error(ConcatStr("inflate failed: ", zError(err)));

	err = inflateEnd(&d_stream);
	if(err != Z_OK)
		throw runtime_error(ConcatStr("inflateEnd failed: ", zError(err)));
	
	decodeDone = true;
	decodeBuffCursor = decodeBuff;
	return;
}

DecodeGzip::~DecodeGzip()
{
	delete [] this->readBuff;
	delete [] this->decodeBuff;
}

streamsize DecodeGzip::xsgetn (char* s, streamsize n)
{	
	char *outputBuffCursor = s;
	streamsize outputTotal = 0;

	while(outputTotal < n && showmanyc() > 0)
	{
		if(!decodeDone && d_stream.avail_out == (uInt)decodeBuffSize)
		{
			//Decode buffer is empty, so do more decoding
			Decode();
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

int DecodeGzip::uflow()
{
	streamsize inputReady = showmanyc();
	if(inputReady==0) return EOF;
	char buff[1];
	xsgetn(buff, 1);
	return *(unsigned char *)&(buff[0]);
}

streamsize DecodeGzip::showmanyc()
{
	streamsize bytesInDecodeBuff = (char *)d_stream.next_out - decodeBuffCursor;
	if(bytesInDecodeBuff > 0)
		return 1;
	if(d_stream.avail_in > 0)
		return 1;
	return inStream.in_avail() > 1;
}

// **************************************

void DecodeGzipQuick(std::streambuf &fb, std::streambuf &out)
{
	class DecodeGzip decodeGzip(fb);

	int buffSize = 1024*100;
	char buff[buffSize];
	while(decodeGzip.in_avail()>0)
	{
		int len = decodeGzip.sgetn(buff, buffSize-1);
		buff[len] = '\0';
		
		out.sputn(buff, len);
	}
}

void DecodeGzipQuick(std::string &data, std::string &out)
{
	std::stringbuf sb(data), outBuff;
	DecodeGzipQuick(sb, outBuff);
	out = outBuff.str();
}

void DecodeGzipQuick(std::string &data, std::streambuf &outBuff)
{
	std::stringbuf sb(data);
	DecodeGzipQuick(sb, outBuff);
}

void DecodeGzipQuick(std::streambuf &inBuff, std::string &out)
{
	std::stringbuf outBuff;
	DecodeGzipQuick(inBuff, outBuff);
	out = outBuff.str();
}

void DecodeGzipQuickFromFilename(const std::string &fina, std::string &out)
{
	std::filebuf infi;
	infi.open(fina.c_str(), std::ios::in | std::ios::binary);
	DecodeGzipQuick(infi, out);
}

