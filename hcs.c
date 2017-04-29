#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

//#include <sys/time.h>
#include <time.h>		// Modern Linux

#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <curses.h>
#include "serial.h"

#define word ushort
#ifndef CHARPN
#  define CHARPN (char *)NULL
#endif
#ifndef FILEPN
#  define FILEPN (FILE *)NULL
#endif

int done=0, debug=0, interp=0, loadmode=0;
SERIALMODE save;
WINDOW *statwin;
WINDOW *infowin;
#define TITLEROW    0
#define TIMEFMT 2
#if TIMEFMT==1
#define TIMEWIDTH   28
#define TIMECOL     (COLS-TIMEWIDTH)
#elif TIMEFMT==2
#define TIMEWIDTH   34
#define TIMECOL     (COLS-TIMEWIDTH-1)
#endif
#define TIMEROW     0
#define DIGINROW    1
#define DIGINCOLP   0
#define DIGINCOL(a) (11+(((a)%3)*12))
#define DIGOUTROW   2
#define DIGOUTCOLP  0
#define DIGOUTCOL(a) (11+(((a)%3)*12))
#define X10ROW      1
#define X10COL     48
#define ANINROW     3
#define ANINCOL     0
#define NETROW      4
#define NETCOL      0

#define HCSIN struct hcsin_struct
HCSIN {
   struct {
      uchar hsec, sec, min;
      uchar hr, dow, day, mon, yr;
   } daytime;
   struct {
      uchar lo, hi;
   } x10[16];
   struct {
      uchar modnum:4;
      uchar modtyp:4;
      uchar stat;
   } net[17];
   struct {
      uchar stat;
   } din[32];
   struct {
      uchar stat;
   } dout[32];
   struct {
      word chan[16];
   } ain[3];
} hcsin;

/**********************************************************************/
void
attr(WINDOW *w,int at)
{
   if(!interp)
      wattrset(w,at);
}
/**********************************************************************/

/**********************************************************************/
void
fresh(WINDOW *w)
{
   if(interp) fflush(stdout);
   else       wrefresh(w);
}
/**********************************************************************/

/**********************************************************************/
void
eol(WINDOW *w)
{
   if(interp) putchar('\n');
/*
   else       wclrtoeol(w);
*/
}
/**********************************************************************/

/**********************************************************************/
void
addc(WINDOW *w,int c)
{
   if(interp) putchar(c);
   else       waddch(w,c);
}
/**********************************************************************/

/**********************************************************************/
void
print(WINDOW *w,char *fmt,...)
{
va_list args;

   va_start(args,fmt);
   if(interp) vprintf(fmt,args);
   else       vwprintw(w,fmt,args);
   va_end(args);
}
/**********************************************************************/

/**********************************************************************/
void
printat(WINDOW *w,int row,int col,char *fmt,...)
{
va_list args;

   va_start(args,fmt);
   if(interp) vprintf(fmt,args);
   else{
      wmove(w,row,col);
      vwprintw(w,fmt,args);
   }
   va_end(args);
}
/**********************************************************************/

/**********************************************************************/
logit(v)
uint v;
{
static FILE *logfp=FILEPN;
static int try=1;
time_t now;
static uint pv=(-32000);

   if(try)
   {
      if(logfp==FILEPN && (logfp=fopen("hcs.log","a"))==FILEPN)
      {
         try=0;
      }
      else
      if(abs(pv-v)>1)
      {
         time(&now);
         fprintf(logfp,"%.8s %u\n",ctime(&now)+11,v);
         fflush(logfp);
         pv=v;
      }
   }
}
/**********************************************************************/

/**********************************************************************/
void
waitread(fd)
int fd;
{
fd_set rd, wr, ex;

   FD_ZERO(&rd);
   FD_ZERO(&wr);
   FD_ZERO(&ex);
   FD_SET(fd,&rd);
   FD_SET(fd,&ex);
   select(fd+1,&rd,&wr,&ex,(struct timeval *)NULL);
}
/**********************************************************************/

/**********************************************************************/
readexact(fd,buf,n)
int fd;
uchar *buf;
int n;
{
int tnr, nr;

   for(tnr=0;tnr<n;tnr+=nr)
   {
      waitread(fd);
      if((nr=readserial(fd,buf+tnr,n-tnr,-1))<0)
      {
         print(infowin,"error: readserial(%d,,%d,-1)==%d (errno=%d)\n",
                       fd,n-tnr,nr,errno);
         return(-1);
      }
   }
   return(tnr);
}
/**********************************************************************/

/**********************************************************************/
writeexact(fd,buf,n)
int fd;
uchar *buf;
int n;
{
int tnr, nr;

   for(tnr=0;tnr<n;tnr+=nr)
   {
      if((nr=writeserial(fd,buf+tnr,n-tnr,-1))<0)
      {
         print(infowin,"error: writeserial(%d,,%d,-1)==%d (errno=%d)\n",
                       fd,n-tnr,nr,errno);
         return(-1);
      }
   }
   return(tnr);
}
/**********************************************************************/

/**********************************************************************/
uint
bcd2b(v)
uint v;
{
   return(((v>>4)*10)+(v&0xf));
}
/**********************************************************************/

/**********************************************************************/
uint
b2bcd(v)
uint v;
{
   return(((v/10)<<4)|(v%10));
}
/**********************************************************************/

/**********************************************************************/
printlbin(w,c)
WINDOW *w;
int c;
{
int i;
   for(i=0;i<8;i++,c>>=1)
      addc(w,c&1?'1':'0');
}
/**********************************************************************/

/**********************************************************************/
htype(p,n)
uchar *p;
int n;
{
uchar *e;

   for(e=p+n;p<e;p++)
   {
      if(interp)
         printf((((*p)<32||(*p)>126)?"[%02X]":"%c"),(*p));
      else
         print(infowin,(((*p)<32||(*p)>126)?"[%02X]":"%c"),(*p));
   }
}
/**********************************************************************/

/**********************************************************************/
loadprog(ser,fn)
int ser;
char *fn;
{
FILE *fp;
int rc=0, nr;
char buf[BUFSIZ];

   if((fp=fopen(fn,"r"))==(FILE *)NULL)
   {
      attr(infowin,A_NORMAL);
      print(infowin,"error: Can't read to %s: %s\n",fn,strerror(errno));
   }
   else
   {
      while((nr=fread(buf,1,BUFSIZ,fp))>0)
         writeserial(ser,buf,nr,-1);
      rc=1;
      fclose(fp);
   }
   return(rc);
}                                                   /* end loadprog() */
/**********************************************************************/

/**********************************************************************/
savelog(ser,fn)
int ser;
char *fn;
{
FILE *fp;
int rc=0, nr, nff;
unsigned char buf[1];

   if((fp=fopen(fn,"w"))==(FILE *)NULL)
   {
      attr(infowin,A_NORMAL);
      print(infowin,"error: Can't write to %s: %s\n",fn,strerror(errno));
   }
   else
   {
      nff=0;
      while((nr=readserial(ser,buf,1,-1))>=0)
      {
         if(nr==0) continue;
         fwrite(buf,1,nr,fp);
         if(buf[0]==0xff)
         { 
            nff++;
            if(nff==3)
               break;
         }
         else
            nff=0;
      }
      rc=1;
      fclose(fp);
   }
   return(rc);
}                                                    /* end savelog() */
/**********************************************************************/

/**********************************************************************/
void
titleme()
{
int x, y;

   attr(statwin,A_REVERSE);
   print(statwin," HCS II Monitor - Press ? for help ");
   getyx(statwin,y,x);
   for(;x<COLS;x++) addc(statwin,' ');
   attr(statwin,A_NORMAL);
}
/**********************************************************************/

/**********************************************************************/
void remoteecho(ser,loc)
int ser, loc;
{
#define INBSZ 64
uchar c, buf[INBSZ];
word  an[16];
int rc, nc, i, j;
#if TIMEFMT==1
static char *days[]={"","Sun","Mon","Tue","Wed","Thu","Fri","Sat","Sun","Unk"};
#elif TIMEFMT==2
static char *days[]={"","Sunday","Monday","Tuesday","Wednesday",
                        "Thursday","Friday","Saturday","Sunday","Unknown"};
#endif
#define NDAYS (sizeof(days)/sizeof(days[0]))
static char *netm[]={"PL","MC/IR","LCD","DIO","ADIO","DIO+","Unknown6","Unknown7"};
#define NNETM (sizeof(netm)/sizeof(netm[0]))
static char *nets[]={"timeout","ok","error","unknown"};
#define NNETS (sizeof(nets)/sizeof(nets[0]))
/*                   a b c d e f g h i j k l m n o p */
static int x10row[]={0,1,2,3,4,0,0,0,0,0,0,0,0,0,5,6,8};
#define NX10ROW (sizeof(x10row)/sizeof(x10row[0]))
static int unk=0;

   if(!interp)
      attr(infowin,A_NORMAL);
   if((rc=readexact(ser,&c,1))!=1) return;
   if(loadmode==1 && c=='^')
   {
      loadmode=0;
      if((rc=loadprog(ser,"/usr1/acct/mark/hcs/events.bin"))<1) return;
      if((rc=readexact(ser,buf,1))!=1) return;
      switch(buf[0])
      {
       case '^': print(infowin,"Program Loaded\n"); break;
       case '#': print(infowin,"Wrong version firmware/program - HALTED\n"); break;
       case '@': print(infowin,"Program too big - HALTED\n"); break;
      }
      return;
   }
   if(c!='$') return;
   if((rc=readexact(ser,&c,1))!=1 || !(c&0x80)) return;
   switch(c){
   case 0x80:                                            /* date/time */
      if(unk) print(infowin,"\n");
      unk=0;
      if((rc=readexact(ser,&hcsin.daytime,sizeof(hcsin.daytime)))!=sizeof(hcsin.daytime)) return;
      if(debug){ htype(&c,1); htype(&hcsin.daytime,sizeof(hcsin.daytime)); print(infowin,"\n"); }
      hcsin.daytime.hsec=bcd2b(hcsin.daytime.hsec);
      hcsin.daytime.sec =bcd2b(hcsin.daytime.sec );
      hcsin.daytime.min =bcd2b(hcsin.daytime.min );
      hcsin.daytime.hr  =bcd2b(hcsin.daytime.hr  );
      hcsin.daytime.dow =bcd2b(hcsin.daytime.dow );
      hcsin.daytime.day =bcd2b(hcsin.daytime.day );
      hcsin.daytime.mon =bcd2b(hcsin.daytime.mon );
      hcsin.daytime.yr  =bcd2b(hcsin.daytime.yr  );
      if(hcsin.daytime.dow<1 || hcsin.daytime.dow>8)
         break;
      if(!interp)
         attr(statwin,A_REVERSE);
      if(hcsin.daytime.dow>=NDAYS) hcsin.daytime.dow=NDAYS-1;
#if TIMEFMT==1
      printat(statwin,TIMEROW,TIMECOL,
             "%s%s %04d-%02d-%02d %02d:%02d:%02d.%02d%s",
             interp?"TIME:":"",
             days[hcsin.daytime.dow],
             1900+hcsin.daytime.yr,hcsin.daytime.mon,hcsin.daytime.day,
             hcsin.daytime.hr>12?(hcsin.daytime.hr-12):hcsin.daytime.hr,
             hcsin.daytime.min,hcsin.daytime.sec,
             hcsin.daytime.hsec,
             hcsin.daytime.hr>12?"pm":"am"
             );
#elif TIMEFMT==2
      printat(statwin,TIMEROW,TIMECOL,
             "%s%9s %02d-%02d-%04d %02d:%02d:%02d.%02d%s",
             interp?"TIME:":"",
             days[hcsin.daytime.dow],
             hcsin.daytime.mon,hcsin.daytime.day,1900+hcsin.daytime.yr,
             hcsin.daytime.hr>12?(hcsin.daytime.hr-12):hcsin.daytime.hr,
             hcsin.daytime.min,hcsin.daytime.sec,
             hcsin.daytime.hsec,
             hcsin.daytime.hr>12?"pm":"am"
             );
#endif
      if(interp) print(statwin,"\n");
      else       attr(statwin,A_NORMAL);
      break;
   case 0x81:                                                  /* X10 */
      if(unk) print(infowin,"\n");
      unk=0;
      if((rc=readexact(ser,buf,1))!=1) return;
      buf[0]-='A';
      if((rc=readexact(ser,&hcsin.x10[buf[0]],sizeof(hcsin.x10[0])))!=sizeof(hcsin.x10[0])) return;
      if(debug){ htype(&c,1); htype(buf,1); htype(&hcsin.x10[buf[0]]); print(infowin,"\n"); }
      if(buf[0]>=NX10ROW) buf[0]=NX10ROW-1;
      if(interp)
         print(statwin,"X10:%c:",buf[0]+'A');
      else
         printat(statwin,X10ROW+x10row[buf[0]],X10COL,"X10     :  %c: ",buf[0]+'A');
      printlbin(statwin,hcsin.x10[buf[0]].lo);
      addc(statwin,' ');
      printlbin(statwin,hcsin.x10[buf[0]].hi);
      eol(statwin);
      break;
   case 0x86:                                      /* network modules */
      if(unk) print(infowin,"\n");
      unk=0;
      if((rc=readexact(ser,&hcsin.net[16],sizeof(hcsin.net[16])))!=sizeof(hcsin.net[16])) return;
      if(debug){ htype(&c,1); htype(&hcsin.net[16],sizeof(hcsin.net[16])); print(infowin,"\n"); }
      i=hcsin.net[16].modnum;
      hcsin.net[i]=hcsin.net[16];
      if(hcsin.net[i].modtyp>=NNETM) hcsin.net[i].modtyp=NNETM-1;
      if(hcsin.net[i].stat>=NNETS) hcsin.net[i].stat=NNETS-1;
      if(interp)
         print(statwin,"Net:%s:%d:%-7s",
                   netm[hcsin.net[i].modtyp],hcsin.net[i].modnum,nets[hcsin.net[i].stat]);
      else
         printat(statwin,NETROW+hcsin.net[i].modtyp,NETCOL,"Net     : %-5s %d: %-7s",
                   netm[hcsin.net[i].modtyp],hcsin.net[i].modnum,nets[hcsin.net[i].stat]);
      eol(statwin);
      break;
   case 0x82:                                           /* digital in */
      if(unk) print(infowin,"\n");
      unk=0;
      if((rc=readexact(ser,buf,1))!=1) return;
      i=buf[0];
      if((rc=readexact(ser,&hcsin.din[i],sizeof(hcsin.din[0])))!=sizeof(hcsin.din[0])) return;
      if(debug){ htype(&c,1); htype(buf,1); htype(&hcsin.din[i],sizeof(hcsin.din[0])); print(infowin,"\n"); }
      if(interp)
         print(statwin,"Input:%d:",i);
      else
      {
         printat(statwin,DIGINROW,DIGINCOLP,"Input   :  ");
         printat(statwin,DIGINROW,DIGINCOL(i),"%d: ",i);
      }
      printlbin(statwin,hcsin.din[i].stat);
/* wtf 
      addc(statwin,' ');
      printlbin(statwin,hcsin.din[i].stat);
      addc(statwin,' ');
      printlbin(statwin,hcsin.din[i].stat);
*/
      eol(statwin);
      break;
   case 0x83:                                          /* digital out */
      if(unk) print(infowin,"\n");
      unk=0;
      if((rc=readexact(ser,buf,1))!=1) return;
      i=buf[0];
      if((rc=readexact(ser,&hcsin.dout[i],sizeof(hcsin.dout[0])))!=sizeof(hcsin.dout[0])) return;
      if(debug){ htype(&c,1); htype(buf,1); htype(&hcsin.dout[i],sizeof(hcsin.dout[0])); print(infowin,"\n"); }
      if(interp)
         print(statwin,"Output:%d:",i+6);
      else
      {
         printat(statwin,DIGOUTROW,DIGOUTCOLP,"Output  :  ");
         printat(statwin,DIGOUTROW,DIGOUTCOL(i),"%d: ",i);
      }
      printlbin(statwin,hcsin.dout[i].stat);
/* wtf
      addc(statwin,' ');
      printlbin(statwin,hcsin.dout[i].stat[1]);
      addc(statwin,' ');
      printlbin(statwin,hcsin.dout[i].stat[2]);
*/
      eol(statwin);
      break;
   case 0x84:                                            /* analog in */
      if(unk) print(infowin,"\n");
      unk=0;
      if(debug) htype(&c,1);
      if((rc=readexact(ser,&c,1))!=1) return;
      if(debug) htype(&c,1);
      switch(c){
      default:
      case  0: i=0; nc=8;  break;
      case  8: i=1; nc=8;  break;
      case 96: i=2; nc=16; break;
      }
      if((rc=readexact(ser,&hcsin.ain[i],nc*2))!=nc*2) return;
      if(debug){ htype(&hcsin.ain[i],nc*2); print(infowin,"\n"); }
      printat(statwin,ANINROW,ANINCOL,"Analogin: %2d:",c);
      for(j=0;j<nc;j++)
         print(statwin," %3u",(uint)hcsin.ain[i].chan[j]);
      eol(statwin);
      /*logit((uint)hcsin.ain[i].chan[0]);*/
      break;
   case 0x85:                                           /* analog out */
      if((rc=readexact(ser,buf,5))!=5) return;
      break;
   case 0x87:                                             /* net bits */
      if((rc=readexact(ser,buf,2))!=2) return;
      break;
   case 0x88:                                      /* console message */
      if(unk) print(infowin,"\n");
      unk=0;
      for(i=0;;)
      {
         if((rc=readexact(ser,buf+i,1))!=1) return;
         if(buf[i]=='\0')
         {
            buf[i]='\n'; buf[++i]='\0'; 
            print(infowin,"%s",buf);
            break;
         }
         else if(i>=INBSZ-2)
         {
            buf[i]='\n'; buf[++i]='\0'; 
            print(infowin,"%s",buf);
            i=0;
         }
         else
            i++;
      }
      break;
   default:
      if(unk==0)
         print(infowin,"Unknown: ");
      unk++;
      attr(infowin,A_BOLD);
      htype(&c,1);
      break;
   }
}                                                 /* end remoteecho() */
/**********************************************************************/

/**********************************************************************/
void localin(ser,loc)
int ser, loc;
{
int rc;
uchar c, buf[16];
struct tm *tms;
time_t t;

   attr(infowin,A_NORMAL);
   do{
      if((rc=read(loc,&c,1))!=1){
         if(debug) print(infowin,"error: read(%d,1)==%d (errno=%d)\n",
                          loc,rc,errno);
         return;
      }
      switch(c){
      case 't':                                           /* set time */
         buf[0]='!';
         if((rc=writeexact(ser,buf,1))!=1) return;
         time(&t);
         tms=localtime(&t);
         buf[0]=0x05;
         buf[1]=0;
         buf[2]=b2bcd(tms->tm_sec);
         buf[3]=b2bcd(tms->tm_min);
         buf[4]=b2bcd(tms->tm_hour);
         buf[5]=b2bcd(tms->tm_wday+1);
         buf[6]=b2bcd(tms->tm_mday);
         buf[7]=b2bcd(tms->tm_mon+1);
         buf[8]=b2bcd(tms->tm_year);
         if((rc=writeexact(ser,buf,9))!=9) return;
         print(infowin,"Time set to: %04u-%02u-%02u %02u:%02u:%02u\n",
                 tms->tm_year+1900,tms->tm_mon+1,tms->tm_mday,
                 tms->tm_hour,tms->tm_min,tms->tm_sec);
         break;
      case 'l':                                       /* load program */
         buf[0]='!';
         buf[1]=0x06;
         if((rc=writeexact(ser,buf,2))!=2) return;
         loadmode=1;
         print(infowin,"Waiting for ACK to load new program\n");
         break;
      case 'c':
         wclear(infowin);
         break;
      case 'C':
         wclear(statwin);
         titleme();
         break;
      case 'q':
         done=1;
         break;
      case '?':
         print(infowin,"\n");
         print(infowin,"t to set HCS time to system time\n");
         print(infowin,"l to load new program into HCS\n");
         print(infowin,"c to clear info\n");
         print(infowin,"C to clear status\n");
         print(infowin,"q to quit\n");
         fresh(infowin);
         c=' ';
         break;
      }
   }while(c=='?');
}                                                    /* end localin() */
/**********************************************************************/

/**********************************************************************/
void pipein(ser,pip)
int ser, pip;
{
char buf[INBSZ], *msg;
int rc, i;

   for(i=0;i<INBSZ-1;i++)
   {
      if((rc=readexact(pip,buf+i,1))!=1) break/*return*/;
      if(buf[i]=='\n') break;
   }
   buf[i]='\0';
   if(buf[0]=='!')
   {
      if((rc=writeexact(ser,buf,i))!=i)
         msg="send failed: ";
      else
         msg="sent: ";
      if(interp) printf("%s",msg);
      else       print(infowin,"%s",msg);
      htype(buf,i);
      if(interp) printf("\n");
      else       print(infowin,"\n");
   }
   else
   if(interp)
      printf("%s\n",buf);
   else
      print(infowin,"%s\n",buf);
   fresh(infowin);
}                                                     /* end pipein() */
/**********************************************************************/

/**********************************************************************/
void terminal(ser,loc,pip)
int ser, loc, pip;
{
int n, rc;
fd_set rd, wr, ex;
#define MAX(a,b) (a>b?a:b)

   FD_ZERO(&rd);
   FD_ZERO(&wr);
   FD_ZERO(&ex);
   FD_SET(ser,&rd);
   FD_SET(loc,&rd);
   FD_SET(ser,&ex);
   FD_SET(loc,&ex);
   n=MAX(ser,loc);
   if(pip!=(-1))
   {
      FD_SET(pip,&rd);
      FD_SET(pip,&ex);
      n=MAX(pip,n);
   }
   n++;
   while(!done && (rc=select(n,&rd,&wr,&ex,(struct timeval *)NULL))>0){
      if(FD_ISSET(ser,&ex) || FD_ISSET(loc,&ex) ||
         (pip!=(-1) && FD_ISSET(pip,&ex)))
         break;
      if(FD_ISSET(ser,&rd))
         remoteecho(ser,loc);                  /* handle remote input */
      if(FD_ISSET(loc,&rd))
         localin(ser,loc);                      /* handle local input */
      if(pip!=(-1) && FD_ISSET(pip,&rd))
         pipein(ser,pip);
      FD_ZERO(&rd);
      FD_ZERO(&wr);
      FD_ZERO(&ex);
      FD_SET(ser,&rd);
      FD_SET(loc,&rd);
      FD_SET(ser,&ex);
      FD_SET(loc,&ex);
      if(pip!=(-1))
      {
         FD_SET(pip,&rd);
         FD_SET(pip,&ex);
      }
      fresh(statwin);
      fresh(infowin);
   }
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

static char *Prog="hcs";
/**********************************************************************/
#define P fprintf(stderr,
void use()
{
   P"Use: %s [options] line speed\n",Prog);
   P"     -l line\n");
   P"     -s speed\n");
   P"     -d turn on debug\n");
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
char *line="/dev/cua2";
char *pipe="/tmp/hcs.pipe";
int ser, loc, pip, inopts, inbundle, restore=0, mypipe=0;
int speed=9600, parity='n', bits=8, stopbits=1;
int raw=1, xonoff=0, echo=0, cbreak=1, nodelay=0;

   Prog=argv[0];
   for(argc--,argv++,inopts=1;inopts && argc>0 && **argv=='-';argc--,argv++){
      for(++*argv,inbundle=1;inbundle && **argv!='\0';++*argv){
         switch(**argv){
         case 'l': NEXTCP(line); inbundle=0; break;
         case 's': NEXTINT(speed); inbundle=0; break;
         case 'p': NEXTCP(pipe); inbundle=0; break;
         case 'i': interp++; break;
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
   if(interp){
      loc=0;
   }else{
                                                         /* local tty */
      if((loc=reopenserial(0,(-1)))==(-1)) goto zerr;
      if(saveserial(loc,&save)==(-1)) goto zerr;
      restore=1;
      if(raw && setserial(loc,-1,parity,bits,stopbits)!=0) goto zerr;
      if(setrawserial(loc,raw)!=0) goto zerr;
      if(setxserial(loc,xonoff,echo,cbreak,nodelay)!=0) goto zerr;
   }

   if(!interp){
      infowin=statwin=initscr();
      nonl();
      statwin=newwin((LINES-1)/2,COLS,1,0);
      infowin=newwin((LINES-1)/2,COLS,1+(LINES-1)/2,0);
      scrollok(stdscr,TRUE);
      scrollok(statwin,TRUE);
      scrollok(infowin,TRUE);
      titleme();
                                                        /* remote tty */
      print(infowin,"%s: ",line);
   }
   if((ser=openserial(line,(-1)))==(-1)) goto zerr;
   if(!interp)
      print(infowin,"%d,%c,%d,%d: ",speed,parity,bits,stopbits);
   if(setserial(ser,speed,parity,bits,stopbits)!=0) goto zerr;
   if(debug && !interp) print(infowin,"r:%d,x:%d,e:%d,c:%d,n:%d: ",raw,xonoff,echo,cbreak,nodelay);
   if(setrawserial(ser,raw)!=0) goto zerr;
   if(setxserial(ser,xonoff,echo,cbreak,nodelay)!=0) goto zerr;

   if(!interp){
      print(infowin,"Connected\nQuit with q\n");
      fresh(statwin);
      fresh(infowin);
   }
   if(mknod(pipe,S_IFIFO|0666,0)==0)
      mypipe=1;
   else
   {
      if(interp)
         printf("Can't make %s: %s\n",pipe,strerror(errno));
      else
         print(infowin,"Can't make %s: %s\n",pipe,strerror(errno));
      mypipe=0;
   }
   if((pip=openserial(pipe,(-1)))==(-1) && !interp)
      print(infowin,"Can't open %s: %s\n",pipe,strerror(errno));
   terminal(ser,loc,pip);
   if(!interp){
      print(infowin,"disconnecting");
      fresh(infowin);
      if(restserial(loc,&save)==(-1)) goto zerr;
      closeserial(loc);
   }
   closeserial(ser);
   if(pip!=(-1))
      closeserial(pip);
   if(!interp){
      print(infowin,"\rdisconnected \n");
      fresh(infowin);
   }
   if(mypipe)
      unlink(pipe);
   return(0);
zerr:
   if(!interp)
      endwin();
   if(restore){
   int se=errno;
      restserial(loc,&save);
      errno=se;
   }
   perror("");
   if(mypipe)
      unlink(pipe);
   return(1);
}
/**********************************************************************/
