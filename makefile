# probably works not with all compilers. Thought this should be easy
# fixable. There is nothing special at this source.

rtcm3torinex: rtcm3torinex.c rtcm3torinex.h
	$(CC) -Wall -W -O3 -lm rtcm3torinex.c -o $@

archive:
	zip -9 rtcm3torinex.zip rtcm3torinex.c rtcm3torinex.h rtcm3torinex.txt makefile

clean:
	$(RM) rtcm3torinex rtcm3torinex.zip
