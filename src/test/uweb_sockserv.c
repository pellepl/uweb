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

#include "testrunner.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include "../uweb.h"

#define CONTENT_PATH "test_data"

static uweb_data_stream in_stream, out_stream, res_stream;
static volatile int running;


static int32_t sockstr_read(UW_STREAM str, uint8_t *dst, uint32_t len) {
  int32_t r = recv((intptr_t)str->user, dst, len, 0);
  return r;
}

static int32_t sockstr_write(UW_STREAM str, uint8_t *src, uint32_t len) {
  int l;
  while (len) {
    l = write((intptr_t)str->user, src, len);
    if (l < 0) break;
    len -= l;
  }
  return l;
}

UW_STREAM make_socket_stream(UW_STREAM str, int sockfd)
{
  str->total_sz = -1;
  str->capacity_sz = 0;
  str->avail_sz = 256;
  str->user = (void *)((intptr_t)sockfd);
  str->read = sockstr_read;
  str->write = sockstr_write;
  return str;
}


static int32_t filestr_read(UW_STREAM str, uint8_t *dst, uint32_t len) {
  int32_t r = read((intptr_t)str->user, dst, len);
  if (r > 0) {
    str->total_sz -= r;
    str->avail_sz -= r;
  }
  if (r <= 0 || str->total_sz == 0) {
    close((intptr_t)str->user);
    str->avail_sz = 0;
  }
  return r;
}

static int32_t filestr_write(UW_STREAM str, uint8_t *src, uint32_t len) {
  int l;
  while (len) {
    l = write((intptr_t)str->user, src, len);
    if (l < 0) break;
    len -= l;
  }
  return l;
}

UW_STREAM make_file_stream(UW_STREAM str, int fd)
{
  uint32_t sz = lseek(fd, 0L, SEEK_END);
  lseek(fd, 0L, SEEK_SET);
  str->total_sz = sz;
  str->capacity_sz = 0;
  str->avail_sz = sz;
  str->user = (void *)((intptr_t)fd);
  str->read = filestr_read;
  str->write = filestr_write;
  return str;
}

static int32_t nullstr_read(UW_STREAM str, uint8_t *dst, uint32_t len) {
  return 0;
}
static int32_t nullstr_write(UW_STREAM str, uint8_t *src, uint32_t len) {
  return 0;
}
UW_STREAM make_null_stream(UW_STREAM str)
{
  str->total_sz = 0;
  str->capacity_sz = 0;
  str->avail_sz = 0;
  str->read = nullstr_read;
  str->write = nullstr_write;
  return str;
}

static uweb_response uweb_response_fn(uweb_request_header *req, UW_STREAM *res, uweb_http_status *http_status, char *content_type, char **extra_headers) {
  if (req->chunk_nbr == 0) {
    printf("opening %s\n", &req->resource[1]);
    char path[512];
    if (strcmp("/exit", req->resource) == 0 ||
        strcmp("/quit", req->resource) == 0 ||
        strcmp("/stop", req->resource) == 0 ||
        strcmp("/halt", req->resource) == 0) {
      printf("req stop server\n");
      running = 0;
      in_stream.avail_sz = 0;
    } else if (strlen(req->resource) == 1) { // "/"
      sprintf(path, "./%s/index.html", CONTENT_PATH);
    } else {
      sprintf(path, "./%s%s", CONTENT_PATH, req->resource);
    }

    int fd = open(path, O_RDONLY, S_IRUSR | S_IWUSR);
    if (fd > 0) {
      make_file_stream(&res_stream, fd);
    } else {
      make_null_stream(&res_stream);
      *http_status = S404_NOT_FOUND;
    }
  }
  *res = &res_stream;

  return UWEB_CHUNKED;
}

static void uweb_data_fn(uweb_request_header *req, uweb_data_type type, uint32_t offset, uint8_t *data, uint32_t length) {
  printf("got data: ");
  uint32_t i;
  for (i = 0; i < length; i++) {
    printf("%02x", data[i]);
  }
  printf("   ");
  for (i = 0; i < length; i++) {
    printf("%c", data[i] >= ' ' ? data[i] : '.');
  }
  printf("\n");
}

void start_socket_server(int port) {
  running = 1;
  int sockfd, client_sock, clilen, read_size;
  struct sockaddr_in server, client;

  // create socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    printf("could not create socket\n");
    return;
  }

  // prepare the sockaddr_in structure
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(port);

  // bind
  if (bind(sockfd, (struct sockaddr *) &server, sizeof(server)) < 0) {
    perror("bind failed.");
    return;
  }

  // listen
  listen(sockfd, 2);

  // accept and incoming connection
  printf("uweb server started @ port %i\n", port);
  clilen = sizeof(struct sockaddr_in);

  UWEB_init(uweb_response_fn, uweb_data_fn);

  while (running) {
    // accept connection from an incoming client
    client_sock = accept(sockfd, (struct sockaddr *)&client,
                         (socklen_t *)&clilen);
    if (client_sock < 0) {
      perror("accept failed");
      close(sockfd);
      return;
    }

    //fcntl(sockfd, F_SETFL, O_NONBLOCK);
    printf(">>> accepted\n");

    UW_STREAM req_str = make_socket_stream(&in_stream, client_sock);
    UW_STREAM out_str = make_socket_stream(&out_stream, client_sock);

    UWEB_parse(req_str, out_str);

    close(client_sock);
    printf("<<< served\n");
  }

  close(sockfd);
}
