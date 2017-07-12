; test file
 8999 STOP 
 9000 LET cnt=0: LET en= PEEK 23635+256* PEEK 23636
 9010 LET en=en+4: LET cnt=cnt+4
 9020 LET tk= PEEK en: IF tk=226 THEN  LET cnt=cnt-4: GOTO 9060
 9030 IF tk=14 THEN  LET en=en+6: GOTO 9020
 9040 IF tk=13 THEN  LET en=en+1: LET cnt=cnt+1: GOTO 9010
 9050 LET en=en+1: LET cnt=cnt+1: GOTO 9020
 9060 PRINT #0; AT 1,0;"Length: ";cnt: PAUSE 0
