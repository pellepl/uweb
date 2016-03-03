/*
The MIT License (MIT)

Copyright (c) 2016 Peter Andersson (pelleplutt1976<at>gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "../uweb.h"
#include "testrunner.h"

static UW_STREAM _response_stream = 0;
static uint32_t _response_chunk_bytes = 0;
static uint32_t _response_buffer_ix = 0;
static uint8_t _response_buffer[65536];
static uint32_t _data_buffer_ix = 0;
static uint8_t _data_buffer[65536];

static int32_t chstr_read(UW_STREAM str, uint8_t *dst, uint32_t len) {
  if (str->avail_sz > str->total_sz)
    str->avail_sz = str->total_sz;
  if (len > str->avail_sz)
    len = str->avail_sz;
  if (len) {
    memcpy(dst, str->user, len);
    str->user += len;
    str->avail_sz -= len;
    str->total_sz -= len;
  }
  return len;
}

static int32_t chstr_write(UW_STREAM str, uint8_t *src, uint32_t len) {
  return -1;
}

UW_STREAM make_char_stream(UW_STREAM str, const char *data)
{
  str->total_sz = strlen(data);
  str->capacity_sz = 0;
  str->avail_sz = str->total_sz;
  str->user = (void *) data;
  str->read = chstr_read;
  str->write = chstr_write;
  return str;
}

static int32_t prstr_read(UW_STREAM str, uint8_t *dst, uint32_t len) {
  return 0;
}

static int32_t prstr_write(UW_STREAM str, uint8_t *src, uint32_t len) {
  while (len--) {
    _response_buffer[_response_buffer_ix++] = *src;
    printf("%c", *src++);
  }
  return -1;
}

UW_STREAM make_printf_stream(UW_STREAM str)
{
  _response_buffer_ix = 0;
  str->total_sz = -1;
  str->capacity_sz = 256;
  str->avail_sz = str->total_sz;
  str->user = 0;
  str->read = prstr_read;
  str->write = prstr_write;
  return str;
}

static uweb_response uweb_response_fn(uweb_request_header *req, UW_STREAM *res, uweb_http_status *http_status, char *content_type, char **extra_headers) {
  *res = _response_stream;
  if (_response_chunk_bytes == 0) {
    return UWEB_OK;
  } else {
    if (_response_stream->total_sz) {
      _response_stream->avail_sz = _response_chunk_bytes < _response_stream->total_sz ? _response_chunk_bytes : _response_stream->total_sz;
    }
    return UWEB_CHUNKED;
  }
}

static void uweb_data_fn(uweb_request_header *req, uweb_data_type type, uint32_t offset, uint8_t *data, uint32_t length) {
#if 0
  printf("#####      DATA TYPE:%i OFFS:%i LENGTH:%i\n"
         "#####           CHUNK:%i CONN:%s CONTENT_TYPE:%s\n"
         "#####           MLTPART #:%i DISP:%s TYPE:%s\n",
         type, offset, length,
         req->chunk_nbr, req->connection, req->content_type,
         req->cur_multipart.multipart_nbr, req->cur_multipart.content_disp, req->cur_multipart.content_type);
#endif
  if (strstr(req->content_type, "multipart/form-data") && offset == 0) {
    _data_buffer_ix += sprintf(&_data_buffer[_data_buffer_ix], "[%s]", req->cur_multipart.content_disp);
  }
  while (length--) {
    _data_buffer[_data_buffer_ix++] = *data++;
  }
}

static const char *REQ_TXT =
    "GET / HTTP/1.1\r\n"
    "Host: www.pelleplutt.com\r\n"
    "User-Agent: Mozilla/4.0\r\n"
    "\r\n";

static uweb_data_stream stream[8];


SUITE(uweb_tests)

  void setup()
  {
    _response_chunk_bytes = 0;
    _response_buffer_ix = 0;
    memset(_response_buffer, 0, sizeof(_response_buffer));
    _data_buffer_ix = 0;
    memset(_data_buffer, 0, sizeof(_data_buffer));
  }

  void teardown()
  {
  }


  TEST(simple_request)
  {
    UW_STREAM req_str = make_char_stream(&stream[0], REQ_TXT);
    UW_STREAM pri_str = make_printf_stream(&stream[1]);
    UW_STREAM res_str = make_char_stream(&stream[2], "Hello world!");
    _response_stream = res_str;
    UWEB_init(uweb_response_fn, uweb_data_fn);
    UWEB_parse(req_str, pri_str);
    TEST_CHECK_EQ(strcmp(_response_buffer,
     "HTTP/1.1 200 OK\n"
     "Server: uWeb\n"
     "Content-Type: text/html; charset=utf-8\n"
     "Content-Length: 12\n"
     "Connection: close\n"
     "\n"
     "Hello world!"), 0);
    return TEST_RES_OK;
  } TEST_END(simple_request)

  TEST(simple_chunk_request)
  {
    UW_STREAM req_str = make_char_stream(&stream[0], REQ_TXT);
    UW_STREAM pri_str = make_printf_stream(&stream[1]);
    UW_STREAM res_str = make_char_stream(&stream[2], "Hello world!");
    _response_stream = res_str;
    _response_chunk_bytes = 5;
    UWEB_init(uweb_response_fn, uweb_data_fn);
    UWEB_parse(req_str, pri_str);
    TEST_CHECK_EQ(strcmp(_response_buffer,
     "HTTP/1.1 200 OK\n"
     "Server: uWeb\n"
     "Content-Type: text/html; charset=utf-8\n"
     "Transfer-Encoding: chunked\n"
     "\n"
     "5; chunk 0\r\n"
     "Hello\r\n"
     "5; chunk 1\r\n"
     " worl\r\n"
     "2; chunk 2\r\n"
     "d!\r\n"
     "0\r\n\r\n"), 0);
    return TEST_RES_OK;
  } TEST_END(simple_chunk_request)


  TEST(simple_request_bad)
  {
    UW_STREAM req_str = make_char_stream(&stream[0],
     "BAD / HTTP/1.1\r\n"
     "Host: www.pelleplutt.com\r\n"
     "User-Agent: Mozilla/4.0\r\n"
     "\r\n");
    UW_STREAM pri_str = make_printf_stream(&stream[1]);
    UW_STREAM res_str = make_char_stream(&stream[2], "Hello world!");
    _response_stream = res_str;
    UWEB_init(uweb_response_fn, uweb_data_fn);
    UWEB_parse(req_str, pri_str);
    TEST_CHECK_EQ((int)(intptr_t)strstr(_response_buffer, "HTTP/1.1 400 Bad Request"),
                  (int)(intptr_t)_response_buffer);
    return TEST_RES_OK;
  } TEST_END(simple_request_bad)


  TEST(simple_post_request)
  {
    UW_STREAM req_str = make_char_stream(&stream[0],
       "POST /foo.php HTTP/1.1\n"
       "Host: localhost\n"
       "User-Agent: Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US; rv:1.9.1.5) Gecko/20091102 Firefox/3.5.5 (.NET CLR 3.5.30729)\n"
       "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\n"
       "Accept-Language: en-us,en;q=0.5\n"
       "Accept-Encoding: gzip,deflate\n"
       "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\n"
       "Keep-Alive: 300\n"
       "Connection: keep-alive\n"
       "Referer: http://localhost/test.php\n"
       "Content-Type: application/x-www-form-urlencoded\n"
       "Content-Length: 43\n"
       "\n"
       "first_name=John&last_name=Doe&action=Submit"
    );
    UW_STREAM pri_str = make_printf_stream(&stream[1]);
    UW_STREAM res_str = make_char_stream(&stream[2], "Hello world!\n");
    _response_stream = res_str;
    UWEB_init(uweb_response_fn, uweb_data_fn);
    UWEB_parse(req_str, pri_str);

    TEST_CHECK_EQ(strcmp(_response_buffer,
     "HTTP/1.1 200 OK\n"
     "Server: uWeb\n"
     "Content-Type: text/html; charset=utf-8\n"
     "Content-Length: 13\n"
     "Connection: close\n"
     "\n"
     "Hello world!\n"), 0);

    TEST_CHECK_EQ(strcmp(_data_buffer,
                         "first_name=John&last_name=Doe&action=Submit"), 0);
    return TEST_RES_OK;
  } TEST_END(simple_post_request)


  TEST(post_multipart_request) {

    UW_STREAM req_str = make_char_stream(&stream[0],
    "POST / HTTP/1.1\n"
    "Host: localhost:8000\n"
    "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:44.0) Gecko/20100101 Firefox/44.0\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\n"
    "Accept-Language: en-US,en;q=0.5\n"
    "Accept-Encoding: gzip, deflate\n"
    "Connection: keep-alive\n"
    "Content-Type: multipart/form-data; boundary=---------------------------812961605669629873499955133\n"
    "Content-Length: 465\n"
    "\n"
    "-----------------------------812961605669629873499955133\n"
    "Content-Disposition: form-data; name=\"text1\"\n"
    "\n"
    "text default\n"
    "-----------------------------812961605669629873499955133\n"
    "Content-Disposition: form-data; name=\"text2\"\n"
    "\n"
    "aωb\n"
    "-----------------------------812961605669629873499955133\n"
    "Content-Disposition: form-data; name=\"file1\"; filename=\"afile.txt\"\n"
    "Content-Type: text/plain\n"
    "\n"
    "Hello world!\n"
    "How's it hanging?\n"
    "\n"
    "-----------------------------812961605669629873499955133--\n"
    );

    UW_STREAM pri_str = make_printf_stream(&stream[1]);
    UW_STREAM res_str = make_char_stream(&stream[2], "Hello world!\n");
    _response_stream = res_str;
    UWEB_init(uweb_response_fn, uweb_data_fn);
    UWEB_parse(req_str, pri_str);

    TEST_CHECK_EQ(strcmp(_data_buffer,
      "[form-data; name=\"text1\"]text default\n"
      "[form-data; name=\"text2\"]aωb\n"
      "[form-data; name=\"file1\"; filename=\"afile.txt\"]Hello world!\n"
      "How's it hanging?\n"
      "\n"), 0);
    return TEST_RES_OK;
  } TEST_END(post_multipart_request)


  TEST(urlnencdec)
  {
    char dst[256];
    urlnencode(dst, "space space", 256);
    TEST_CHECK_EQ(strcmp(dst, "space+space"), 0);
    urlndecode(dst, "space+space", 256);
    TEST_CHECK_EQ(strcmp(dst, "space space"), 0);
    urlnencode(dst, "\\/<>\r\nåäö", 256);
    TEST_CHECK_EQ(strcmp(dst, "%5c%2f%3c%3e%0d%0a%c3%a5%c3%a4%c3%b6"), 0);
    urlndecode(dst, "%5c%2f%3c%3e%0d%0a%c3%a5%c3%a4%c3%b6", 256);
    TEST_CHECK_EQ(strcmp(dst, "\\/<>\r\nåäö"), 0);

    return TEST_RES_OK;
  } TEST_END(urlnencdec)


SUITE_END(uweb_tests)
