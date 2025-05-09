#include <fstream>
#include <iostream>
#include <streambuf>
#include <vector>
#include <cassert>
#include <zstd.h>

class ZstdOStreamBuf : public std::streambuf {
private:
  ZSTD_CCtx* const cctx = ZSTD_createCCtx();
  std::vector<char> outbuf = std::vector<char>(ZSTD_CStreamOutSize());
  ZSTD_outBuffer output = {outbuf.data(), outbuf.size(), 0};
  std::ostream &sink;

protected:
  int overflow(int c = EOF) override {
    if (c != EOF) {
      char ch = c;
      if (xsputn(&ch, 1) != 1)
        return EOF;
    }
    return 0;
  }

  std::streamsize xsputn(const char *s, std::streamsize n) override {
    ZSTD_inBuffer input = { s, static_cast<size_t>(n), 0 };

    while (input.pos < input.size) {
      output.pos = 0;
      size_t ret = ZSTD_compressStream(cctx, &output, &input);
      if (ZSTD_isError(ret)) return -1;
      sink.write(outbuf.data(), output.pos);
    }

    return n;
  }

  int sync_internal(bool is_close) {
    size_t ret;
    do {
      output.pos = 0;
      ret = is_close
        ? ZSTD_endStream(cctx, &output)
        : ZSTD_flushStream(cctx, &output);
      if (ZSTD_isError(ret)) return -1;
      sink.write(outbuf.data(), output.pos);
    } while (ret != 0);

    sink.flush();
    return 0;
  }

  int sync() override {
    return sync_internal(false);
  }

public:
  ZstdOStreamBuf(std::ostream &os, int compressionLevel = 15) : sink(os) {
    size_t ret = ZSTD_initCStream(cctx, compressionLevel);
    assert(!ZSTD_isError(ret));
    ret = ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);
    assert(!ZSTD_isError(ret));
  }

  int close() {
    return sync_internal(true);
  }
  
  ~ZstdOStreamBuf() {
    close();
    ZSTD_freeCStream(cctx);
  }
};

class ZstdOStream : public std::ostream {
private:
  std::ofstream underlying_ofstream;
  ZstdOStreamBuf buf;
public:
  ZstdOStream(std::ostream &os, int compressionLevel = 15) : std::ostream(&buf), buf(os, compressionLevel) { }
  ZstdOStream(const std::string &filename, int compressionLevel = 15) : std::ostream(&buf), buf(underlying_ofstream, compressionLevel) {
    underlying_ofstream.open(filename);
  }
  int close() { return buf.close(); }
};

class ZstdIStreamBuf : public std::streambuf {
private:
  ZSTD_DCtx* const dctx = ZSTD_createDCtx();
  std::vector<char> inbuf = std::vector<char>(ZSTD_DStreamInSize());
  ZSTD_inBuffer input = {inbuf.data(), 0, 0};
  std::vector<char> outbuf = std::vector<char>(ZSTD_DStreamOutSize());
  ZSTD_outBuffer output = {outbuf.data(), outbuf.size(), 0};
  std::istream &source;
  size_t lastRet = 0;
  bool isEmpty = false;

protected:
  int underflow() override {
    if (gptr() < egptr()) {
      return (unsigned char) *gptr();
    }

    /* Given a valid frame, zstd won't consume the last byte of the frame
     * until it has flushed all of the decompressed data of the frame.
     * Therefore, instead of checking if the return code is 0, we can
     * decompress just check if input.pos < input.size.
     */
    if (input.pos >= input.size) {
      if (isEmpty)
        return EOF;
      source.read(inbuf.data(), inbuf.size());
      isEmpty = source.gcount() == 0;
      if (isEmpty) {
        if (lastRet != 0) {
          /* The last return value from ZSTD_decompressStream did not end on a
           * frame, but we reached the end of the file! We assume this is an
           * error, and the input was truncated.
           */
          std::cerr << "EOF before end of stream: " << lastRet << std::endl;
          assert(false);
        }
        return EOF;
      }
      input.size = source.gcount();
      input.pos = 0;
    }

    output.pos = 0;
    /* The return code is zero if the frame is complete, but there may
     * be multiple frames concatenated together. Zstd will automatically
     * reset the context when a frame is complete. Still, calling
     * ZSTD_DCtx_reset() can be useful to reset the context to a clean
     * state, for instance if the last decompression call returned an
     * error.
     */
    lastRet = ZSTD_decompressStream(dctx, &output , &input);
    assert(!ZSTD_isError(lastRet)); // ZSTD_getErrorName(ret);
    setg(outbuf.data(), outbuf.data(), outbuf.data() + output.pos);

    return (unsigned char)*gptr();
  }

public:
  ZstdIStreamBuf(std::istream &is) : source(is) {
    assert(dctx != NULL);
  }

  ~ZstdIStreamBuf() {
    ZSTD_freeDCtx(dctx);
  }
};

class ZstdIStream : public std::istream {
private:
  std::istream *underlying_ifstream = nullptr;
  ZstdIStreamBuf buf;
public:
  ZstdIStream(std::istream &is) : std::istream(&buf), buf(is) {}
  ZstdIStream(const std::string &filename) : std::istream(&buf), underlying_ifstream(new std::ifstream(filename)), buf(*underlying_ifstream) { }
  ~ZstdIStream() {
    if (underlying_ifstream) {
      delete underlying_ifstream;
      underlying_ifstream = nullptr;
    }
  }
};

/*

// zstd -d
int main() {
  const size_t buffer_size = 16 * 1024;
  char buffer[buffer_size];
  ZstdIStream zstd_in(std::cin);
  while (zstd_in.read(buffer, buffer_size) || zstd_in.gcount() > 0) {
    std::cout.write(buffer, zstd_in.gcount());
  }
  std::cout.flush();
  return 0;
}

// zstd -c
int main(int argc, char *argv[]) {
  int compressionLevel = argc > 1 ? atoi(argv[1]) : 15;
  const size_t buffer_size = 16 * 1024;
  char buffer[buffer_size];
  ZstdOStream zstd_out(std::cout, compressionLevel);
  while (std::cin.read(buffer, buffer_size) || std::cin.gcount() > 0) {
    zstd_out.write(buffer, std::cin.gcount());
  }
  zstd_out.flush();
  return 0;
}

*/
