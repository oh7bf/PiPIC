/**************************************************************************
 * 
 * Control external power switch using i2c bus on Raspberry Pi. The switch
 * has a PIC processor to interprete commands send by this program.  
 *       
 * Copyright (C) 2014 Jaakko Koivuniemi.
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
 * Sun Feb 16 14:29:25 CET 2014
 * Edit: Thu Sep 11 21:50:56 CEST 2014
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
#include "pipicswd.h"
#include "logmessage.h"
#include "writecmd.h"
#include "readdata.h"
#include "testi2c.h"

#define CHECK_BIT(var,pos) !!((var) & (1<<(pos)))

const int version=20140911; // program version

int portno=5001; // socket port number

float picycle=0.445; // length of PIC counter cycles [s]

const char *i2cdev="/dev/i2c-1"; // i2c device file
const int  address=0x27;
const int  i2lockmax=10; // maximum number of times to try lock i2c port  
int forcereset=0; // force PIC timer reset if i2c test fails

const char confile[200]="/etc/pipicswd_config";

const char pidfile[200]="/var/run/pipicswd.pid";

int loglev=3;
const char *logfile="/var/log/pipicswd.log"; // log file
char message[200]="";

char status[200]=""; // switch status message

int initswitch1=0; // switch 1 at start 0=do nothing, 1=switch on, 2=off
int initswitch2=0; // switch 2 at start 0=do nothing, 1=switch on, 2=off
int stopswitch1=0; // switch 1 at stop 0=do nothing, 1=switch on, 2=off
int stopswitch2=0; // switch 2 at stop 0=do nothing, 1=switch on, 2=off


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
    sprintf(message,"Read configuration file");
    logmessage(logfile,message,loglev,4);

    while((read=getline(&line,&len,cfile))!=-1)
    {
       if(sscanf(line,"%s %f",par,&value)!=EOF)
       {
          if(strncmp(par,"LOGLEVEL",8)==0)
          {
             loglev=(int)value;
             sprintf(message,"Log level set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"DCSWITCHPORT",12)==0)
          {
             portno=(int)value;
             sprintf(message,"Switch port number set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"INITSWITCH1",11)==0)
          {
             initswitch1=(int)value;
             sprintf(message,"Switch 1 initialization set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"INITSWITCH2",11)==0)
          {
             initswitch2=(int)value;
             sprintf(message,"Switch 2 initialization set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"STOPSWITCH1",11)==0)
          {
             stopswitch1=(int)value;
             sprintf(message,"Switch 1 stop value set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"STOPSWITCH2",11)==0)
          {
             stopswitch2=(int)value;
             sprintf(message,"Switch 2 stop value set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"PICYCLE",7)==0)
          {
             picycle=value;
             sprintf(message,"PIC cycle %f s",value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"FORCERESET",10)==0)
          {
             if(value==1)
             {
                forcereset=1;
                sprintf(message,"Force PIC timer reset at start if i2c test fails");
                logmessage(logfile,message,loglev,4);
             }
             else
             {
                forcereset=0;
                sprintf(message,"Exit in case of i2c test failure");
                logmessage(logfile,message,loglev,4);
             }
          }
       }
    }
    fclose(cfile);
  }
  else
  {
    sprintf(message, "Could not open %s", confile);
    logmessage(logfile, message, loglev,4);
  }
}

// read switch status
int read_status()
{
  int ok=-1;
  int gpio=0;

  ok=write_cmd(0x01,0x05,1); 
  if(ok==1)
  { 
    gpio=read_data(1);
    if(CHECK_BIT(gpio,4)==1) 
    {
      strcpy(message,"switch 1 closed");
      strcpy(status,"1 closed"); 
    }
    else 
    {
      strcpy(message,"switch 1 open");
      strcpy(status,"1 open");
    }

    if(CHECK_BIT(gpio,5)==1) 
    {
      strcat(message," switch 2 closed");
      strcat(status," 2 closed");
    }
    else 
    {
      strcat(message," switch 2 open");
      strcat(status," 2 open");
    }
    logmessage(logfile,message,loglev,3);
  }
  else
  {
    strcpy(message,"failed to read PIC GPIO register"); 
    logmessage(logfile,message,loglev,4);
  }

  return ok;
}

int operate_switch1(int switch1)
{
  int ok=0;

  if(switch1==1) 
  {
     ok=write_cmd(0x24,0x00,0); 
     if(ok!=1) 
     {
       sprintf(message,"closing switch 1 failed");
       logmessage(logfile,message,loglev,4);
     }
     else
     {
       sprintf(message,"switch 1 closed");
       logmessage(logfile,message,loglev,2);
     }

  }
  else if(switch1==2) 
  {
     ok=write_cmd(0x14,0x00,0); 
     if(ok!=1) 
     {
       sprintf(message,"opening switch 1 failed");
       logmessage(logfile,message,loglev,4);
     }
     else
     {
       sprintf(message,"switch 1 opened");
       logmessage(logfile,message,loglev,2);
     }
  }

  return ok;
}

int operate_switch2(int switch2)
{
  int ok=0;

  if(switch2==1) 
  {
     ok=write_cmd(0x25,0x00,0); 
     if(ok!=1) 
     {
       sprintf(message,"closing switch 2 failed");
       logmessage(logfile,message,loglev,4);
     }
     else
     {
       sprintf(message,"switch 2 closed");
       logmessage(logfile,message,loglev,2);
     }
  }
  else if(switch2==2) 
  {
     ok=write_cmd(0x15,0x00,0); 
     if(ok!=1) 
     {
       sprintf(message,"opening switch 2 failed");
       logmessage(logfile,message,loglev,4);
     }
     else
     {
       sprintf(message,"switch 2 opened");
       logmessage(logfile,message,loglev,2);
     }
  }

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
  logmessage(logfile,message,loglev,4);
  cont=0;
}

void terminate(int sig)
{
  int ok=0;

  sprintf(message,"signal %d catched",sig);
  logmessage(logfile,message,loglev,4);

  sleep(1);
  ok=operate_switch1(stopswitch1);
  ok=operate_switch2(stopswitch2);

  sleep(1);
  strcpy(message,"stop");
  logmessage(logfile,message,loglev,4);

  cont=0;
}

void hup(int sig)
{
  sprintf(message,"signal %d catched",sig);
  logmessage(logfile,message,loglev,4);
}



int main()
{  
  int ok=0;

  sprintf(message,"pipicswd v. %d started",version); 
  logmessage(logfile,message,loglev,4);

  signal(SIGINT,&stop); 
  signal(SIGKILL,&stop); 
  signal(SIGTERM,&terminate); 
  signal(SIGQUIT,&stop); 
  signal(SIGHUP,&hup); 

  read_config(); // read configuration file

  int i2cok=testi2c(); // test i2c data flow to PIC 
  if(i2cok==1)
  {
    strcpy(message,"i2c dataflow test ok"); 
    logmessage(logfile,message,loglev,4);
  }
  else
  {
    if(forcereset==1)
    {
      sleep(1); 
      strcpy(message,"try to reset timer now");
      logmessage(logfile,message,loglev,4); 
      ok=resetimer();
      sleep(1);
      if(resetimer()!=1)
      {
        strcpy(message,"failed to reset timer");
        logmessage(logfile,message,loglev,4);
        cont=0; 
      }
      else
      {
        sleep(1);
        i2cok=testi2c(); // test i2c data flow to PIC 
        if(i2cok==1)
        {
          strcpy(message,"i2c dataflow test ok"); 
          logmessage(logfile,message,loglev,4);
        }
        else
        {
          strcpy(message,"i2c dataflow test failed");
          logmessage(logfile,message,loglev,4);
          cont=0;
          exit(EXIT_FAILURE);
        }
      }
    }
    else
    {
      strcpy(message,"i2c dataflow test failed, exit");
      logmessage(logfile,message,loglev,4); 
      strcpy(message,"try to reset timer with 'pipic -a 27 -c 50'");
      logmessage(logfile,message,loglev,4); 
      strcpy(message,"and restart with 'service pipicswd start'");
      logmessage(logfile,message,loglev,4); 
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
    strcpy(message,"failed to create child process"); 
    logmessage(logfile,message,loglev,4);
    exit(EXIT_FAILURE);
  }
        
  if((chdir("/")) < 0) 
  {
    strcpy(message,"failed to change to root directory"); 
    logmessage(logfile,message,loglev,4);
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
    sprintf(message,"Could not open PID lock file %s, exiting", pidfile);
    logmessage(logfile,message,loglev,4);
    exit(EXIT_FAILURE);
  }

  if(flock(fileno(pidf),LOCK_EX||LOCK_NB)==-1)
  {
    sprintf(message,"Could not lock PID lock file %s, exiting", pidfile);
    logmessage(logfile,message,loglev,4);
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
    sprintf(message,"Could not open socket");
    logmessage(logfile,message,loglev,4);
    exit(EXIT_FAILURE);
  }
  else
  {
    sprintf(message,"Socket open");
    logmessage(logfile,message,loglev,2);
  }
  
  memset(&serv_addr, '0', sizeof(serv_addr));
  memset(sbuff, '0', sizeof(sbuff)); 

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(portno); 

  if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0)
  {
    sprintf(message,"Could not bind socket");
    logmessage(logfile,message,loglev,4);
    exit(EXIT_FAILURE);
  }
  else
  {
    sprintf(message,"Socket binding successful");
    logmessage(logfile,message,loglev,2);
  }

  listen(sockfd, 1); // listen one client only 
  clilen=sizeof(cli_addr);

// initialize switches
  ok=operate_switch1(initswitch1);
  ok=operate_switch2(initswitch2);
 
  sleep(1);
  ok=read_status();

  int n=0;
  while(cont==1)
  {
    connfd=accept(sockfd, (struct sockaddr*)&cli_addr, (socklen_t *)&clilen); 
    if(connfd<0) 
    {
      sprintf(message,"Socket accept failed");
      logmessage(logfile,message,loglev,4);
      exit(EXIT_FAILURE);
    }
    else
    {
      sprintf(message,"Socket accepted");
      logmessage(logfile,message,loglev,2);
    }

    bzero(rbuff,25);
    n=read(connfd,rbuff,24);
    if(n<0)
    {
      sprintf(message,"Socket reading failed");
      logmessage(logfile,message,loglev,4);
      exit(EXIT_FAILURE);
    }
    else
    {
      sprintf(message,"Received: %s",rbuff);
      logmessage(logfile,message,loglev,3);
    }

    if(strncmp(rbuff,"open 1",6)==0)
    {
      ok=operate_switch1(2);
      sleep(1);
    } 
    else if(strncmp(rbuff,"close 1",7)==0)
    {
      ok=operate_switch1(1);
      sleep(1);
    } 
    else if(strncmp(rbuff,"open 2",6)==0)
    {
      ok=operate_switch2(2);
      sleep(1);
    } 
    else if(strncmp(rbuff,"close 2",7)==0)
    {
      ok=operate_switch2(1);
      sleep(1);
    } 

    ok=read_status();
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
      logmessage(logfile,message,loglev,3);
    }

    close(connfd);

    sleep(1);
  }

  strcpy(message,"remove PID file");
  logmessage(logfile,message,loglev,4);
  ok=remove(pidfile);

  return ok;
}
