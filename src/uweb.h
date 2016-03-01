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

#ifndef UWEB_H_
#define UWEB_H_

#include "uweb_cfg.h"
#include "uweb_http.h"

#ifndef UWEB_SERVER_NAME
#define UWEB_SERVER_NAME "uWeb"
#endif

#ifndef UWEB_TX_MAX_LEN
#define UWEB_TX_MAX_LEN                2048
#endif

#ifndef UWEB_MAX_RESOURCE_LEN
#define UWEB_MAX_RESOURCE_LEN          256
#endif

#ifndef UWEB_MAX_HOST_LEN
#define UWEB_MAX_HOST_LEN              64
#endif

#ifndef UWEB_MAX_CONTENT_TYPE_LEN
#define UWEB_MAX_CONTENT_TYPE_LEN      128
#endif

#ifndef UWEB_MAX_CONNECTION_LEN
#define UWEB_MAX_CONNECTION_LEN        64
#endif

#ifndef UWEB_MAX_CONTENT_DISP_LEN
#define UWEB_MAX_CONTENT_DISP_LEN      256
#endif

#ifndef UWEB_REQ_BUF_MAX_LEN
#define UWEB_REQ_BUF_MAX_LEN           512
#endif

#ifndef UWEB_DBG
#define UWEB_DBG(...)
#endif

#ifndef UWEB_ASSERT
#define UWEB_ASSERT(x)
#endif

#ifndef UWEB_HTTP_MSG_TIMEOUT
#define UWEB_HTTP_MSG_TIMEOUT           "Request timed out\n"
#endif

#ifndef UWEB_HTTP_MSG_BAD_REQUEST
#define UWEB_HTTP_MSG_BAD_REQUEST       "Bad request\n"
#endif

#ifndef UWEB_HTTP_MSG_NOT_IMPL
#define UWEB_HTTP_MSG_NOT_IMPL          "Not implemented\n"
#endif

// Request return codes
typedef enum {
  UWEB_OK = 0,
  UWEB_CHUNKED
} uweb_response;

// Multipart content metadata
typedef struct {
  uint32_t multipart_nbr;
  char content_type[UWEB_MAX_CONTENT_TYPE_LEN];
  char content_disp[UWEB_MAX_CONTENT_DISP_LEN];
} uweb_request_multipart;

// Request metadata
typedef struct {
  uweb_http_req_method method;
  char resource[UWEB_MAX_RESOURCE_LEN];
  char host[UWEB_MAX_HOST_LEN];
  uint32_t content_length;
  char content_type[UWEB_MAX_CONTENT_TYPE_LEN];
  char connection[UWEB_MAX_CONNECTION_LEN];
  uint8_t chunked;
  uint32_t chunk_nbr;
  uweb_request_multipart cur_multipart;
} uweb_request_header;

// Data types
typedef enum {
  DATA_CONTENT = 0,
  DATA_CHUNK,
  DATA_MULTIPART
} uweb_data_type;


#define UWEB_UNKNONW_SZ          -1

typedef struct uweb_data_stream_s {
  void *user;
  /**
   * Total size of the content describing the stream, or
   * UWEB_UNKNONW_SZ.
   */
  int32_t total_sz;
  /**
   * Current available read size of the stream, in bytes.
   */
  int32_t avail_sz;
  /**
   * Current available write size of the stream, in bytes.
   */
  int32_t capacity_sz;
  /**
   * Reads from the stream into given buffer. Returns number of bytes
   * read.
   */
  int32_t (* read)(struct uweb_data_stream_s *stream, uint8_t *dst, uint32_t len);
  /**
   * Writes to the stream from given buffer. Returns number of bytes
   * written.
   */
  int32_t (* write)(struct uweb_data_stream_s *stream, uint8_t *src, uint32_t len);
} uweb_data_stream;

typedef uweb_data_stream *UW_STREAM;

/**
 * Serve a client request.
 * Can respond with a full data, or with chunked transfer.
 *
 * @param req - contains the client request data
 * @param res - stream where to put data to be sent to client
 * @param http_status - defaults to S200_OK, but can be altered if necessary
 * @param content_type - defaults to text/plain, but can be altered if necessary
 * @return SERVER_OK if all data to send to client is filled in iobuf;
 *         SERVER_CHUNK if server wants to send partial data to client via iobuf.
 *         If so, this function will be called repeatedly until user sends zero data.
 */
typedef uweb_response (*uweb_response_f)(
    uweb_request_header *req,
    UW_STREAM *res,
    uweb_http_status *http_status,
    char *content_type);

/**
 * Called when there is a request containing data to the server.
 * @param req - pointer to the client request
 * @param type - the data type
 * @param offset - offset in received data
 * @param data - pointer to the actual data
 * @param length - length of this piece of data
 */
typedef void (*uweb_data_f)(
    uweb_request_header *req,
    uweb_data_type type,
    uint32_t offset,
    uint8_t *data,
    uint32_t length);

/* Initiates uweb with given response and data functions */
void UWEB_init(uweb_response_f server_resp_f, uweb_data_f server_data_f);
/*  Call this when client has sent no data in a while */
void UWEB_timeout(UW_STREAM out);
/* Call this when there is client request data in stream in.
 * Response will be sent to out stream. */
void UWEB_parse(UW_STREAM in, UW_STREAM out);

/* Returns a url-decoded version of str */
char *urlndecode(char *dst, char *str, int num);
/* Returns a url-encoded version of str */
char *urlnencode(char *dst, char *str, int num);



#endif /* UWEB_H_ */
