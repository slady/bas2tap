#ifndef __BUFS_H
#define __BUFS_H

int initBufs(FILE *fin);

void freeBufs(void);

void clearIBuf(void);

char *getIBuf();

char *getOBuf();

int getIC();

void ungetC();

void addOutC(void);

void addOutCh(int ch);

void addInC(void);

void copyInToOut(void);

void markLineLength(void);

void writeLineLength(void);

int getLength(void);

int iBufIs(const char *tstStr);

#endif
