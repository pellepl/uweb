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

/*
 * Petite web server implementation
 * - supports multipart and chunked transfers
 */

#include "uweb.h"

static const char * const ERR_HTTP_TIMEOUT = UWEB_HTTP_MSG_TIMEOUT;
static const char * const ERR_HTTP_BAD_REQUEST = UWEB_HTTP_MSG_BAD_REQUEST;
static const char * const ERR_HTTP_NOT_IMPL = UWEB_HTTP_MSG_NOT_IMPL;

typedef enum {
  HEADER_METHOD = 0,
  HEADER_FIELDS,
  CONTENT,
  MULTI_CONTENT_HEADER,
  MULTI_CONTENT_DATA,
  CHUNK_DATA_HEADER,
  CHUNK_DATA,
  CHUNK_DATA_END,
  CHUNK_FOOTER,
} us_state;

static struct {
  uweb_response_f server_resp_f;
  uweb_data_f server_data_f;

  uint8_t tx_buf[UWEB_TX_MAX_LEN];

  us_state state;

  uweb_request_header req;

  uint16_t header_line;

  char *multipart_boundary;
  uint8_t multipart_boundary_ix;
  uint8_t multipart_boundary_len;
  uint8_t multipart_delim;
  uint32_t received_multipart_len;

  char req_buf[UWEB_REQ_BUF_MAX_LEN+1];
  volatile uint16_t req_buf_len;

  uint32_t chunk_ix;
  uint32_t chunk_len;
  uint32_t received_content_len;
} uweb;

static char *_uweb_space_strip(char *);

// clear incoming request and reset server states
static void _uweb_clear_req(uweb_request_header *req) {
  memset(req, 0, sizeof(uweb_request_header));
  uweb.state = HEADER_METHOD;
  uweb.header_line = 0;
}

static void _uweb_sendf(UW_STREAM out, const char *str, ...) {
  va_list arg_p;
  va_start(arg_p, str);
  int len = vsprintf((char *)uweb.tx_buf, str, arg_p);
  va_end(arg_p);

  out->write(out, uweb.tx_buf, len);
}

// send data to client
static void _uweb_send_data(UW_STREAM out, UW_STREAM data) {
  while (data->avail_sz > 0) {
    int32_t rlen = UWEB_TX_MAX_LEN < data->avail_sz ? UWEB_TX_MAX_LEN : data->avail_sz;
    rlen = data->read(data, uweb.tx_buf, rlen);
    out->write(out, uweb.tx_buf, rlen);
  } // while tx
}

static void _uweb_send_data_fixed(UW_STREAM out, UW_STREAM data, int32_t len) {
  while (len > 0) {
    int32_t rlen = UWEB_TX_MAX_LEN < data->avail_sz ? UWEB_TX_MAX_LEN : data->avail_sz;
    rlen = len < rlen ? len : rlen;
    rlen = data->read(data, uweb.tx_buf, rlen);
    out->write(out, uweb.tx_buf, rlen);
    len -= rlen;
  } // while tx
}

// request error response
static void _uweb_error(UW_STREAM out, uweb_http_status http_status, const char *error_page) {
  _uweb_sendf(out,
    "HTTP/1.1 %i %s\n"
    "Server: "UWEB_SERVER_NAME"\n"
    "Content-Type: text/html; charset=UTF-8\n"
    "Content-Length: %i\n"
    "Connection: close\n"
    "\n",
    UWEB_HTTP_STATUS_NUM[http_status], UWEB_HTTP_STATUS_STRING[http_status],
    strlen(error_page));
  _uweb_clear_req(&uweb.req);
  uweb.chunk_ix = 0;
  uweb.chunk_len = 0;
}

// serve a request and send answer
static void _uweb_request(UW_STREAM out, uweb_request_header *req) {
  UWEB_DBG("req method %s\n", UWEB_HTTP_REQ_METHODS[req->method]);
  UWEB_DBG("        res    %s\n", req->resource);
  UWEB_DBG("        host   %s\n", req->host);
  UWEB_DBG("        type   %s\n", req->content_type);
  if (req->chunked) {
    UWEB_DBG("        chunked\n");
  } else {
    UWEB_DBG("        length %i\n", req->content_length);
  }
  UWEB_DBG("        conn   %s\n", req->connection);

  if (req->method == _BAD_REQ) {
    UWEB_DBG("BAD REQUEST\n");
    _uweb_error(out, S400_BAD_REQ, ERR_HTTP_BAD_REQUEST);
    return;
  }

  char content_type[UWEB_MAX_CONTENT_TYPE_LEN];
  uweb_http_status http_status = S200_OK;
  char *extra_headers = 0;

  strncpy(content_type, "text/html; charset=utf-8", UWEB_MAX_CONTENT_TYPE_LEN);

  uweb_response res = UWEB_OK;
  UW_STREAM response_stream;
  if (uweb.server_resp_f){
    res = uweb.server_resp_f(req, &response_stream, &http_status, content_type, &extra_headers);
  } else {
    _uweb_error(out, S501_NOT_IMPLEMENTED, ERR_HTTP_NOT_IMPL);
    return;
  }

  if (res == UWEB_OK) {
    // plain response
    _uweb_sendf(out,
      "HTTP/1.1 %i %s\n"
      "Server: "UWEB_SERVER_NAME"\n"
      "Content-Type: %s\n"
      "Content-Length: %i\n"
      "%s"
      "Connection: close\n"
      "\n",
      UWEB_HTTP_STATUS_NUM[http_status], UWEB_HTTP_STATUS_STRING[http_status],
      content_type,
      response_stream->total_sz,
      extra_headers ? extra_headers : "");
    if (req->method != HEAD) {
      _uweb_send_data(out, response_stream);
    }
  } else if (res == UWEB_CHUNKED) {
    // chunked response
    _uweb_sendf(out,
      "HTTP/1.1 %i %s\n"
      "Server: "UWEB_SERVER_NAME"\n"
      "Content-Type: %s\n"
      "%s"
      "Transfer-Encoding: chunked\n"
      "\n",
      UWEB_HTTP_STATUS_NUM[http_status], UWEB_HTTP_STATUS_STRING[http_status],
      content_type,
      extra_headers ? extra_headers : "");
    if (req->method != HEAD) {
      uint32_t chunk_len;
      while (response_stream && (chunk_len = response_stream->avail_sz) > 0) {
        _uweb_sendf(out, "%x; chunk %i\r\n", chunk_len, req->chunk_nbr);
        _uweb_send_data_fixed(out, response_stream, chunk_len);
        _uweb_sendf(out, "\r\n");
        uweb.req.chunk_nbr++;
        (void)uweb.server_resp_f(req, &response_stream, &http_status,
            content_type, &extra_headers); // from now on, we ignore response
      }
      _uweb_sendf(out, "0\r\n\r\n");
    }
  }
}

// handle HTTP header line
static void _uweb_handle_http_header_line(UW_STREAM out, char *s, uint16_t len, UW_STREAM in) {
  (void)in;
  if (len > 0) {
    // http header element
    switch (uweb.state) {
    case HEADER_METHOD: {
      uint32_t i;
      for (i = 0; i < _REQ_METHOD_COUNT; i++) {
        if (strstr(s, UWEB_HTTP_REQ_METHODS[i]) == s) {
          uweb.req.method = i;
          char *resource = _uweb_space_strip(&s[strlen(UWEB_HTTP_REQ_METHODS[i])]);
          char *space = (char *)strchr(resource, ' ');
          if (space) {
            *space = 0;
          }
          strcpy(uweb.req.resource, resource);
          break;
        }
      } // per method
      uweb.state = HEADER_FIELDS;
      break;
    }

    case HEADER_FIELDS: {
      uint32_t i;
      for (i = 0; i < _FIELD_COUNT; i++) {
        if (strstr(s, UWEB_HTTP_FIELDS[i]) == s) {
          switch (i) {
          case FCONNECTION: {
            char *value = _uweb_space_strip(&s[strlen(UWEB_HTTP_FIELDS[i])]);
            strncpy(uweb.req.connection, value, UWEB_MAX_CONNECTION_LEN);
            break;
          }
          case FHOST: {
            char *value = _uweb_space_strip(&s[strlen(UWEB_HTTP_FIELDS[i])]);
            strncpy(uweb.req.host, value, UWEB_MAX_HOST_LEN);
            break;
          }
          case FCONTENT_TYPE: {
            char *value = _uweb_space_strip(&s[strlen(UWEB_HTTP_FIELDS[i])]);
            strncpy(uweb.req.content_type, value, UWEB_MAX_CONTENT_DISP_LEN);
            break;
          }
          case FCONTENT_LENGTH: {
            char *value = _uweb_space_strip(&s[strlen(UWEB_HTTP_FIELDS[i])]);
            uweb.req.content_length = atoi(value);
            break;
          }
          case FTRANSFER_ENCODING: {
            char *value = _uweb_space_strip(&s[strlen(UWEB_HTTP_FIELDS[i])]);
            uweb.req.chunked = strcmp("chunked", value) == 0;
            break;
          }
          } // switch field
          break;
        }
      } // per field

      break;
    }
    default:
      UWEB_ASSERT(0);
      break;
    }
  }
  else // if (len == 0) meaning blank line
  {
    // end of HTTP header

    // serve request
    _uweb_request(out, &uweb.req);

    // expecting data?
    if (uweb.req.chunked) {
      // --- chunked content
      if (uweb.req.content_length > 0) {
        UWEB_DBG("BAD CHUNK REQUEST, Content-Length > 0\n");
        _uweb_error(out, S400_BAD_REQ, ERR_HTTP_BAD_REQUEST);
        return;
      }
      uweb.state = CHUNK_DATA_HEADER;
      uweb.chunk_ix = 0;
      uweb.chunk_len = 0;
      uweb.received_content_len = 0;
    } else  if (uweb.req.content_length > 0) {
      // --- plain content
      uweb.received_content_len = 0;
      uweb.state = CONTENT;
      UWEB_DBG("getting content length %i, available now %i\n", uweb.req.content_length, in->avail_sz);

      // --- multipart content
      if (strstr(uweb.req.content_type, "multipart/form-data") == uweb.req.content_type) {
        // get boundary string
        char *boundary_start = strstr(uweb.req.content_type, "boundary");
        if (boundary_start == 0) {
          UWEB_DBG("BAD MULTIPART REQUEST, boundary not found\n");
          _uweb_error(out, S400_BAD_REQ, ERR_HTTP_BAD_REQUEST);
          return;
        }
        boundary_start += 8; // "boundary"
        boundary_start = _uweb_space_strip(boundary_start);
        if (*boundary_start != '=') {
          UWEB_DBG("BAD MULTIPART REQUEST, = not found\n");
          _uweb_error(out, S400_BAD_REQ, ERR_HTTP_BAD_REQUEST);
          return;
        }
        boundary_start++;
        boundary_start = _uweb_space_strip(boundary_start);
        if (strlen(boundary_start) == 0) {
          UWEB_DBG("BAD MULTIPART REQUEST, boundary id not found\n");
          _uweb_error(out, S400_BAD_REQ, ERR_HTTP_BAD_REQUEST);
          return;
        }
        uweb.multipart_boundary = boundary_start;
        uweb.multipart_boundary_ix = 0;
        uweb.multipart_delim = 0;
        uweb.multipart_boundary_len = strlen(boundary_start);
        uweb.req.cur_multipart.multipart_nbr = 0;
        uweb.state = MULTI_CONTENT_HEADER;
        uweb.header_line = 0;
        UWEB_DBG("boundary start: %s\n", boundary_start);
      }

    } else {
      // back to expecting a http header
      _uweb_clear_req(&uweb.req);
    }

    return;
  }
}

// handle multipart content header line
static void _uweb_handle_multi_content_header_line(UW_STREAM out, char *s, uint16_t len, UW_STREAM in) {
  (void)out;
  (void)in;
  char *boundary_start;
  if (strstr(s, "--") == s && (boundary_start = strstr(s+2, uweb.multipart_boundary))) {
    // boundary match
    if (strstr(boundary_start + uweb.multipart_boundary_len, "--")) {
      // end of multipart message
      // back to expecting a http header
      UWEB_DBG("multipart finished\n");
      _uweb_clear_req(&uweb.req);
    } else {
      // multipart section
      UWEB_DBG("multipart section %i header\n", uweb.req.cur_multipart.multipart_nbr);
    }
  } else if (len == 0) {
    // end of multipart header, start of multipart data
    UWEB_DBG("multipart data section %i [%s]\n", uweb.req.cur_multipart.multipart_nbr,
        uweb.req.cur_multipart.content_disp);
    uweb.state = MULTI_CONTENT_DATA;
    uweb.multipart_boundary_ix = 0;
    uweb.multipart_delim = 0;
    uweb.received_multipart_len = 0;
  } else {
    // multipart header, get fields
    uint32_t i;
    for (i = 0; i < _FIELD_COUNT; i++) {
      if (strstr(s, UWEB_HTTP_FIELDS[i]) == s) {
        switch (i) {
        case FCONTENT_DISPOSITION: {
          char *value = _uweb_space_strip(&s[strlen(UWEB_HTTP_FIELDS[i])]);
          strncpy(uweb.req.cur_multipart.content_disp, value, UWEB_MAX_CONTENT_DISP_LEN);
          break;
        }
        case FCONTENT_TYPE: {
          char *value = _uweb_space_strip(&s[strlen(UWEB_HTTP_FIELDS[i])]);
          strncpy(uweb.req.cur_multipart.content_type, value, UWEB_MAX_CONTENT_TYPE_LEN);
          break;
        }
        } // switch field
        break;
      }
    } // per field
  }
}

// handle chunk header line
static void _uweb_handle_chunk_header_line(UW_STREAM out, char *s, uint16_t len, UW_STREAM in) {
  (void)out;
  (void)in;
  (void)len;
  char *start = _uweb_space_strip(s);
  char *end = (char *)strchr(start, ';');
  if (end) *end = 0;
  uweb.chunk_len = strtol(start, 0, 16);//atoin(start, 16, strlen(start));
  if (uweb.chunk_len > 0) {
    UWEB_DBG("chunk %i, length %i\n", uweb.chunk_ix, uweb.chunk_len);
    uweb.state = CHUNK_DATA;
  } else {
    UWEB_DBG("chunks finished, footer\n");
    uweb.state = CHUNK_FOOTER;
    uweb.header_line = 0;
  }
}

// handle chunk footer line
static void _uweb_handle_chunk_footer_line(UW_STREAM out, char *s, uint16_t len, UW_STREAM in) {
  (void)out;
  (void)in;
  (void)s;
  if (len == 0) {
    _uweb_clear_req(&uweb.req);
  }
}

// http data timeout
void UWEB_timeout(UW_STREAM out) {
  if (uweb.state != HEADER_METHOD) {
    UWEB_DBG("request timeout\n");
    _uweb_error(out, S408_REQUEST_TIMEOUT, ERR_HTTP_TIMEOUT);
  }
}

// parse http data characters
void UWEB_parse(UW_STREAM in, UW_STREAM out) {
  int32_t rx;
  while ((rx = in->avail_sz) > 0) {
    switch (uweb.state) {

    // --- HEADER PARSING

    case MULTI_CONTENT_HEADER:
    case CHUNK_DATA_HEADER:
    case CHUNK_DATA_END:
    case HEADER_METHOD:
    case CHUNK_FOOTER:
    case HEADER_FIELDS: {
      uint8_t c;
      int32_t res = in->read(in, &c, 1);
      if (res < 1) {
        return;
      }

      if (c == '\r') continue;
      if (uweb.req_buf_len >= UWEB_REQ_BUF_MAX_LEN || c == '\n') {
        if (uweb.req_buf_len >= UWEB_REQ_BUF_MAX_LEN) {
          uweb.req_buf[UWEB_REQ_BUF_MAX_LEN] = 0;
        } else {
          uweb.req_buf[uweb.req_buf_len] = 0;
        }
        if (uweb.state == CHUNK_DATA_HEADER) {
          UWEB_DBG("CHUNK-HDR: %s\n", uweb.req_buf);
          _uweb_handle_chunk_header_line(out, uweb.req_buf, uweb.req_buf_len, in);
        } else if (uweb.state == CHUNK_DATA_END) {
          // ignore
          UWEB_DBG("CHUNK-DATA_END\n");
          uweb.state = CHUNK_DATA_HEADER;
          uweb.header_line = 0;
          uweb.received_content_len = 0;
        } else if (uweb.state == CHUNK_FOOTER) {
          UWEB_DBG("CHUNK-FOOTER: %s\n", uweb.req_buf);
          _uweb_handle_chunk_footer_line(out, uweb.req_buf, uweb.req_buf_len, in);
        } else if (uweb.state == MULTI_CONTENT_HEADER) {
          UWEB_DBG("MULTI-HDR:%s\n", uweb.req_buf);
          _uweb_handle_multi_content_header_line(out, uweb.req_buf, uweb.req_buf_len, in);
        } else {
          UWEB_DBG("HTTP-HDR: %s\n", uweb.req_buf);
          _uweb_handle_http_header_line(out, uweb.req_buf, uweb.req_buf_len, in);
        }
        uweb.header_line++;
        uweb.req_buf_len = 0;
      }

      if (c != '\n') {
        uweb.req_buf[uweb.req_buf_len++] = c;
      }
      break;
    }

    // --- DATA PARSING

    case CONTENT:
    case CHUNK_DATA: {
      // known content size
      int32_t len = rx < UWEB_REQ_BUF_MAX_LEN ? rx : UWEB_REQ_BUF_MAX_LEN;
      if (uweb.state == CONTENT) {
        len = len < (int32_t)(uweb.req.content_length - uweb.received_content_len) ?
            len : (int32_t)(uweb.req.content_length - uweb.received_content_len);
      } else if (uweb.state == CHUNK_DATA) {
        len = len < (int32_t)(uweb.chunk_len - uweb.received_content_len) ?
            len : (int32_t)(uweb.chunk_len - uweb.received_content_len);
      }
      len = in->read(in, (uint8_t *)uweb.req_buf, len);
      if (len <= 0) return;

      if (uweb.server_data_f) {
        // report data
        uweb.server_data_f(&uweb.req, uweb.state == CONTENT ? DATA_CONTENT : DATA_CHUNK,
            uweb.received_content_len, (uint8_t *)uweb.req_buf, len);
      }

      uweb.received_content_len += len;
      if (uweb.req.chunked) {
        // chunking data
        if (uweb.received_content_len == uweb.chunk_len) {
          UWEB_DBG("chunk %i received\n", uweb.chunk_ix);
          uweb.chunk_ix++;
          uweb.state = CHUNK_DATA_END;
          uweb.header_line = 0;
        }
      } else {
        // content data
        if (uweb.received_content_len == uweb.req.content_length) {
          UWEB_DBG("all content received\n");
          _uweb_clear_req(&uweb.req);
        }
      }
      break;
    }
    case MULTI_CONTENT_DATA: {
      uint8_t flush_buf = 0;
      uint8_t c;
      int32_t res = in->read(in, &c, 1);
      if (res < 1) {
        return;
      }

      uweb.req_buf[uweb.req_buf_len++] = c;

      if (c == '\n') {
        // report data at each newline for simplicity with boundary recognition
        if (uweb.server_data_f) {
          uweb.server_data_f(&uweb.req, DATA_MULTIPART, uweb.received_multipart_len,
              (uint8_t*)uweb.req_buf, uweb.req_buf_len);
        }
        uweb.received_multipart_len += uweb.req_buf_len;
        uweb.req_buf_len = 0;
      }

      // find boundary --<BOUNDARY>(--)
      if (c == '-' && uweb.multipart_delim < 2) {
        uweb.multipart_delim++;
      } else if (uweb.multipart_delim == 2 &&
          c == uweb.multipart_boundary[uweb.multipart_boundary_ix]) {
        uweb.multipart_boundary_ix++;
        if (uweb.multipart_boundary_ix == uweb.multipart_boundary_len) {
          uweb.multipart_boundary_ix = 0;
          uweb.multipart_delim = 0;
          uweb.req.cur_multipart.multipart_nbr++;
          uweb.state = MULTI_CONTENT_HEADER;
          _uweb_handle_multi_content_header_line(out,
              &uweb.req_buf[uweb.req_buf_len - uweb.multipart_boundary_len - 2],
              uweb.multipart_boundary_len + 2,
              in);
          continue;
        }
      } else {
        // no boundary indication, pure data
        if (uweb.multipart_delim > 0 || uweb.multipart_boundary_ix > 0) {
          // report eaten data believed to be boundary
          flush_buf = 1;
        }
        uweb.multipart_delim = 0;
        uweb.multipart_boundary_ix = 0;
      }

      if (flush_buf || uweb.req_buf_len >= UWEB_REQ_BUF_MAX_LEN) {
        // flush req or buffer overflow, report
        if (uweb.server_data_f) {
          uweb.server_data_f(&uweb.req, DATA_MULTIPART, uweb.received_multipart_len,
              (uint8_t*)uweb.req_buf, uweb.req_buf_len);
        }
        uweb.received_multipart_len += uweb.req_buf_len;
        uweb.req_buf_len = 0;
      }

      uweb.received_content_len++;
      if (uweb.received_content_len == uweb.req.content_length) {
        if (uweb.server_data_f) {
          // report last bytes if we have not left this state already
          uweb.server_data_f(&uweb.req, DATA_MULTIPART, uweb.received_multipart_len,
              (uint8_t *)uweb.req_buf, uweb.req_buf_len);
        }
        uweb.received_multipart_len += uweb.req_buf_len;
        UWEB_DBG("all multi content received %i\n", uweb.req.content_length);
        _uweb_clear_req(&uweb.req);
      }

      break;
    }
    } // switch state
  } // while rx avail
}

void UWEB_init(uweb_response_f server_resp_f, uweb_data_f server_data_f) {
  memset(&uweb, 0, sizeof(uweb));
  uweb.server_resp_f = server_resp_f;
  uweb.server_data_f = server_data_f;
}

static char *_uweb_space_strip(char *s) {
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  return s;
}
