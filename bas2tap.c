#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

const int IN_BUFFER_SIZE = 1024;
const int OUT_BUFFER_SIZE = 0x10000;

const char *errIn = "Cannot open input file: %s\n";
const char *errOut = "Cannot open output file: %s\n";
const char *errMem = "Error: Not enough memory!\n";
const char *errProg = "Internal program error!\n";
const char *errSyntLineNum = "Syntax error: Line number\n";
const char *errPos = "Location: line %d column %d\n";

const int FIRST_COMMAND = 0xA5;
const char *COMMAND_LIST[] = { "RND", "INKEY$", "PI", "FN", "POINT", "SCREEN$", "ATTR", "AT", "TAB", "VAL$", "CODE",
  "VAL", "LEN", "SIN", "COS", "TAN", "ASN", "ACS", "ATN", "LN", "EXP", "INT", "SQR", "SGN", "ABS", "PEEK", "IN",
  "USR", "STR$", "CHR$", "NOT", "BIN", "OR", "AND", "<=", ">=", "<>", "LINE", "THEN", "TO", "STEP", "DEFFN", "CAT",
  "FORMAT", "MOVE", "ERASE", "OPEN#", "CLOSE#", "MERGE", "VERIFY", "BEEP", "CIRCLE", "INK", "PAPER", "FLASH", "BRIGHT", "INVERSE", "OVER", "OUT",
  "LPRINT", "LLIST", "STOP", "READ", "DATA", "RESTORE", "NEW", "BORDER", "CONTINUE", "DIM", "REM", "FOR", "GOTO", "GOSUB", "INPUT", "LOAD",
  "LIST", "LET", "PAUSE", "NEXT", "POKE", "PRINT", "PLOT", "RUN", "SAVE", "RANDOMIZE", "IF", "CLS", "DRAW", "CLEAR", "RETURN", "COPY" };
// changed key words:
// "DEF FN", "OPEN #", "CLOSE #", "GO TO", "GO SUB"
const int COMMAND_LIST_SIZE = (sizeof(COMMAND_LIST) / sizeof(COMMAND_LIST[0]));

const int ZX_FLOAT_MANTISSA_BASE = 128;
const float ZX_FLOAT_SIGN_FIX = .5;

typedef enum { LINE_START, LINE_NUMBER, COMMAND_EXPECTED,
    READING_STRING, READING_COMMAND, READING_NUMBER, READING_NUMBER_DECIMAL,
    SINGLE_LINE_COMMENT } state;

void writeByte(const int value, int *checksumPointer, FILE *fout)
{
  putc(value, fout);
  *checksumPointer ^= value;
}

void writeDoubleByte(const int size, int *checksumPointer, FILE *fout)
{
  writeByte(size & 0xFF, checksumPointer, fout);
  writeByte((size / 0x100) & 0xFF, checksumPointer, fout);
}

void writeFile(const int size, const char *name, const char *buf, FILE *fout)
{
  int len = strlen(name), i, checksum;

  // writing the header packet
  writeByte(0x13, &checksum, fout);
  checksum = 0;
  writeByte(0, &checksum, fout);
  writeByte(0, &checksum, fout);
  writeByte(0, &checksum, fout);

  for (i = 0; i < len && i < 10; i++) {
    writeByte(name[i], &checksum, fout);
  }

  for (i = len; i < 10; i++) {
    writeByte(' ', &checksum, fout);
  }

  writeDoubleByte(size, &checksum, fout);
  writeDoubleByte(0x8000, &checksum, fout);
  writeDoubleByte(size, &checksum, fout);
  writeByte(checksum, &checksum, fout);
  // the header ends here

  // writing data packet
  writeDoubleByte(size + 2, &checksum, fout);
  checksum = 0;
  writeByte(0xFF, &checksum, fout);

  // writing raw data
  for (i = 0; i < size; i++) {
    writeByte(buf[i], &checksum, fout);
  }

  writeByte(checksum, &checksum, fout);
}

void clearBuf(char *ibuf)
{
  memset(ibuf, 0, IN_BUFFER_SIZE);
}

int parseFile(FILE *fin, char *obuf, char *ibuf)
{
  // c - character read from the input file
  // p - position in obuf
  // b - position in ibuf
  // l - line reading position in the input file
  // x - column reading position in the input file
  // n - a number being read from the input file
  // r - remember where to put the last line length into the obuf output buffer
  int c, p = 0, b = 0, l = 1, x = 1, n, r = -1;
  // f - a floating point number being read from the input file
  float f;
  // s - reading state
  state s = LINE_START;

  clearBuf(ibuf);

  do {
    c = getc(fin);

    switch (s) {
      case LINE_START:
        if (isdigit(c)) {
          s = LINE_NUMBER;
          ibuf[b++] = c;
        } else if (';' == c) {
          s = SINGLE_LINE_COMMENT;
        } else if (!isspace(c) && (c != EOF)) {
          fputs(errSyntLineNum, stderr);
          fprintf(stderr, errPos, l, x);
          return -1;
        }
        break;

      case SINGLE_LINE_COMMENT:
        if (c == '\n') {
          s = LINE_START;
        }
        // do nothing at all..
        // stop chasing shadows,
        // just enjoy the ride!
        break;

      case LINE_NUMBER:
        if (isdigit(c)) {
          ibuf[b++] = c;
        } else if (isspace(c)) {
          sscanf(ibuf, "%d", &n);
          obuf[p++] = (n / 0x100) & 0xFF;
          obuf[p++] = n & 0xFF;
          r = p;
          p += 2;
          clearBuf(ibuf);
          b = 0;
          s = COMMAND_EXPECTED;
        } else {
          fputs(errSyntLineNum, stderr);
          fprintf(stderr, errPos, l, x);
          return -1;
        }
        break;

      case COMMAND_EXPECTED:
        if (isdigit(c)) {
          s = READING_NUMBER;
          ibuf[b++] = c;
          obuf[p++] = c;
        } else if (c == '.') {
          s = READING_NUMBER_DECIMAL;
          ibuf[b++] = c;
          obuf[p++] = c;
        } else if (isalpha(c)) {
          s = READING_COMMAND;
          ibuf[b++] = c;
        } else if (c == '"') {
          s = READING_STRING;
          obuf[p++] = c;
        } else if (c == '\n' || c == EOF) {
          if (r >= 0) {
            obuf[p++] = 0x0D;
            n = p - r - 2;
            obuf[r++] = n & 0xFF;
            obuf[r++] = (n / 0x100) & 0xFF;
            r = -1;
          }
          s = LINE_START;
        } else if (!isspace(c)) {
          obuf[p++] = c;
        }
        break;

      case READING_STRING:
        obuf[p++] = c;

        if (c == '"') {
          s = COMMAND_EXPECTED;
        }
        break;

      case READING_NUMBER:
        if (c == '.' || tolower(c) == 'e') {
          s = READING_NUMBER_DECIMAL;
          ibuf[b++] = c;
          obuf[p++] = c;
        } else if (isdigit(c)) {
          ibuf[b++] = c;
          obuf[p++] = c;
        } else {
          sscanf(ibuf, "%d", &n);
          obuf[p++] = 0x0E;
          obuf[p++] = 0;
          obuf[p++] = 0;
          obuf[p++] = n & 0xFF;
          obuf[p++] = (n / 0x100) & 0xFF;
          obuf[p++] = 0;

          if (c == EOF) {
            break;
          }

          clearBuf(ibuf);
          b = 0;

          ungetc(c, fin);
          s = COMMAND_EXPECTED;
          continue;
        }
        break;

      case READING_NUMBER_DECIMAL:
        if (isdigit(c) || tolower(c) == 'e') {
          ibuf[b++] = c;
          obuf[p++] = c;
        } else {
          sscanf(ibuf, "%f", &f);
          f = frexpf(f, &n);

          n += ZX_FLOAT_MANTISSA_BASE;

          if (f >= ZX_FLOAT_SIGN_FIX) {
            f -= ZX_FLOAT_SIGN_FIX;
          }

          obuf[p++] = 0x0E;
          obuf[p++] = n & 0xFF;

          for (int i = 0; i < 4; i++) {
            n = 0;

            for (int j = 0; j < 8; j++) {
              n <<= 1;
              f *= 2;

              if (f >= 1) {
                f -= 1;
                n |= 1;
              }
            }
            obuf[p++] = n & 0xFF;
          }

          if (c == EOF) {
            break;
          }

          clearBuf(ibuf);
          b = 0;

          ungetc(c, fin);
          s = COMMAND_EXPECTED;
          continue;
        }
      break;

      case READING_COMMAND:
        if (isalpha(c)) {
          ibuf[b++] = c;
        } else {
          int f = -1;

          for (int i = 0; i < COMMAND_LIST_SIZE; i++) {
            if (strcasecmp(ibuf, COMMAND_LIST[i]) == 0) {
              obuf[p++] = FIRST_COMMAND + i;
              f = i;
              break;
            }
          }

          if (f < 0) {
            for (int i = 0; i < b; i++) {
              obuf[p++] = ibuf[i];
            }
          }

          if (c == EOF) {
            break;
          }

          clearBuf(ibuf);
          b = 0;

          ungetc(c, fin);
          s = COMMAND_EXPECTED;
          continue;
        }
        break;

      default:
        fputs(errProg, stderr);
        return -1;
        break;
    }

    // line counting
    if ('\n' == c) {
      l++;
      x = 1;
    } else {
      x++;
    }
  } while (c != EOF);

  if (r >= 0) {
    obuf[p++] = 0x0D;
    n = p - r - 2;
    obuf[r++] = n & 0xFF;
    obuf[r++] = (n / 0x100) & 0xFF;
  }

  return p;
}

void process(char *name, FILE *fin, FILE *fout)
{
  int result;
  char *obuf = malloc(OUT_BUFFER_SIZE + 10), *ibuf;

  if (NULL == obuf) {
    fputs(errMem, stderr);
    return;
  }

  ibuf = malloc(IN_BUFFER_SIZE);

  if (NULL == ibuf) {
    free(obuf);
    fputs(errMem, stderr);
    return;
  }

  result = parseFile(fin, obuf, ibuf);

  free(ibuf);

  if (result >= 0) {
    writeFile(result, name, obuf, fout);
  }

  free(obuf);
}

int main(int ac, char **argv)
{
  FILE *fin, *fout;

  switch (ac) {
    case 2:
      process(argv[1], stdin, stdout);
      break;

    case 3:
      fin = fopen(argv[2], "r");

      if (NULL == fin) {
        fprintf(stderr, errIn, argv[2]);
        return 2;
      }

      process(argv[1], fin, stdout);
      fclose(fin);
      break;

    case 4:
      fin = fopen(argv[2], "r");

      if (NULL == fin) {
        fprintf(stderr, errIn, argv[2]);
        return 2;
      }

      fout = fopen(argv[3], "w");

      if (NULL == fout) {
        fprintf(stderr, errOut, argv[3]);
        return 3;
      }

      process(argv[1], fin, fout);
      fclose(fin);
      fclose(fout);
      break;

    default:
      printf("Use: bas2tap <name> [<input.bas> [<output.tap]]\n");
      return 1;
      break;
  }

  return 0;
}
