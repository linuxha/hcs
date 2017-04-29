hcs: hcs.c serial.o
	cc -g hcs.c serial.o -lcurses -o hcs

serial.o: serial.c serial.h
	cc -O -c serial.c

serial: serial.c serial.h
	cc -DTEST serial.c -o serial

# ncherry@linuxha.com 20170428
clean:
	rm -rf hcs serial.o serial *~ foo bar code
