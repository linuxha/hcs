#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <malloc.h>
#include "serial.h"

#ifdef iAPX286
#  define SIGTYPE int
#  define SIGRETURN return(0)
#else
#  define SIGTYPE void
#  define SIGRETURN return
#endif

#ifdef NCCS
#define NCCHR NCCS
#define GET TCGETS
#define SET TCSETS
static uchar sane_cc[NCCHR]={ 127, 3, 8, 255, 4, 10, 255, 255 };
#else
#define NCCHR NCC
#define GET TCGETA
#define SET TCSETA
static uchar sane_cc[NCCHR]={ 127, 3, 8, 255, 4, 10, 255, 255 };
#endif

/**********************************************************************/
static SIGTYPE alrmtrap(sig)
int sig;
{
   SIGRETURN;
}                                                   /* end alrmtrap() */
/**********************************************************************/

/**********************************************************************/
int openserial(device,secs)
char *device;
int secs; /* block till ready or secs expires (-1) means never expire */
{
int ser, fl=O_RDWR, osecs;
SIGTYPE (*ohandl)();

   if(secs==0) fl|=O_NDELAY;
   if(secs>0){
      ohandl=signal(SIGALRM,alrmtrap);
      osecs=alarm(secs);
   }
   ser=open(device,fl);
   if(secs>0){
      signal(SIGALRM,ohandl);
      if(osecs>secs && ser<0 && errno==EINTR) alarm(osecs-secs);
   }else if(secs==0 && ser>=0){
      if(fcntl(ser,F_SETFL,0)==(-1)){
         fl=errno;
         close(ser);
         ser=(-1);
         errno=fl;
      }
   }
   return(ser);
}                                                 /* end openserial() */
/**********************************************************************/

/**********************************************************************/
int reopenserial(fh,secs)
int fh;
int secs; /* block till ready or secs expires (-1) means never expire */
{
char *tty;
extern char *ttyname();

   if((tty=ttyname(fh))==(char *)NULL) return(-1);
   else  return(openserial(tty,secs));
}                                               /* end reopenserial() */
/**********************************************************************/

/**********************************************************************/
int closeserial(ser)
int ser;
{
   close(ser);
   return(0);
}                                                /* end closeserial() */
/**********************************************************************/

/**********************************************************************/
static int lookup(lst,val)
int lst[][2];
int val;
{
int i;

   for(i=0;lst[i][0]!=(-1);i++) if(lst[i][0]==val) return(lst[i][1]);
   return(-1);
}                                                     /* end lookup() */
/**********************************************************************/

static int lst_speed[][2]={
   { 0    ,B0     },
   { 110  ,B110   },
   { 300  ,B300   },
   { 600  ,B600   },
   { 1200 ,B1200  },
   { 2400 ,B2400  },
   { 4800 ,B4800  },
   { 9600 ,B9600  },
#ifdef B19200
   { 19200,B19200 },
#endif
#ifdef B38400
   { 38400,B38400 },
#endif
   { -1   ,-1     }
};
static int lst_bits[][2]={
   { 5    ,CS5    },
   { 6    ,CS6    },
   { 7    ,CS7    },
   { 8    ,CS8    },
   { -1   ,-1     }
};
/**********************************************************************/
int setserial(ser,speed,parity,bits,stopbits)
int ser;
int speed, parity, bits, stopbits;
{
TERMIO t;

   if(ioctl(ser,GET,&t)==(-1)) goto zerr;
   if(speed!=(-1)){
      if((speed=lookup(lst_speed,speed))==(-1)){
         errno=EINVAL;
         goto zerr;
      }
      t.c_cflag&=~CBAUD;                    /* turn off all baud bits */
      t.c_cflag|=speed;
   }
   switch(parity){
   case ' ': break;
   case 'e': case 'E':
      t.c_cflag|=PARENB;
      t.c_cflag&=~PARODD;
      t.c_iflag|=ISTRIP;
      break;
   case 'o': case 'O':
      t.c_cflag|=PARENB;
      t.c_cflag|=PARODD;
      t.c_iflag|=ISTRIP;
      break;
   case 'n': case 'N':
      t.c_cflag&=~PARENB;
      t.c_iflag&=~ISTRIP;
      break;
   default:
      errno=EINVAL;
      goto zerr;
   }
   if(bits!=(-1)){
      if((bits=lookup(lst_bits,bits))==(-1)){
         errno=EINVAL;
         goto zerr;
      }
      t.c_cflag&=~CSIZE;                    /* turn off all size bits */
      t.c_cflag|=bits;
   }
   switch(stopbits){
   case -1: break;
   case 1: t.c_cflag&=~CSTOPB; break;
   case 2: t.c_cflag|=CSTOPB;  break;
   default:
      errno=EINVAL;
      goto zerr;
   }
   if(ioctl(ser,SET,&t)==(-1)) goto zerr;
   return(0);
zerr:
   return(-1);
}                                                  /* end setserial() */
/**********************************************************************/

/**********************************************************************/
int setxserial(ser,xonoff,echo,cbreak,nodelay)
int ser;
int xonoff, echo, cbreak, nodelay;
{
TERMIO t;

   if(ioctl(ser,GET,&t)==(-1)) goto zerr;
   if(xonoff!=(-1)){
      if(xonoff) t.c_iflag|=(IXOFF|IXON);
      else       t.c_iflag&=~(IXOFF|IXON);
   }
   if(echo!=(-1)){
      if(echo) t.c_lflag|=(ECHO|ECHONL);
      else     t.c_lflag&=~(ECHO|ECHONL);
   }
   if(cbreak!=(-1)){
      if(cbreak){
         t.c_lflag&=~ICANON;
         t.c_cc[VEOF]=1;
         t.c_cc[VEOL]=0;
      }else{
         t.c_lflag|=ICANON;
         t.c_cc[VEOF]=4;
         t.c_cc[VEOL]=10;
      }
   }
   if(nodelay!=(-1)){
      if(nodelay){
         if(fcntl(ser,F_SETFL,O_NDELAY)==(-1)) goto zerr;
      }else{
         if(fcntl(ser,F_SETFL,0)==(-1)) goto zerr;
      }
   }
   if(ioctl(ser,SET,&t)==(-1)) goto zerr;
   return(0);
zerr:
   return(-1);
}                                                 /* end setxserial() */
/**********************************************************************/

/**********************************************************************/
int setrawserial(ser,raw)
int ser;
int raw;
{
TERMIO t;

   if(ioctl(ser,GET,&t)==(-1)) goto zerr;
   if(raw){
      memset(t.c_cc,0,NCCHR);
      t.c_iflag=t.c_oflag=t.c_lflag=0;
   }else{
      memcpy(t.c_cc,sane_cc,NCCHR);
      t.c_oflag|=OPOST|ONLCR;
      t.c_iflag&=~(IGNBRK|IGNPAR);
      t.c_iflag|=(PARMRK|INPCK|INLCR|IGNCR|ICRNL);
      t.c_lflag|=(ISIG|ICANON|ECHOE|ECHOK);
   }
   if(ioctl(ser,SET,&t)==(-1)) goto zerr;
   return(0);
zerr:
   return(-1);
}                                                 /* end setrserial() */
/**********************************************************************/

/**********************************************************************/
int readserial(ser,buf,cnt,secs)
int ser;
uchar *buf;
int cnt;
int secs; /* block till ready or secs expires (-1) means never expire */
{
int nread, osecs, se;
SIGTYPE (*ohandl)();

   if(secs>0){
      ohandl=signal(SIGALRM,alrmtrap);
      osecs=alarm(secs);
   }
   nread=read(ser,buf,cnt);
   if(secs>0){
      se=errno;
      signal(SIGALRM,ohandl);
      if(osecs>secs && nread<0 && se==EINTR) alarm(osecs-secs);
      errno=se;
   }
   return(nread);
}                                                 /* end readserial() */
/**********************************************************************/

/**********************************************************************/
int writeserial(ser,buf,cnt,secs)
int ser;
uchar *buf;
int cnt;
int secs; /* block till ready or secs expires (-1) means never expire */
{
int nwrt, osecs, se;
SIGTYPE (*ohandl)();

   if(secs>0){
      ohandl=signal(SIGALRM,alrmtrap);
      osecs=alarm(secs);
   }
   nwrt=write(ser,buf,cnt);
   if(secs>0){
      se=errno;
      signal(SIGALRM,ohandl);
      if(osecs>secs && nwrt<0 && se==EINTR) alarm(osecs-secs);
      errno=se;
   }
   return(nwrt);
}                                                /* end writeserial() */
/**********************************************************************/

/**********************************************************************/
int hangupserial(ser,secs)
int ser;
int secs;                                      /* time to hold hangup */
{
TERMIO t;
int ocflag;

   if(ioctl(ser,GET,&t)==(-1)) goto zerr;
   ocflag=t.c_cflag;
   t.c_cflag&=~CBAUD;                       /* turn off all baud bits */
   t.c_cflag|=B0;
   if(ioctl(ser,SET,&t)==(-1)) goto zerr;
   sleep(secs);
   t.c_cflag=ocflag;
   if(ioctl(ser,SET,&t)==(-1)) goto zerr;
   return(0);
zerr:
   return(-1);
}                                               /* end hangupserial() */
/**********************************************************************/

/**********************************************************************/
int breakserial(ser,n)
int ser, n;
{
   for(;n>0;n--) if(ioctl(ser,TCSBRK,0)==(-1)) return(-1);
   return(0);
}                                                /* end breakserial() */
/**********************************************************************/

/**********************************************************************/
int saveserial(ser,save)
int ser;
SERIALMODE *save;
{
   if(ioctl(ser,GET,&save->t)==(-1) ||
      (save->fflags=fcntl(ser,F_GETFL))==(-1)){
      return(-1);
   }
   return(0);
}                                                 /* end saveserial() */
/**********************************************************************/

/**********************************************************************/
int restserial(ser,save)
int ser;
SERIALMODE *save;
{
   if(ioctl(ser,SET,&save->t)==(-1) ||
      fcntl(ser,F_SETFL,save->fflags)==(-1)){
      return(-1);
   }
   return(0);
}                                                 /* end restserial() */
/**********************************************************************/

#ifdef TEST

#ifndef CPNULL
#  define CPNULL (char *)NULL
#endif
#ifndef FPNULL
#  define FPNULL (FILE *)NULL
#endif

int done=0, debug=0, active=1, reaper=0;
FILE *capt=FPNULL;
#define HBSZ 8192
int hbufsz=0;
uchar hbuf[HBSZ], *hbufp=hbuf;
SERIALMODE save;
/**********************************************************************/
SIGTYPE trap1(sig)
int sig;
{
   done=sig;
   SIGRETURN;
}                                                      /* end trap1() */
/**********************************************************************/

/**********************************************************************/
SIGTYPE trap2(sig)
int sig;
{
   signal(sig,trap2);
   if(active) active=0;
   else       active=1;
   SIGRETURN;
}                                                      /* end trap2() */
/**********************************************************************/

/**********************************************************************/
SIGTYPE funeral(sig)
int sig;
{
   reaper++;
   SIGRETURN;
}                                                    /* end funeral() */
/**********************************************************************/

/**********************************************************************/
void remoteecho(ser,loc,parent)
int ser, loc, parent;
{
uchar c, buf[5];
int rc;

   signal(SIGUSR2,trap2);
   while(!done){
      if(active && hbufsz>0){
         c= *hbufp;
         if(--hbufsz==0) hbufp=hbuf;
         else            hbufp++;
      }else{
         if((rc=readserial(ser,&c,1,-1))!=1){
            if(debug) printf("readserial(%d,,1,-1)==%d (errno=%d)\n",
                             ser,rc,errno);
            if(errno==EINTR) continue;
            else             break;
         }
      }
      if(active){
         if(capt!=FPNULL) fwrite(&c,1,1,capt);
         if(debug && (c<32 || c>126)){
            sprintf(buf,"[%02x]",(int)c&0xff);
            if((rc=writeserial(loc,buf,4,-1))!=4){
               if(debug) printf("writeserial(%d,,4,-1)==%d (errno=%d)\n",
                                loc,rc,errno);
               break;
            }
            if(c=='\n' && (rc=writeserial(loc,&c,1,-1))!=1){
               if(debug) printf("writeserial(%d,,1,-1)==%d (errno=%d)\n",
                                loc,rc,errno);
               break;
            }
         }else{
            if((rc=writeserial(loc,&c,1,-1))!=1){
               if(debug) printf("writeserial(%d,,1,-1)==%d (errno=%d)\n",
                                loc,rc,errno);
               break;
            }
         }
      }else{
         if(hbufsz<HBSZ) hbuf[hbufsz++]=c;
      }
   }
   if(!done){
      fprintf(stderr,"\n\r--- Background errno=%d ---\n\r",errno);
      kill(parent,SIGUSR1);
   }
   signal(SIGUSR2,SIG_DFL);
   exit(0);
}                                                 /* end remoteecho() */
/**********************************************************************/

/**********************************************************************/
void localin(ser,loc,child)
int ser, loc, child;
{
SERIALMODE cur;
int childrc, rc;
char c;

   while(!done){
      if((rc=read(loc,&c,1))<0/*!=1 wtf */){
         if(debug) printf("read(%d,1)==%d (errno=%d)\n",
                          loc,rc,errno);
         break;
      }
      if(c!=28){
         if((rc=writeserial(ser,&c,1,-1))!=1){
            if(debug) printf("writeserial(%d,,1,-1)==%d (errno=%d)\n",
                             ser,rc,errno);
            break;
         }
      }else do{                                                 /* ^\ */
         if(read(loc,&c,1)!=1) break;
         switch(c){
         case 28:
            if(writeserial(ser,&c,1,-1)!=1){
               if(debug) printf("writeserial(%d,,1,-1)==%d (errno=%d)\n",
                                ser,rc,errno);
               done=1;
            }
            break;
         case 'c':
         case 'C':
         case 3  :
            fputs("\n\rClosing terminal\n\r",stdout);
            done=1;
            kill(child,SIGUSR1);
            while((rc=wait(&childrc))!=child && rc!=(-1)) ;
            if(rc==(-1)) printf("wait=%d, errno=%d\n\r",rc,errno);
            else if(childrc!=0) printf("childrc=0x%04x\n\r",childrc);
            break;
         case 'h':
         case 'H':
         case 8  :
            if(hangupserial(ser,1)!=0) perror("\n\rhangup");
            break;
         case 'b':
         case 'B':
         case 2  :
            if(breakserial(ser,1)!=0) perror("\n\rbreak");
            break;
         case '!':
         case 'p':
         case 'P':
         case 16:
            kill(child,SIGUSR2);
            saveserial(loc,&cur);
            restserial(loc,&save);
            puts("\nLocal shell");
            system("${SHELL:-/bin/sh}");
            puts("\nReturning to remote connect");
            restserial(loc,&cur);
            kill(child,SIGUSR2);
            break;
         case '?':
            fputs("\n\r",stdout);
            fputs("^\\ again to send it\n\r",stdout);
            fputs("c to close terminal\n\r",stdout);
            fputs("h to hangup remote\n\r",stdout);
            fputs("b to send break to remote\n\r",stdout);
            fputs("p or ! to push to shell\n\r",stdout);
            fputs("==> ",stdout);
            break;
         }
      }while(c=='?');
   }
   if(!done){
      if(reaper) fprintf(stderr,"\n\rChild died");
      fprintf(stderr,"\n\r--- Foreground errno=%d ---\n\r",errno);
      kill(child,SIGUSR1);
   }
}                                                    /* end localin() */
/**********************************************************************/

/**********************************************************************/
void localpass(ser,loc,child,pass)
int ser, loc, child;
char *pass;
{
SERIALMODE cur;
int childrc, rc, bsz;
char c, *buf;

   bsz=strlen(pass);
   buf=malloc(bsz);
   *buf='\0';
   while(!done && read(loc,&c,1)==1){
   int j;
      if(writeserial(ser,&c,1,-1)!=1) break;
      for(j=0;j<bsz-1;j++) buf[j]=buf[j+1];
      buf[j]=c;
      if(strncmp(buf,pass,bsz)==0){
         done=1;
         kill(child,SIGUSR1);
         while((rc=wait(&childrc))!=child && rc!=(-1)) ;
         if(rc==(-1)) printf("wait=%d, errno=%d\n\r",rc,errno);
         else if(childrc!=0) printf("childrc=0x%04x\n\r",childrc);
         break;
      }
   }
   if(!done){
      if(reaper) fprintf(stderr,"\n\rChild died");
      fprintf(stderr,"\n\r--- Foreground errno=%d ---\n\r",errno);
      kill(child,SIGUSR1);
   }
}                                                  /* end localpass() */
/**********************************************************************/

/**********************************************************************/
void terminal(ser,loc,pass)
int ser, loc;
char *pass;
{
int child;

   signal(SIGUSR1,trap1);
   signal(SIGCLD,funeral);
   switch(child=fork()){                                     /* child */
   case (-1):                                                /* error */
      fputs("Can't fork\n\r",stderr);
      break;
   case 0:                             /* child - handle remote input */
      remoteecho(ser,loc,getppid());
      break;
   default:                            /* parent - handle local input */
      if(pass==CPNULL) localin(ser,loc,child);
      else             localpass(ser,loc,child,pass);
      break;
   }
   signal(SIGUSR1,SIG_DFL);
}                                                   /* end terminal() */
/**********************************************************************/

/**********************************************************************/
void
trap(sig)
int sig;
{
   signal(sig,trap);
}                                                       /* end trap() */
/**********************************************************************/

static char *Prog="serial";
/**********************************************************************/
#define P fprintf(stderr,
void use()
{
   P"Use: %s [options] line speed\n",Prog);
   P"     -l line\n");
   P"     -s speed\n");
   P"     -b bits\n");
   P"     -p parity\n");
   P"     -x  (enable xon/xoff)\n");
   P"     -c file  - capture to file\n");
   P"     -r[stopstring]\n");
   P"     -d turn on debug (you brave soul)\n");
   P"     -- force end of options\n");
   exit(1);
}
#undef P
/**********************************************************************/

#define NEXTARG()   (*++*argv=='\0'?++argv,--argc:1)
#define NEXTINT(a)  ((NEXTARG() && isdigit(**argv))?((void)((a)=atoi(*argv))):use())
#define NEXTLONG(a) ((NEXTARG() && isdigit(**argv))?((void)((a)=atol(*argv))):use())
#define NEXTCP(a)   (NEXTARG()?((void)((a)= *argv)):use())
#define NEXTCHAR(a) (NEXTARG()?((void)((a)= **argv)):use())
/**********************************************************************/
main(argc,argv)
int argc;
char **argv;
{
char *line="/dev/cua1", *capture=CPNULL;
char *pass=CPNULL;
int ser, loc, inopts, inbundle, restore=0;
int speed=19200, parity='n', bits=8, stopbits=1;
int raw=1, xonoff=0, echo=0, cbreak=1, nodelay=0;

   Prog=argv[0];
   for(argc--,argv++,inopts=1;inopts && argc>0 && **argv=='-';argc--,argv++){
      for(++*argv,inbundle=1;inbundle && **argv!='\0';++*argv){
         switch(**argv){
         case 'l': NEXTCP(line); inbundle=0; break;
         case 's': NEXTINT(speed); inbundle=0; break;
         case 'b': NEXTINT(bits); inbundle=0; break;
         case 'x': xonoff=1; break;
         case 'p': NEXTCHAR(parity); inbundle=0; break;
         case 'c': NEXTCP(capture); inbundle=0; break;
         case 'r': if(*++*argv=='\0') pass=".stoppipe\.";
                   else               pass= *argv;
                   inbundle=0;
                   break;
         case 'd': debug++; break;
         case '-': inopts=0; break;
         default: fprintf(stderr,"Bad option '%c'\n",**argv); use(); break;
         }
      }
   }
   if(argc>0){
      line= *argv;
      if(argc>1) speed=atoi(*++argv);
   }
   signal(SIGINT,trap);
   signal(SIGQUIT,trap);
   if(capture!=CPNULL){
      if((capt=fopen(capture,"wb"))==FPNULL) perror(capture);
   }
                                                         /* local tty */
   if((loc=reopenserial(0,(-1)))==(-1)) goto zerr;
   if(saveserial(loc,&save)==(-1)) goto zerr;
   restore=1;
   if(raw && setserial(loc,-1,parity,bits,stopbits)!=0) goto zerr;
   if(setrawserial(loc,raw)!=0) goto zerr;
   if(setxserial(loc,xonoff,echo,cbreak,nodelay)!=0) goto zerr;
                                                        /* remote tty */
   printf("%s: ",line);
   if((ser=openserial(line,(-1)))==(-1)) goto zerr;
   printf("%d,%c,%d,%d: ",speed,parity,bits,stopbits);
   if(setserial(ser,speed,parity,bits,stopbits)!=0) goto zerr;
   if(debug) printf("r:%d,x:%d,e:%d,c:%d,n:%d: ",raw,xonoff,echo,cbreak,nodelay);
   if(setrawserial(ser,raw)!=0) goto zerr;
   if(setxserial(ser,xonoff,echo,cbreak,nodelay)!=0) goto zerr;

   fputs("\n\rconnected\n\r",stdout);
   if(pass!=CPNULL) printf("quit with \"%s\"\n\r",pass);
   else             printf("quit with ^\\c\n\r");
   terminal(ser,loc,pass);
   fputs("\n\rdisconnecting",stdout);
   if(restserial(loc,&save)==(-1)) goto zerr;
   closeserial(loc);
   closeserial(ser);
   puts("\rdisconnected ");
   if(capt!=FPNULL) fclose(capt);
   return(0);
zerr:
   if(restore){              /* MAW 08-31-92 - put tty back to normal */
   int se=errno;
      restserial(loc,&save);
      errno=se;
   }
   perror("");
   if(capt!=FPNULL) fclose(capt);
   return(1);
}
/**********************************************************************/
#endif                                                        /* TEST */
