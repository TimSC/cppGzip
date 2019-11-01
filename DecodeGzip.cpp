
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

DecodeGzipPoint::DecodeGzipPoint()
{
	bytesDecodedIn = 0;
	bytesDecodedOut = 0;
	bits = 0;
}

DecodeGzipPoint::DecodeGzipPoint(const DecodeGzipPoint &obj)
{
	bytesDecodedIn = obj.bytesDecodedIn;
	bytesDecodedOut = obj.bytesDecodedOut;
	bits = obj.bits;
	window = obj.window;
}

DecodeGzipPoint::~DecodeGzipPoint()
{

}

// *******************************************

DecodeGzip::DecodeGzip(std::streambuf &inStream, std::streamsize readBuffSize, std::streamsize decodeBuffSize, int windowBits) : 
	inStream(inStream), decodeDone(false),
	readBuffSize(readBuffSize), decodeBuffSize(decodeBuffSize), windowBits(windowBits)
{
	this->readBuff = new char[readBuffSize];
	this->decodeBuff = new char[decodeBuffSize];

	decodeBuffCursor = nullptr;
	buildIndex = false;
	streamsize len = inStream.sgetn(this->readBuff, readBuffSize);

	d_stream.zalloc = (alloc_func)nullptr;
	d_stream.zfree = (free_func)nullptr;
	d_stream.opaque = (voidpf)nullptr;
	d_stream.next_in  = (Bytef*)this->readBuff;
	d_stream.avail_in = (uInt)len;
	d_stream.next_out = (Bytef*)this->decodeBuff;
	d_stream.avail_out = (uInt)decodeBuffSize;

	int err = inflateInit2(&d_stream, windowBits);
	if(err != Z_OK)
		throw runtime_error(ConcatStr("inflateInit2 failed: ", zError(err)));
	decodeBuffCursor = decodeBuff;

	bytesDecodedIn = 0;
	bytesDecodedOut = 0;
	spanBetweenAccess = 1024*1024;
	lastAccessBytes = 0;
	historyBytesToStore = 32*1024;
	maxHistoryBuffSize = 1024*1024;
	indexOut = nullptr;
}

void DecodeGzip::Decode()
{
	//We always start with the decode buffer empty and return as soon as we have anything in there
	if(d_stream.avail_out != (uInt)decodeBuffSize)
		throw runtime_error("Internal error");
		
	int err = Z_OK;
	bool streamEnded = false;
	while((inStream.in_avail() > 0 || d_stream.avail_in > 0) and !streamEnded)
	{
		if(d_stream.avail_in == 0 && inStream.in_avail() > 0)
		{
			//Read buffer is empty, so read more from file
			streamsize len = inStream.sgetn(this->readBuff, readBuffSize);
			d_stream.next_in  = (Bytef*)this->readBuff;
			d_stream.avail_in = (uInt)len;
		}

		if(d_stream.avail_in > 0)
		{
			int old_avail_in = d_stream.avail_in;
			unsigned char *old_next_in = d_stream.next_in;
	
			//Data is waiting to be decoded
			int flags = Z_NO_FLUSH;
			if(buildIndex)
				flags = Z_BLOCK;

			err = inflate(&d_stream, flags);

			if (buildIndex)
			{
				//Various things need to be tracked in order to provide random access
				size_t bytesInLastCall = (old_avail_in - d_stream.avail_in);
				size_t bytesDecodedLastCall = (decodeBuffSize - d_stream.avail_out);
				bytesDecodedIn += bytesInLastCall;
				bytesDecodedOut += bytesDecodedLastCall;

				decodeHistoryBuff.append(decodeBuff, bytesDecodedLastCall);
				if(decodeHistoryBuff.size() > maxHistoryBuffSize)
					decodeHistoryBuff = decodeHistoryBuff.substr(decodeHistoryBuff.length()-historyBytesToStore);
			}

			if (err != Z_STREAM_END)
			{
				if(err != Z_OK)
					throw runtime_error(ConcatStr("inflate failed: ", zError(err)));

				if (buildIndex)
				{
					//Check if we need to remember this (for later random access)
				    if ((d_stream.data_type & 128) && !(d_stream.data_type & 64) && (bytesDecodedOut > spanBetweenAccess+lastAccessBytes))
					{
						int bits = d_stream.data_type & 7;
						decodeHistoryBuff = decodeHistoryBuff.substr(decodeHistoryBuff.length()-historyBytesToStore);

						class DecodeGzipPoint pt;
						pt.bytesDecodedIn = bytesDecodedIn;
						pt.bytesDecodedOut = bytesDecodedOut;
						pt.bits = bits;
						pt.window = decodeHistoryBuff;
	
						if(indexOut)
							indexOut->push_back(pt);

						lastAccessBytes = bytesDecodedOut;
					}
				}

				decodeBuffCursor = decodeBuff;
				//Wait for this data to be read by output before doing any more inflating
				return;
			}
			else
			{
				streamEnded = true;
				d_stream.avail_in = 0;
			}
		}
	}

	//Finish and clean up
	err = inflate(&d_stream, Z_FINISH);
	if(err != Z_OK && err != Z_STREAM_END)
		throw runtime_error(ConcatStr("inflate failed: ", zError(err)));
	
	decodeDone = true;
	decodeBuffCursor = decodeBuff;
	return;
}

DecodeGzip::~DecodeGzip()
{
	int err = inflateEnd(&d_stream);
	if(err != Z_OK)
		throw runtime_error(ConcatStr("inflateEnd failed: ", zError(err)));

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
	return inStream.in_avail() > 0;
}

streampos DecodeGzip::seekpos (streampos sp, ios_base::openmode which = ios_base::in | ios_base::out)
{
	//Reset to beginning (naive seeking approach)
	buildIndex = 0;
	bytesDecodedIn = 0;
	bytesDecodedOut = 0;
	lastAccessBytes = 0;
	decodeDone = false;
	decodeHistoryBuff.clear();
	streampos inPos = inStream.pubseekpos(0);
	if(inPos != 0)
		throw runtime_error("Could not seek source");

	d_stream.next_in  = (Bytef*)this->readBuff;
	d_stream.avail_in = (uInt)0;
	d_stream.next_out = (Bytef*)this->decodeBuff;
	d_stream.avail_out = (uInt)decodeBuffSize;

	inflateReset2(&d_stream, windowBits);
	
	return SkimToStreamPos(0, sp);
}

streampos DecodeGzip::SkimToStreamPos(std::streampos start, std::streampos sp)
{
	//Start decoding until where we want
	char tmpbuff[decodeBuffSize];
	size_t totalRead = start;
	while(this->in_avail()>0 && totalRead < sp)
	{
		size_t maxRead = decodeBuffSize;
		size_t bytesLeftToSeek = sp - totalRead;
		if(bytesLeftToSeek < maxRead)
			maxRead = bytesLeftToSeek;

		totalRead += this->sgetn(tmpbuff, maxRead);
	}
	return totalRead;
}

// ************************************************

DecodeGzipFastSeek::DecodeGzipFastSeek(std::streambuf &inStream, 
	const DecodeGzipIndex &index,
	std::streamsize readBuffSize, std::streamsize decodeBuffSize,
	int windowBits):

	DecodeGzip(inStream, readBuffSize, decodeBuffSize, windowBits),
	index(index)
{

}

DecodeGzipFastSeek::~DecodeGzipFastSeek()
{

}

std::streampos DecodeGzipFastSeek::seekpos (std::streampos sp, std::ios_base::openmode which)
{	
	bool found = false;
	size_t bestIndex = 0;
	for(size_t i=0; i < index.size(); i++)
	{
		if(index[i].bytesDecodedOut <= sp)
		{
			bestIndex = i;
			found = true;
		}
		else break;
	}

	//If no index point found, fall back to naive seek	
	if(!found || sp == 0)
		return DecodeGzip::seekpos(sp);

	const class DecodeGzipPoint &pt = index[bestIndex];

	streampos actualSeekTarget = pt.bytesDecodedIn - (pt.bits ? 1 : 0);
	streampos isp = inStream.pubseekpos(actualSeekTarget);
	if(isp != actualSeekTarget)
		throw runtime_error("Could not seek source");

	decodeDone = false;
	d_stream.next_in  = (Bytef*)this->readBuff;
	d_stream.avail_in = (uInt)0;
	d_stream.next_out = (Bytef*)this->decodeBuff;
	d_stream.avail_out = (uInt)decodeBuffSize;

	//Prepare to resume inflate
	int ret = inflateReset2(&d_stream, -15);
	if(ret != Z_OK)	
		throw runtime_error("Could not inflateReset2");
	if(pt.bits)
	{
		unsigned char val[1];
		ret = inStream.sgetn((char*)val, 1);
		if(ret != 1)
			throw runtime_error("Could not read source");
		ret = inflatePrime(&d_stream, pt.bits, val[0] >> (8 - pt.bits));
		if(ret != Z_OK)	
			throw runtime_error("Could not inflatePrime");
	}
    ret = inflateSetDictionary(&d_stream, (const Bytef*)pt.window.c_str(), pt.window.length());
		if(ret != Z_OK)	
			throw runtime_error("Could not inflateSetDictionary");
	
	streampos out = SkimToStreamPos(pt.bytesDecodedOut, sp);
	return out;
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

void CreateDecodeGzipIndex(std::streambuf &inStream, 
	DecodeGzipIndex &out,
	std::streamsize readBuffSize, std::streamsize decodeBuffSize,
	int windowBits)
{
	class DecodeGzip dec(inStream, readBuffSize, decodeBuffSize, windowBits);
	dec.buildIndex = true;
	dec.indexOut = &out;
	
	char tmpbuff[decodeBuffSize];
	while(dec.in_avail()>0)
	{
		dec.sgetn(tmpbuff, decodeBuffSize);
	}
}

