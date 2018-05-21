#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "bufs.h"

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
  "LPRINT", "LLIST", "STOP", "READ", "DATA", "RESTORE", "NEW", "BORDER", "CONTINUE", "DIM", "REM", "FOR", "GO TO", "GO SUB", "INPUT", "LOAD",
  "LIST", "LET", "PAUSE", "NEXT", "POKE", "PRINT", "PLOT", "RUN", "SAVE", "RANDOMIZE", "IF", "CLS", "DRAW", "CLEAR", "RETURN", "COPY" };
// changed key words:
// "DEF FN", "OPEN #", "CLOSE #", "GO TO", "GO SUB"
const int COMMAND_LIST_SIZE = (sizeof(COMMAND_LIST) / sizeof(COMMAND_LIST[0]));

const int ZX_FLOAT_MANTISSA_BASE = 128;
const float ZX_FLOAT_SIGN_FIX = .5;

const int COMMAND_CODE_BIN = 196;
const int COMMAND_CODE_LE = 199;
const int COMMAND_CODE_GE = 200;
const int COMMAND_CODE_NE = 201;

typedef enum { FIRST_LINE_START, NEXT_LINE_START, LINE_NUMBER, COMMAND_EXPECTED,
    READING_STRING, READING_COMMAND, READING_NUMBER, READING_NUMBER_DECIMAL,
    SYMBOL_PREFIX, COMMAND_PREFIX, SINGLE_LINE_COMMENT } state;

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

int parseFile(void)
{
  // c - character read from the input file
  // line - line reading position in the input file
  // column - column reading position in the input file
  // number - a number being read from the input file
  int c, line = 1, column = 1, number, isBinNumber = 0;
  // f - a floating point number being read from the input file
  float f;
  // s - reading state
  state s = FIRST_LINE_START;

  clearIBuf();

  do {
    c = getIC();

    switch (s) {
      case FIRST_LINE_START:
        if (isdigit(c)) {
          s = LINE_NUMBER;
          addInC();
        } else if (';' == c) {
          s = SINGLE_LINE_COMMENT;
        } else if (!isspace(c) && (c != EOF)) {
          fputs(errSyntLineNum, stderr);
          fprintf(stderr, errPos, line, column);
          return -1;
        }
        break;

      case NEXT_LINE_START:
        if (isdigit(c) || c == ';') {
          writeLineLength();

          if (isdigit(c)) {
            s = LINE_NUMBER;
            addInC();
          } else if (';' == c) {
            s = SINGLE_LINE_COMMENT;
          }
        } else if (isalpha(c)) {
          s = READING_COMMAND;
          addInC();
        } else if (c == '"') {
          s = READING_STRING;
          addOutC();
        } else if (!isspace(c) && (c != EOF)) {
          fputs(errSyntLineNum, stderr);
          fprintf(stderr, errPos, line, column);
          return -1;
        }
        break;

      case SINGLE_LINE_COMMENT:
        if (c == '\n') {
          s = FIRST_LINE_START;
        }
        // do nothing at all..
        // stop chasing shadows,
        // just enjoy the ride!
        break;

      case LINE_NUMBER:
        if (isdigit(c)) {
          addInC();
        } else if (isspace(c)) {
          sscanf(getIBuf(), "%d", &number);
          addOutCh((number / 0x100) & 0xFF);
          addOutCh(number & 0xFF);
          markLineLength();
          clearIBuf();
          s = COMMAND_EXPECTED;
        } else {
          fputs(errSyntLineNum, stderr);
          fprintf(stderr, errPos, line, column);
          return -1;
        }
        break;

      case COMMAND_EXPECTED:
        if (isdigit(c)) {
          s = READING_NUMBER;
          addInC();
          addOutC();
        } else if (c == '.') {
          s = READING_NUMBER_DECIMAL;
          addInC();
          addOutC();
        } else if (isalpha(c)) {
          s = READING_COMMAND;
          addInC();
        } else if (c == '"') {
          s = READING_STRING;
          addOutC();
        } else if (c == '<' || c == '>') {
          s = SYMBOL_PREFIX;
          addInC();
        } else if (c == '\n' || c == EOF) {
          s = NEXT_LINE_START;
        } else if (!isspace(c)) {
          addOutC();
        }
        break;

      case READING_STRING:
        addOutC();

        if (c == '"') {
          s = COMMAND_EXPECTED;
        }
        break;

      case SYMBOL_PREFIX:
        {
          int f = -1;
          char *ibuf = getIBuf();

          if (ibuf[0] == '<' && c == '=') {
            f = COMMAND_CODE_LE;
          } else if (ibuf[0] == '>' && c == '=') {
            f = COMMAND_CODE_GE;
          } else if (ibuf[0] == '<' && c == '>') {
            f = COMMAND_CODE_NE;
          }

          if (f < 0) {
            copyInToOut();
            ungetC();
          } else {
            addOutCh(f);
          }

          s = COMMAND_EXPECTED;

          clearIBuf();
        }

        break;

      case READING_NUMBER:
        if (c == '.' || tolower(c) == 'e') {
          s = READING_NUMBER_DECIMAL;
          addInC();
          addOutC();
        } else if (isdigit(c)) {
          addInC();
          addOutC();
        } else {
          if (isBinNumber) {
            number = (int) strtol(getIBuf(), NULL, 2);
            isBinNumber = 0;
          } else {
            sscanf(getIBuf(), "%d", &number);
          }

          addOutCh(0x0E);
          addOutCh(0);
          addOutCh(0);
          addOutCh(number & 0xFF);
          addOutCh((number / 0x100) & 0xFF);
          addOutCh(0);

          if (c == EOF) {
            break;
          }

          clearIBuf();

          ungetC();
          s = COMMAND_EXPECTED;
          continue;
        }
        break;

      case READING_NUMBER_DECIMAL:
        if (isdigit(c) || tolower(c) == 'e') {
          addInC();
          addOutC();
        } else {
          sscanf(getIBuf(), "%f", &f);
          f = frexpf(f, &number);

          number += ZX_FLOAT_MANTISSA_BASE;

          if (f >= ZX_FLOAT_SIGN_FIX) {
            f -= ZX_FLOAT_SIGN_FIX;
          }

          addOutCh(0x0E);
          addOutCh(number & 0xFF);

          for (int i = 0; i < 4; i++) {
            number = 0;

            for (int j = 0; j < 8; j++) {
              number <<= 1;
              f *= 2;

              if (f >= 1) {
                f -= 1;
                number |= 1;
              }
            }
            addOutCh(number & 0xFF);
          }

          if (c == EOF) {
            break;
          }

          clearIBuf();

          ungetC();
          s = COMMAND_EXPECTED;
          continue;
        }
      break;

      case READING_COMMAND:
        if (isalpha(c) || (c == ' ' && strcasecmp(getIBuf(), "GO") == 0)) {
          addInC();
        } else {
          int f = -1;

          for (int i = 0; i < COMMAND_LIST_SIZE; i++) {
            if (strcasecmp(getIBuf(), COMMAND_LIST[i]) == 0) {
              f = FIRST_COMMAND + i;
              addOutCh(f);
              break;
            }
          }

          if (f < 0) {
            copyInToOut();
          }

          if (c == EOF) {
            break;
          }

          clearIBuf();

          ungetC();

          if (f == COMMAND_CODE_BIN) {
            isBinNumber = 1;
          }

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
      line++;
      column = 1;
    } else {
      column++;
    }
  } while (c != EOF);

  writeLineLength();

  return getLength();
}

void process(char *name, FILE *fin, FILE *fout)
{
  int result;

  if (!initBufs(fin)) {
    fputs(errMem, stderr);
    return;
  }

  result = parseFile();

  if (result >= 0) {
    writeFile(result, name, getOBuf(), fout);
  }

  freeBufs();
}

int main(int argc, char **argv)
{
  FILE *fin, *fout;

  switch (argc) {
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
