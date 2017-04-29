#ifndef SERIAL_H
#define SERIAL_H
/**********************************************************************/
#ifndef ARGS
#  ifdef LINT_ARGS
#     define ARGS(a) a
#  else
#     define ARGS(a) ()
#  endif
#endif
/**********************************************************************/
#ifndef uchar
#  define uchar unsigned char
#endif
#ifndef B300
#  include <termio.h>
#endif

#ifdef NCCS
#define TERMIO struct termios
#else
#define TERMIO struct termio
#endif

#define SERIALMODE struct serialmode
SERIALMODE {
   TERMIO t;
   int fflags;
};
/**********************************************************************/
int openserial    ARGS((char *device,int secs));
int reopenserial  ARGS((int fh,int secs));
int closeserial   ARGS((int ser));
int setserial     ARGS((int ser,int speed,int parity,int bits,int stopbits));
int setxserial    ARGS((int ser,int xonoff,int echo,int cbreak,int nodelay));
int setrawserial  ARGS((int ser,int raw));
int readserial    ARGS((int ser,uchar *buf,int cnt,int secs));
int writeserial   ARGS((int ser,uchar *buf,int cnt,int secs));
int hangupserial  ARGS((int ser,int secs));
int breakserial   ARGS((int ser,int n));
int restserial    ARGS((int ser,SERIALMODE *save));
/**********************************************************************/
#endif                                                    /* SERIAL_H */
