/**************************************************************************
 * 
 * Control external H-bridge using i2c bus on Raspberry Pi. The H-bridge 
 * has a PIC processor to interprete commands send by this program.  
 *       
 * Copyright (C) 2014 - 2015 Jaakko Koivuniemi.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 ****************************************************************************
 *
 * Sun Aug 10 20:06:24 CEST 2014
 * Edit: Sat Feb 21 19:49:35 CET 2015
 *
 * Jaakko Koivuniemi
 **/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include "pipichbd.h"
#include "writecmd.h"
#include "readdata.h"
#include "testi2c.h"

#define CHECK_BIT(var,pos) !!((var) & (1<<(pos)))

const int version=20150221; // program version

const char *i2cdev="/dev/i2c-1"; // i2c device file
const int address=0x28; // PiPIC i2c address
const int i2lockmax=10; // maximum number of times to try lock i2c port  
int loglev=5; // log level
char message[200]="";

int portno=5002; // socket port number
float picycle=0.445; // length of PIC counter cycles [s]
int forcereset=0; // force PIC timer reset if i2c test fails
int maxcycles=-1; // maximum allowed rotation time in PIC cycles
float rotmax=1; // maximum number of axis rotations
float motrpm=7; // axis turning speed [rpm]
int mpos=-1; // motor position from AN0 [0-1023]
int pot=-1; // potentiometer from AN1 [0-1023]
int track=0; // 1=track potentiometer position
int minpos=0; // motor minimum position [0-1023]
int maxpos=1023; // motor maximum position [0-1023]
char status[200]=""; // bridge status message

const char confile[200]="/etc/pipichbd_config";

const char pidfile[200]="/var/run/pipichbd.pid";

// read configuration file if it exists
void read_config()
{
  FILE *cfile;
  char *line=NULL;
  char par[20];
  float value;
  size_t len;
  ssize_t read;

  cfile=fopen(confile, "r");
  if(NULL!=cfile)
  {
    syslog(LOG_INFO|LOG_DAEMON, "Read configuration file");

    while((read=getline(&line,&len,cfile))!=-1)
    {
       if(sscanf(line,"%s %f",par,&value)!=EOF)
       {
          if(strncmp(par,"LOGLEVEL",8)==0)
          {
             loglev=(int)value;
             sprintf(message,"Log level set to %d",(int)value);
             syslog(LOG_INFO|LOG_DAEMON, "%s", message);
             setlogmask(LOG_UPTO (loglev));
          }
          if(strncmp(par,"HBRIDGEPORT",11)==0)
          {
             portno=(int)value;
             sprintf(message,"Bridge port number set to %d",(int)value);
             syslog(LOG_INFO|LOG_DAEMON, "%s", message);
          }
          if(strncmp(par,"PICYCLE",7)==0)
          {
             picycle=value;
             sprintf(message,"PIC cycle %f s",value);
             syslog(LOG_INFO|LOG_DAEMON, "%s", message);
          }
          if(strncmp(par,"ROTMAX",6)==0)
          {
             rotmax=value;
             sprintf(message,"maximum motor turns %f",value);
             syslog(LOG_INFO|LOG_DAEMON, "%s", message);
          }
          if(strncmp(par,"MOTRPM",6)==0)
          {
             motrpm=value;
             sprintf(message,"motor speed %f rpm",value);
             syslog(LOG_INFO|LOG_DAEMON, "%s", message);
          }
          if(strncmp(par,"TRACK",5)==0)
          {
             track=(int)value;
             if(track==1) syslog(LOG_INFO|LOG_DAEMON, "track potentiometer");
          }
          if(strncmp(par,"MINPOS",6)==0)
          {
             minpos=(int)value;
             sprintf(message,"set minimum position to %d",minpos);
             syslog(LOG_INFO|LOG_DAEMON, "%s", message);
          }
          if(strncmp(par,"MAXPOS",6)==0)
          {
             maxpos=(int)value;
             sprintf(message,"set maximum position to %d",maxpos);
             syslog(LOG_INFO|LOG_DAEMON, "%s", message);
          }
          if(strncmp(par,"FORCERESET",10)==0)
          {
             if(value==1)
             {
                forcereset=1;
                syslog(LOG_INFO|LOG_DAEMON, "Force PIC timer reset at start if i2c test fails");
             }
             else
             {
                forcereset=0;
                syslog(LOG_INFO|LOG_DAEMON, "Exit in case of i2c test failure");
             }
          }
       }
    }
    fclose(cfile);
  }
  else
  {
    sprintf(message, "Could not open %s", confile);
    syslog(LOG_ERR|LOG_DAEMON, "%s", message);
  }
}

// read motor status: stopped, rotate cw or rotate ccw
int read_status()
{
  int ok=-1;
  int gpio=0;

  ok=write_cmd(0x01,0x05,1); 
  if(ok==1)
  { 
    gpio=read_data(1);
    if((CHECK_BIT(gpio,4)==0)&&(CHECK_BIT(gpio,5)==0)) 
    {
      strcpy(message,"motor stopped");
      strcpy(status,"motor stopped"); 
    }
    else if((CHECK_BIT(gpio,4)==1)&&(CHECK_BIT(gpio,5)==0))  
    {
      strcpy(message,"rotate cw");
      strcpy(status,"rotate cw");
    }
    else if((CHECK_BIT(gpio,4)==0)&&(CHECK_BIT(gpio,5)==1))  
    {
      strcpy(message,"rotate ccw");
      strcpy(status,"rotate ccw");
    }
    else if((CHECK_BIT(gpio,4)==1)&&(CHECK_BIT(gpio,5)==1))  
    {
      strcpy(message,"breaking");
      strcpy(status,"breaking");
    }

    syslog(LOG_INFO|LOG_DAEMON, "%s", message);
  }
  else syslog(LOG_ERR|LOG_DAEMON, "failed to read PIC GPIO register"); 

  return ok;
}

// read motor position sensor from AN0
int read_motorpos()
{
  int ok=-1;
  int pos=-1;
  ok=write_cmd(0x40,0,0); // A/D conversion is done twice 
  ok=write_cmd(0x40,0,0); 
  if(ok==1)
  { 
    pos=read_data(2);
    sprintf(message,"Motor position at %d",pos);
    syslog(LOG_INFO|LOG_DAEMON, "%s", message);
    if((pos<0)||(pos>1023)) pos=-1;
  }
  else syslog(LOG_ERR|LOG_DAEMON, "Failed to read PIC AN0"); 

  return pos;
}

// read potentiometer from AN1
int read_potentiometer()
{
  int ok=-1;
  int pot=-1;
  ok=write_cmd(0x41,0,0); // A/D conversion is done twice 
  ok=write_cmd(0x41,0,0); 
  if(ok==1)
  { 
    pot=read_data(2);
    sprintf(message,"Potentiometer at %d",pot);
    syslog(LOG_INFO|LOG_DAEMON, "%s", message);
  }
  else syslog(LOG_ERR|LOG_DAEMON, "Failed to read PIC AN1"); 

  return pot;
}

// stop motor
int stop_motor()
{
  int ok=0;
  ok=write_cmd(0x30,0x0F,1);
  if(ok!=1) syslog(LOG_ERR|LOG_DAEMON, "stopping motor failed");
  else syslog(LOG_INFO|LOG_DAEMON, "motor stopped");

  return ok;
}

// turn motor, rotcw=+1 clockwise, -1 counter clockwise, stop after given
// number of cycles
int turn_motor(int rotcw, int cycles)
{
  int ok=0;

  if((cycles>2)&&(cycles<maxcycles))
  { 
// timed task1 to start turning motor
     ok=write_cmd(0x62,2,4); // start after 2 cycles
     if(rotcw==+1) 
     {
       ok=write_cmd(0x63,0x301F,2);
       syslog(LOG_INFO|LOG_DAEMON, "turn motor cw");
     } 
     else if(rotcw==-1) 
     {
       ok=write_cmd(0x63,0x302F,2);
       syslog(LOG_INFO|LOG_DAEMON, "turn motor ccw");
     } 
     ok=write_cmd(0x64,0,1); // do task once

// timed task2 to stop motor
     ok=write_cmd(0x72,cycles,4); // stop after given cycles
     ok=write_cmd(0x73,0x300F,2);
     sprintf(message,"stop motor after %d PIC cycles",cycles);
     syslog(LOG_INFO|LOG_DAEMON, "%s", message);
     ok=write_cmd(0x74,0,1); // do task once

     ok=write_cmd(0x61,0,0); // start task1
     ok=write_cmd(0x71,0,0); // start task2
  }

  return ok;
}

// turn motor to reach given position, return number of seconds needed
// for turning
int goto_pos(int topos)
{
  int ok=0;
  int dt=0;
  int rotcw=1;
  if(topos>mpos) rotcw=-1;

  int cycles=0;

  if((topos>=minpos)&&(topos<=maxpos)&&(mpos>=0)&&(mpos<1024))
  {
    cycles=(int)abs(rotmax*60.0*(topos-mpos)/(1024.0*motrpm*picycle));
    dt=cycles*picycle;
    sprintf(message,"turning time %d PIC cycles and direction %d",cycles,rotcw);
    syslog(LOG_INFO|LOG_DAEMON, "%s", message);

    ok=turn_motor(rotcw,cycles);
  }
  else
  {
    sprintf(message,"motor position out of range [%d-%d]",minpos,maxpos);
    syslog(LOG_NOTICE|LOG_DAEMON, "%s", message);
  }

  return (dt*ok);
}

// set motor position, first 90 % of travel, then rest and finally correction
// steps if needed
int set_pos(int topos)
{
  int ok=0;

  int nxtpos=(int)(0.9*(topos-mpos)+mpos);
  int dt=goto_pos(nxtpos);

  sleep(dt+1);
  mpos=read_motorpos();
  dt=goto_pos(topos);
  sleep(dt+1);
  mpos=read_motorpos();

  return ok;
}


// reset PIC internal timer
int resetimer()
{
  int ok=-1;

  ok=write_cmd(0x50,0,0);

  return ok;
}

int cont=1; /* main loop flag */

void stop(int sig)
{
  sprintf(message,"signal %d catched, stop",sig);
  syslog(LOG_NOTICE|LOG_DAEMON, "%s", message);
  cont=0;
}

void terminate(int sig)
{
  sprintf(message,"signal %d catched",sig);
  syslog(LOG_NOTICE|LOG_DAEMON, "%s", message);
  sleep(1);
  syslog(LOG_NOTICE|LOG_DAEMON, "stop");

  cont=0;
}

void hup(int sig)
{
  sprintf(message,"signal %d catched",sig);
  syslog(LOG_NOTICE|LOG_DAEMON, "%s", message);
  syslog(LOG_NOTICE|LOG_DAEMON, "stop tracking potentiometer");
  track=0;
}

int main()
{  
  int ok=0;

  setlogmask(LOG_UPTO (loglev));
  syslog(LOG_NOTICE|LOG_DAEMON, "pipichbd v. %d started", version);

  signal(SIGINT,&stop); 
  signal(SIGKILL,&stop); 
  signal(SIGTERM,&terminate); 
  signal(SIGQUIT,&stop); 
  signal(SIGHUP,&hup); 

  read_config(); // read configuration file
  maxcycles=(int)(rotmax*60/(motrpm*picycle));
  sprintf(message,"set maximum turning time to %d cycles",maxcycles); 
  syslog(LOG_NOTICE|LOG_DAEMON, "%s", message);

  int i2cok=testi2c(); // test i2c data flow to PIC 
  if(i2cok==1) syslog(LOG_NOTICE|LOG_DAEMON, "i2c dataflow test ok"); 
  else
  {
    if(forcereset==1)
    {
      sleep(1); 
      syslog(LOG_NOTICE|LOG_DAEMON, "try to reset timer now");
      ok=resetimer();
      sleep(1);
      if(resetimer()!=1)
      {
        syslog(LOG_ERR|LOG_DAEMON, "failed to reset timer");
        cont=0; 
      }
      else
      {
        sleep(1);
        i2cok=testi2c(); // test i2c data flow to PIC 
        if(i2cok==1) syslog(LOG_NOTICE|LOG_DAEMON, "i2c dataflow test ok"); 
        else
        {
          syslog(LOG_ERR|LOG_DAEMON, "i2c dataflow test failed");
          cont=0;
          exit(EXIT_FAILURE);
        }
      }
    }
    else
    {
      syslog(LOG_ERR|LOG_DAEMON, "i2c dataflow test failed, exit");
      syslog(LOG_NOTICE|LOG_DAEMON, "try to reset timer with 'pipic -a [address] -c 50' and restart with 'service pipichbd start'");
      cont=0;
      exit(EXIT_FAILURE);
    }
  }

  pid_t pid, sid;
        
  pid=fork();
  if(pid<0) 
  {
    exit(EXIT_FAILURE);
  }

  if(pid>0) 
  {
    exit(EXIT_SUCCESS);
  }

  umask(0);

  /* Create a new SID for the child process */
  sid=setsid();
  if(sid<0) 
  {
    syslog(LOG_ERR|LOG_DAEMON, "failed to create child process"); 
    exit(EXIT_FAILURE);
  }
        
  if((chdir("/")) < 0) 
  {
    syslog(LOG_ERR|LOG_DAEMON, "failed to change to root directory"); 
    exit(EXIT_FAILURE);
  }
        
  /* Close out the standard file descriptors */
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  FILE *pidf;
  pidf=fopen(pidfile,"w");

  if(pidf==NULL)
  {
    sprintf(message, "Could not open PID lock file %s, exiting", pidfile);
    syslog(LOG_ERR|LOG_DAEMON, "%s", message);
    exit(EXIT_FAILURE);
  }

  if(flock(fileno(pidf),LOCK_EX||LOCK_NB)==-1)
  {
    sprintf(message,"Could not lock PID lock file %s, exiting", pidfile);
    syslog(LOG_ERR|LOG_DAEMON, "%s", message);
    exit(EXIT_FAILURE);
  }

  fprintf(pidf,"%d\n",getpid());
  fclose(pidf);

// open socket
  int sockfd, connfd, clilen;
  char rbuff[25];
  char sbuff[25];
  struct sockaddr_in serv_addr, cli_addr; 

  sockfd=socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd<0) 
  {
    syslog(LOG_ERR|LOG_DAEMON, "Could not open socket");
    exit(EXIT_FAILURE);
  }
  else syslog(LOG_NOTICE|LOG_DAEMON, "Socket open");
  
  memset(&serv_addr, '0', sizeof(serv_addr));
  memset(sbuff, '0', sizeof(sbuff)); 

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(portno); 

  if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0)
  {
    syslog(LOG_ERR|LOG_DAEMON, "Could not bind socket");
    exit(EXIT_FAILURE);
  }
  else
  {
    syslog(LOG_NOTICE|LOG_DAEMON, "Socket binding successful");
  }

  listen(sockfd, 1); // listen one client only 
  clilen=sizeof(cli_addr);
 
  sleep(1);

  int cycles=0;
  int dt=0;
  int idelt=0;
  int mpos2=0;
  int pot2=0;
  int topos=-1;
  mpos=read_motorpos();
  pot=read_potentiometer();
  if(loglev>2)
  {
    sprintf(message, "motor at %d and pot at %d",mpos,pot);
    syslog(LOG_INFO|LOG_DAEMON, "%s", message);
  }

  int n=0;
  while(cont==1)
  {
// track motor position with the potentiometer connected on AN1
    idelt=10;
    while(track==1)
    {
      mpos=read_motorpos();
      sleep(1);
      mpos=read_motorpos();
      sleep(1);
      pot=read_potentiometer();
      sleep(1);
      pot=read_potentiometer();

      if(pot<minpos) pot=minpos;
      else if(pot>maxpos) pot=maxpos;
      if((mpos!=mpos2)||(pot!=pot2))
      {
        sprintf(message,"motor at %d and potentiometer at %d",mpos,pot);
        syslog(LOG_INFO|LOG_DAEMON, "%s", message);
      }
      mpos2=mpos;
      pot2=pot;
      if(abs(mpos-pot)>1)
      {
        dt=goto_pos(pot);
        if(dt>=0) sleep(dt+1);
        idelt=10;
      }
      else if(idelt<50) idelt++; // read mpos and pot less freq if no change
      sleep(idelt/10); 
    }

    connfd=accept(sockfd, (struct sockaddr*)&cli_addr, (socklen_t *)&clilen); 
    if(connfd<0) 
    {
      syslog(LOG_ERR|LOG_DAEMON, "Socket accept failed");
      exit(EXIT_FAILURE);
    }
    else syslog(LOG_NOTICE|LOG_DAEMON, "Socket accepted");

    bzero(rbuff,25);
    n=read(connfd,rbuff,24);
    if(n<0)
    {
      syslog(LOG_ERR|LOG_DAEMON, "Socket reading failed");
      exit(EXIT_FAILURE);
    }
    else
    {
      sprintf(message,"Received: %s",rbuff);
      syslog(LOG_DEBUG, "%s", message);
    }

    if(strncmp(rbuff,"stop",4)==0)
    {
      ok=stop_motor();
      sprintf(status,"motor stopped");
    } 
    else if(strncmp(rbuff,"pos",3)==0)
    {
      mpos=read_motorpos();
      sprintf(status,"motor at %d",mpos);
    } 
    else if(strncmp(rbuff,"pot",3)==0)
    {
      pot=read_potentiometer();
      sprintf(status,"pot at %d",pot);
    } 
    else if(strncmp(rbuff,"cw",2)==0)
    {
      if(sscanf(rbuff,"cw %d",&cycles)!=EOF)
      {
        ok=turn_motor(1,cycles);
        sprintf(status,"turn cw");
      }
    } 
    else if(strncmp(rbuff,"ccw",3)==0)
    {
      if(sscanf(rbuff,"ccw %d",&cycles)!=EOF)
      {
        ok=turn_motor(-1,cycles);
        sprintf(status,"turn ccw");
      }
    }
    else if(strncmp(rbuff,"go",2)==0)
    {
      if(sscanf(rbuff,"go %d",&topos)!=EOF)
      {
        mpos=read_motorpos(); // initial position
        ok=goto_pos(topos);
        mpos=read_motorpos();
        sprintf(status,"motor at %d",mpos);
      }
    }
    else if(strncmp(rbuff,"set",3)==0)
    {
      if(sscanf(rbuff,"set %d",&topos)!=EOF)
      {
        mpos=read_motorpos(); // initial position
        ok=set_pos(topos);
        mpos=read_motorpos();
        sprintf(status,"motor at %d",mpos);
      }
    }
    else if(strncmp(rbuff,"track",5)==0)
    {
      syslog(LOG_NOTICE|LOG_DAEMON, "start tracking motor position");
      sprintf(status,"start tracking motor position");
      track=1;
    }
    else if(strncmp(rbuff,"status",6)==0)
    {
      ok=read_status();
    } 

    sleep(1);
    snprintf(sbuff,sizeof(sbuff),"%.24s",status);

    n=write(connfd,sbuff,strlen(sbuff)); 
    if(n<0)
    {
      sprintf(message,"Socket writing failed");
      exit(EXIT_FAILURE);
    }
    else
    {
      sprintf(message,"Send: %s",sbuff);
      syslog(LOG_DEBUG, "%s", message);
    }

    close(connfd);
    strcpy(status,"");
    sleep(1);
  }

  syslog(LOG_NOTICE|LOG_DAEMON, "remove PID file");
  ok=remove(pidfile);

  return ok;
}
