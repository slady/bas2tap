#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const int IN_BUFFER_SIZE = 1024;
const int OUT_BUFFER_SIZE = 0x10000;

FILE *fin;

char *obuf, *ibuf;

// c - character read from the input file
// p - position in obuf
// b - position in ibuf
// r - remember where to put the last line length into the obuf output buffer
int c, r, p, b;

int initBufs(FILE *_fin)
{
  fin = _fin;

  obuf = malloc(OUT_BUFFER_SIZE + 10);

  if (NULL == obuf) {
    return 0;
  }

  ibuf = malloc(IN_BUFFER_SIZE);

  if (NULL == ibuf) {
    free(obuf);
    return 0;
  }

  r = -1;
  p = 0;
  b = 0;

  return 1;
}

void freeBufs(void)
{
  free(ibuf);
  free(obuf);
}

void clearIBuf(void)
{
  memset(ibuf, 0, IN_BUFFER_SIZE);
  b = 0;
}

char *getIBuf()
{
  return ibuf;
}

char *getOBuf()
{
  return obuf;
}

int getIC()
{
  c = getc(fin);
  return c;
}

void ungetC()
{
  ungetc(c, fin);
}

void addOutC(void)
{
  obuf[p++] = c;
}

void addOutCh(int ch)
{
  obuf[p++] = ch;
}

void addInC(void)
{
  ibuf[b++] = c;
}

void copyInToOut(void)
{
  for (int i = 0; i < b; i++) {
    obuf[p++] = ibuf[i];
  }
}

void markLineLength(void)
{
  r = p;
  p += 2;
}

void writeLineLength(void)
{
  if (r >= 0) {
    obuf[p++] = 0x0D;
    int n = p - r - 2;
    obuf[r++] = n & 0xFF;
    obuf[r++] = (n / 0x100) & 0xFF;
  }
}

int getLength(void)
{
  return p;
}

int iBufIs(const char *tstStr)
{
  return strcasecmp(ibuf, tstStr) == 0;
}
