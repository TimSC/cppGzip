# cppGzip
Read and write gzip and deflate from C++ (in a convenient way). It depends on zlib. http://www.zlib.net/ Multiple files within the gzip is not supported.

The encoder and decoder are derived from std::streambuf, and can take inputs of arbitrary size. See TestGzip.cpp and TestDeflate.cpp for example usage.


