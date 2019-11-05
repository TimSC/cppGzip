# cppGzip
Read and write gzip and deflate from C++ (in a convenient way). It depends on zlib. http://www.zlib.net/ Multiple files within the gzip is not supported. To make use of this code, simply include the appropriate source files in your project.

The GZip encoder and decoder are derived from std::streambuf, and can take inputs of arbitrary size. See TestGzip.cpp and TestDeflate.cpp for example usage.

Seeking within a gzip is supported by first calling CreateDecodeGzipIndex. This index can be used to create a DecodeGzipFastSeek object. See TestGzipSeek.cpp for example usage.

Also contains a modified libtar that allows in memory encoding and decoding. SeekableTarRead in tar.h supports random access in a tar.

TestTarGz shows how to open a seekable gzipped tar, using both DecodeGzipFastSeek and SeekableTarRead.

