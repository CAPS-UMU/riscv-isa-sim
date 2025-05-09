#include <fstream>
#include <iostream>
#include <streambuf>
#include <lzma.h>
#include <cassert>

class LzmaOStreamBuf : public std::streambuf {
private:
  lzma_stream strm = LZMA_STREAM_INIT;
  char outbuf[16 * 1024];
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
    strm.next_in = (const uint8_t *) s;
    strm.avail_in = n;

    while (strm.avail_in > 0) {
      strm.next_out = (uint8_t *) outbuf;
      strm.avail_out = sizeof(outbuf);

      lzma_ret ret = lzma_code(&strm, LZMA_RUN);
      if (ret != LZMA_OK) return -1;

      sink.write(outbuf, sizeof(outbuf) - strm.avail_out);
    }
    return n;
  }

  int sync_internal(bool is_close) {
    assert (strm.avail_in == 0);
    lzma_ret ret;
    do {
      strm.next_out = (uint8_t *)outbuf;
      strm.avail_out = sizeof(outbuf);
      ret = lzma_code(&strm, is_close ? LZMA_FINISH : LZMA_SYNC_FLUSH);
      if (ret != LZMA_OK && ret != LZMA_STREAM_END) return -1;
      sink.write(outbuf, sizeof(outbuf) - strm.avail_out);
    } while (ret == LZMA_OK);
    assert(ret == LZMA_STREAM_END);
    sink.flush();
    return 0;
  }

  int sync() override {
    return sync_internal(false);
  }

public:
  LzmaOStreamBuf(std::ostream &os, int preset = 6) : sink(os) {
    auto ret = lzma_easy_encoder(&strm, preset, LZMA_CHECK_CRC64);
    assert(ret == LZMA_OK);
  }
  
  int close() {
    return sync_internal(true);
  }

  ~LzmaOStreamBuf() {
    close();
    lzma_end(&strm);
  }
};

class LzmaOStream : public std::ostream {
private:
  std::ostream *underlying_ofstream = nullptr;
  LzmaOStreamBuf buf;
public:
  LzmaOStream(std::ostream &os, int preset = 6) : std::ostream(&buf), buf(os, preset) {}
  LzmaOStream(const std::string &filename, int preset = 6) : std::ostream(&buf), underlying_ofstream(new std::ofstream(filename)), buf(*underlying_ofstream, preset) { }
  ~LzmaOStream() {
    if (underlying_ofstream) {
      delete underlying_ofstream;
    }
  }
  int close() { return buf.close(); }
};


class LzmaIStreamBuf : public std::streambuf {
private:
  lzma_stream strm = LZMA_STREAM_INIT;
  char inbuf[4096];
  char outbuf[4096];
  std::istream &source;
  bool end_of_stream = false;

protected:
  int underflow() override {
    if (gptr() < egptr()) {
      return (unsigned char) *gptr();
    }

    if (end_of_stream)
      return EOF;

    if (strm.avail_in <= 0) {
      source.read(inbuf, sizeof(inbuf));
      if (source.eof() && source.gcount() == 0) {
        return EOF;
      }
      
      strm.next_in = (const uint8_t *) inbuf;
      strm.avail_in = source.gcount();
    }
    
    lzma_action action = source.eof() ? LZMA_FINISH : LZMA_RUN;

    strm.next_out = (uint8_t *) outbuf;
    strm.avail_out = sizeof(outbuf);
    
    lzma_ret ret = lzma_code(&strm, action);
    
    if (ret == LZMA_STREAM_END) {
      end_of_stream = true;
    } else {
      assert(ret == LZMA_OK);
    }
    
    auto out_size = sizeof(outbuf) - strm.avail_out;
    assert(out_size > 0);
    setg(outbuf, outbuf, outbuf + out_size);
    
    return (unsigned char) *gptr();
}

public:
  LzmaIStreamBuf(std::istream &is) : source(is) {
    auto ret = lzma_stream_decoder(&strm, UINT64_MAX, LZMA_CONCATENATED);
    assert(ret == LZMA_OK);
  }

  ~LzmaIStreamBuf() {
    lzma_end(&strm);
  }
};

class LzmaIStream : public std::istream {
private:
  std::istream *underlying_ifstream = nullptr;
  LzmaIStreamBuf buf;
public:
  LzmaIStream(std::istream &is) : std::istream(&buf), buf(is) {}
  LzmaIStream(const std::string &filename) : std::istream(&buf), underlying_ifstream(new std::ifstream(filename)), buf(*underlying_ifstream) { }
  ~LzmaIStream() {
    if (underlying_ifstream) {
      delete underlying_ifstream;
    }
  }
};


/*
int main() {
  const size_t buffer_size = 16 * 1024;
  char buffer[buffer_size];
  LzmaOStream lzma_out(std::cout);
  
  while (std::cin.read(buffer, buffer_size) || std::cin.gcount() > 0) {
    lzma_out.write(buffer, std::cin.gcount());
  }
  return 0;
}

*/
