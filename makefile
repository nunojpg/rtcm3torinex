# probably works not with all compilers. Thought this should be easy
# fixable. There is nothing special at this source.

rtcm3torinex: lib/rtcm3torinex.c lib/rtcm3torinex.h
	$(CC) -Wall -W -O3 -Ilib lib/rtcm3torinex.c -lm -o $@

archive:
	zip -9 rtcm3torinex.zip lib/rtcm3torinex.c lib/rtcm3torinex.h rtcm3torinex.txt makefile

clean:
	$(RM) rtcm3torinex rtcm3torinex.zip