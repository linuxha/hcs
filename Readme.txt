# HCS
Rudementary HCS II host program for Linux and curses.
Do what you like with it. There's no copyright. Have fun.
Let me know if you find anything too arcane.

There are some hardcoded settings in hcs.c that reflect
my settings. Ones I can think of:
   default serial device (settable with -l)
      /dev/cua2
   location of events.bin
      /usr1/acct/mark/hcs/events.bin
   x10 house codes (see x10row array)
I don't know about y2k compliance. The HCS folks tell me the supervisor
is compliant, and I think I followed the spec...
I didn't have every device to test every output. I have pllink and mcir and dio.

The curses display has been funny for a couple linux releases now
(I use redhat). I don't know if it's curses or xterm, but I've seen
similar behavior from other curses programs. It works fine on the console.
And I'm confident that my curses usage is correct.
Under xterm, I set TERM=vt100 to make it display correctly.

hcs options:
   -l  - serial line device
   -s  - serial line speed
   -p  - pipe filename (an attempt to provide passthru to the HCS II
                        while hcs is running. don't remember if it worked!
                        the intent was to be able to tell HCS II to do stuff
                        from cron jobs and the like.)
   -i  - interpret only mode, no curses, can pipe to display prog like xhcs
   -d  - debug


hcs.c
  - hcs specific code.  you may find some "wtf" ("What To Fix") comments.
    they are potential problems or just stuff that was being considered.
serial.c
serial.h
  - generic serial port routines. initially written years ago, as you
    may guess from some of the ifdefs (iAPX286 for microport unix on 80286).
  - can be compiled standalone to be a basic terminal program.
makefile
  - guess
xhcs
  - crude X11 interface.  my first and last attempt at a Tk app. probably
    written very stupidly.

# Author
Mark (Sorry I don't have anything more)

# Copyright
None

# License
Public Domain (?)
