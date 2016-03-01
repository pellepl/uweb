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

#ifndef UWEB_HTTP_H_
#define UWEB_HTTP_H_

#include "uweb_cfg.h"

typedef enum {
  _BAD_REQ = 0,
  // Requests a representation of the specified resource. Requests using GET should only retrieve data and should have no other effect. (This is also true of some other HTTP methods.)[1] The W3C has published guidance principles on this distinction, saying, "Web application design should be informed by the above principles, but also by the relevant limitations."
  GET,
  // Asks for the response identical to the one that would correspond to a GET request, but without the response body. This is useful for retrieving meta-information written in response headers, without having to transport the entire content.
  HEAD,
  // Requests that the server accept the entity enclosed in the request as a new subordinate of the web resource identified by the URI. The data POSTed might be, as examples, an annotation for existing resources; a message for a bulletin board, newsgroup, mailing list, or comment thread; a block of data that is the result of submitting a web form to a data-handling process; or an item to add to a database.
  POST,
  // Requests that the enclosed entity be stored under the supplied URI. If the URI refers to an already existing resource, it is modified; if the URI does not point to an existing resource, then the server can create the resource with that URI
  PUT,
  // Deletes the specified resource.
  DELETE,
  // Echoes back the received request so that a client can see what (if any) changes or additions have been made by intermediate servers.
  TRACE,
  // Returns the HTTP methods that the server supports for the specified URL. This can be used to check the functionality of a web server by requesting '*' instead of a specific resource
  OPTIONS,
  // Converts the request connection to a transparent TCP/IP tunnel, usually to facilitate SSL-encrypted communication (HTTPS) through an unencrypted HTTP proxy
  CONNECT,
  // Is used to apply partial modifications to a resource
  PATCH,
  _REQ_METHOD_COUNT
} uweb_http_req_method;

static const char* const UWEB_HTTP_REQ_METHODS[] = {
  "<BAD>",
  "GET",
  "HEAD",
  "POST",
  "PUT",
  "DELETE",
  "TRACE",
  "OPTIONS",
  "CONNECT",
  "PATCH",
};

typedef enum {
  FCONNECTION = 0,
  FHOST,
  FCONTENT_LENGTH,
  FCONTENT_TYPE,
  FTRANSFER_ENCODING,
  FCONTENT_DISPOSITION,
  _FIELD_COUNT
} uweb_http_fields;

static const char* const UWEB_HTTP_FIELDS[] = {
  "Connection:",
  "Host:",
  "Content-Length:",
  "Content-Type:",
  "Transfer-Encoding:",
  "Content-Disposition:",
};


typedef enum {
  S100_CONTINUE = 0,
  S101_SWITCHING_PROTOCOLS,
  S200_OK,
  S201_CREATED,
  S202_ACCEPTED,
  S203_NON_AUTH_INFO,
  S204_NO_CONTENT,
  S205_RESET_CONTENT,
  S206_PARTIAL_CONTENT,
  S300_MULT_CHOICES,
  S301_MOVED_PERMANENTLY,
  S302_FOUND,
  S303_SEE_OTHER,
  S304_NOT_MODIFIED,
  S305_USE_PROXY,
  S307_TEMPORY_REDIRECT,
  S400_BAD_REQ,
  S401_UNAUTH,
  S402_PAYMENT_REQUIRED,
  S403_FORBIDDEN,
  S404_NOT_FOUND,
  S405_METHOD_NOT_ALLOWED,
  S406_NOT_ACCEPTABLE,
  S407_PROXY_AUTH_REQ,
  S408_REQUEST_TIMEOUT,
  S409_CONFLICT,
  S410_GONE,
  S411_LENGTH_REQ,
  S412_PRECONDITION_FAILED,
  S413_REQ_ENTITY_TOO_LARGE,
  S414_REQ_URI_TOO_LONG,
  S415_UNSUPPORTED_MEDIA_TYPE,
  S416_REQ_RANGE_NOT_SATISFIABLE,
  S417_EXPECTATION_FAILED,
  S500_INTERNAL_SERVER_ERROR,
  S501_NOT_IMPLEMENTED,
  S502_BAD_GATEWAY,
  S503_SERVICE_UNAVAILABLE,
  S504_GATEWAY_TIMEOUT,
  S505_HTTP_VERSION_NOT_SUPPORTED,
} uweb_http_status;

static const uint16_t const UWEB_HTTP_STATUS_NUM[] = {
  100, 101,
  200, 201, 202, 203, 204, 205, 206,
  300, 301, 302, 303, 304, 305, 307,
  400, 401, 402, 403, 404, 405, 406, 407, 408, 409,
  410, 411, 412, 413, 414, 415, 416, 417,
  500, 501, 502, 503, 504, 505,
};

static const char * const UWEB_HTTP_STATUS_STRING[] = {
  "Continue",
  "Switching Protocols",
  "OK",
  "Created",
  "Accepted",
  "Non-Authoritative Information",
  "No Content",
  "Reset Content",
  "Partial Content",
  "Multiple Choices",
  "Moved Permanently",
  "Found",
  "See Other",
  "Not Modified",
  "Use Proxy",
  "Temporary Redirect",
  "Bad Request",
  "Unauthorized",
  "Payment Required",
  "Forbidden",
  "Not Found",
  "Method Not Allowed",
  "Not Acceptable",
  "Proxy Authentication Required",
  "Request Time-out",
  "Conflict",
  "Gone",
  "Length Required",
  "Precondition Failed",
  "Request Entity Too Large",
  "Request-URI Too Large",
  "Unsupported Media Type",
  "Requested range not satisfiable",
  "Expectation Failed",
  "Internal Server Error",
  "Not Implemented",
  "Bad Gateway",
  "Service Unavailable",
  "Gateway Time-out",
  "HTTP Version not supported",
};


#endif /* UWEB_HTTP_H_ */
