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

#include "uweb.h"

static char nib2c(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'a' && c <= 'z') {
    return c - 'a' + 10;
  } else if (c >= 'A' && c <= 'Z') {
    return c - 'A' + 10;
  } else {
    return -1;
  }
}

static char c2nib(char n) {
  static const char const __hex[] = "0123456789abcdef";
  return __hex[n & 0xf];
}

char *urlnencode(char *dst, char *str, int num) {
  char *pstr = str;
  char *pdst = dst;
  while (*pstr && pdst < dst+num) {
    if ((*pstr >= '0' && *pstr <= '9') ||
        (*pstr >= 'A' && *pstr <= 'Z') ||
        (*pstr >= 'a' && *pstr <= 'z') ||
        *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~') {
      *pdst++ = *pstr;
    } else if (*pstr == ' ') {
      *pdst++ = '+';
    } else {
      if (pdst + 3 >= dst+num) {
        break;
      }
      *pdst++ = '%';
      *pdst++ = c2nib(*pstr >> 4);
      *pdst++ = c2nib(*pstr & 15);
    }
    pstr++;
  }
  *pdst = '\0';
  return dst;
}

char *urlndecode(char *dst, char *str, int num) {
  char *pstr = str;
  char *pdst = dst;
  while (*pstr && pdst < dst+num) {
    if (*pstr == '%') {
      if (pstr[1] && pstr[2]) {
        *pdst++ = nib2c(pstr[1]) << 4 | nib2c(pstr[2]);
        pstr += 2;
      }
    } else if (*pstr == '+') {
      *pdst++ = ' ';
    } else {
      *pdst++ = *pstr;
    }
    pstr++;
  }
  *pdst = '\0';
  return dst;
}
