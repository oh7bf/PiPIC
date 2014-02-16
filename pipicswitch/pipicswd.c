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
 * Edit: 
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

#define CHECK_BIT(var,pos) !!((var) & (1<<(pos)))

const int version=20140216; // program version

int statusint=60; // switch status reading interval [s]

float picycle=0.445; // length of PIC counter cycles [s]

const char i2cdev[100]="/dev/i2c-1";
const int  address=0x27;
const int  i2lockmax=10; // maximum number of times to try lock i2c port  

const char confile[200]="/etc/pipicswd_config";

const char pidfile[200]="/var/run/pipicswd.pid";

int loglev=3;
const char logfile[200]="/var/log/pipicswd.log";
char message[200]="";

int initswitch1=0; // switch 1 at start 0=do nothing, 1=switch on, 2=off
int initswitch2=0; // switch 2 at start 0=do nothing, 1=switch on, 2=off
int stopswitch1=0; // switch 1 at stop 0=do nothing, 1=switch on, 2=off
int stopswitch2=0; // switch 2 at stop 0=do nothing, 1=switch on, 2=off

void logmessage(const char logfile[200], const char message[200], int loglev, int msglev)
{
  time_t now;
  char tstr[25];
  struct tm* tm_info;
  FILE *log;

  time(&now);
  tm_info=localtime(&now);
  strftime(tstr,25,"%Y-%m-%d %H:%M:%S",tm_info);
  if(msglev>=loglev)
  {
    log=fopen(logfile, "a");
    if(NULL==log)
    {
      perror("could not open log file");
    }
    else
    { 
      fprintf(log,"%s ",tstr);
      fprintf(log,"%s\n",message);
      fclose(log);
    }
  }
}

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
          if(strncmp(par,"STATUSINT",9)==0)
          {
             statusint=(int)value;
             sprintf(message,"Switch status reading interval set to %d s",(int)value);
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

// write i2c command to PIC optionally followed by data, length is the number 
// of bytes and can be 0, 1, 2 or 4 
// return: 1=ok, -1=open failed, -2=lock failed, -3=bus access failed, 
// -4=i2c slave writing failed
int write_cmd(int cmd, int data, int length)
{
  int ok=0;
  int fd,rd;
  int rnxt=0;
  int cnt=0;
  unsigned char buf[10];

  if((cmd>=0)&&(cmd<=255))
  {
    if((fd=open(i2cdev, O_RDWR)) < 0) 
    {
      sprintf(message,"Failed to open i2c port");
      logmessage(logfile,message,loglev,4);
      return -1;
    }

    rd=flock(fd, LOCK_EX|LOCK_NB);

    cnt=i2lockmax;
    while((rd==1)&&(cnt>0)) // try again if port locking failed
    {
      sleep(1);
      rd=flock(fd, LOCK_EX|LOCK_NB);
      cnt--;
    }

    if(rd)
    {
      sprintf(message,"Failed to lock i2c port");
      logmessage(logfile,message,loglev,4);
      return -2;
    }

    if(ioctl(fd, I2C_SLAVE, address) < 0) 
    {
      sprintf(message,"Unable to get bus access to talk to slave");
      logmessage(logfile,message,loglev,4);
      return -3;
    }

    buf[0]=cmd;
    if(length==1)
    { 
      buf[1]=data;
      sprintf(message,"Send 0x%02x%02x",buf[0],buf[1]);
      logmessage(logfile,message,loglev,1);
      if((write(fd, buf, 2)) != 2) 
      {
        sprintf(message,"Error writing to i2c slave");
        logmessage(logfile,message,loglev,4);
        return -4;
      }
   }
   else if(length==2)
   { 
      buf[1]=(int)(data/256);
      buf[2]=data%256;
      sprintf(message,"Send 0x%02x%02x%02x",buf[0],buf[1],buf[2]);
      logmessage(logfile,message,loglev,1); 
      if((write(fd, buf, 3)) != 3) 
      {
        sprintf(message,"Error writing to i2c slave");
        logmessage(logfile,message,loglev,4);
        return -4;
      }
   }
   else if(length==4)
   { 
      buf[1]=(int)(data/16777216);
      rnxt=data%16777216;
      buf[2]=(int)(rnxt/65536);
      rnxt=rnxt%65536;
      buf[3]=(int)(rnxt/256);
      buf[4]=rnxt%256;
      sprintf(message,"Send 0x%02x%02x%02x%02x%02x",buf[0],buf[1],buf[2],buf[3],buf[4]);
      logmessage(logfile,message,loglev,1); 
      if((write(fd, buf, 5)) != 5) 
      {
        sprintf(message,"Error writing to i2c slave");
        logmessage(logfile,message,loglev,4);
        return -4;
      }
   }
   else
   {
      sprintf(message,"Send 0x%02x",buf[0]);
      logmessage(logfile,message,loglev,1); 
      if((write(fd, buf, 1)) != 1) 
      {
        sprintf(message,"Error writing to i2c slave");
        logmessage(logfile,message,loglev,4);
        return -4;
      }
   }
   close(fd);
   ok=1;
  }
  
  return ok;
}

// read data with i2c from PIC, length is the number of bytes to read 
// return: -1=open failed, -2=lock failed, -3=bus access failed, 
// -4=i2c slave reading failed
int read_data(int length)
{
  int rdata=0;
  int fd,rd;
  int cnt=0;
  unsigned char buf[10];

  if((fd=open(i2cdev, O_RDWR)) < 0) 
  {
    sprintf(message,"Failed to open i2c port");
    logmessage(logfile,message,loglev,4);
    return -1;
  }

  rd=flock(fd, LOCK_EX|LOCK_NB);

  cnt=i2lockmax;
  while((rd==1)&&(cnt>0)) // try again if port locking failed
  {
    sleep(1);
    rd=flock(fd, LOCK_EX|LOCK_NB);
    cnt--;
  }
  if(rd)
  {
    sprintf(message,"Failed to lock i2c port");
    logmessage(logfile,message,loglev,4);
    return -2;
  }

  if(ioctl(fd, I2C_SLAVE, address) < 0) 
  {
    sprintf(message,"Unable to get bus access to talk to slave");
    logmessage(logfile,message,loglev,4);
    return -3;
  }

  if(length==1)
  {
     if(read(fd, buf,1)!=1) 
     {
       sprintf(message,"Unable to read from slave");
       logmessage(logfile,message,loglev,4);
       return -4;
     }
     else 
     {
       sprintf(message,"Receive 0x%02x",buf[0]);
       logmessage(logfile,message,loglev,1); 
       rdata=buf[0];
     }
  } 
  else if(length==2)
  {
     if(read(fd, buf,2)!=2) 
     {
       sprintf(message,"Unable to read from slave");
       logmessage(logfile,message,loglev,4);
       return -4;
     }
     else 
     {
       sprintf(message,"Receive 0x%02x%02x",buf[0],buf[1]);
       logmessage(logfile,message,loglev,1);  
       rdata=256*buf[0]+buf[1];
     }
  }
  else if(length==4)
  {
     if(read(fd, buf,4)!=4) 
     {
       sprintf(message,"Unable to read from slave");
       logmessage(logfile,message,loglev,4);
       return -4;
     }
     else 
     {
        sprintf(message,"Receive 0x%02x%02x%02x%02x",buf[0],buf[1],buf[2],buf[3]);
        logmessage(logfile,message,loglev,1);  
        rdata=16777216*buf[0]+65536*buf[1]+256*buf[2]+buf[3];
     }
  }

  close(fd);

  return rdata;
}

int testi2c()
{
  int ok=-1;
  int testint=0;
  int testres=1;

  srand((unsigned int)time(NULL));

  testint=rand();
  while((testint==-1)||(testint==-2)||(testint==-3)||(testint==-4))
  {
    testint=rand();
  }

  ok=write_cmd(0x02,testint,4);

  if(ok!=1)
  {
    strcpy(message,"failed to write 4 test bytes"); 
    logmessage(logfile,message,loglev,4);
  }
  else
  {
    testres=read_data(4);

    if((testres==-1)||(testres==-2)||(testres==-3)||(testres==-4))
    {
      strcpy(message,"failed to read 4 test bytes"); 
      logmessage(logfile,message,loglev,4);
    }
    else
    {
      if(testint==testres) ok=1; else ok=0;
    }
  }

  return ok;
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
    if(CHECK_BIT(gpio,4)==1) strcpy(message,"switch 1 on");
    else strcpy(message,"switch 1 off");
    logmessage(logfile,message,loglev,3);
    if(CHECK_BIT(gpio,5)==1) strcpy(message,"switch 2 on");
    else strcpy(message,"switch 2 off");
    logmessage(logfile,message,loglev,3);
  }
  else
  {
    strcpy(message,"failed to read GPIO register"); 
    logmessage(logfile,message,loglev,4);
  }

  return ok;
}

int operate_switch(int switch1, int switch2)
{
  int ok=0;

  if(switch1==1) 
  {
     ok=write_cmd(0x24,0x00,0); 
     if(ok!=1) 
     {
       sprintf(message,"initial closing switch 1 failed");
       logmessage(logfile,message,loglev,4);
     }
  }
  else if(switch1==2) 
  {
     ok=write_cmd(0x14,0x00,0); 
     if(ok!=1) 
     {
       sprintf(message,"initial opening switch 1 failed");
       logmessage(logfile,message,loglev,4);
     }
  }
  if(switch2==1) 
  {
     ok=write_cmd(0x25,0x00,0); 
     if(ok!=1) 
     {
       sprintf(message,"initial closing switch 2 failed");
       logmessage(logfile,message,loglev,4);
     }
  }
  else if(switch2==2) 
  {
     ok=write_cmd(0x15,0x00,0); 
     if(ok!=1) 
     {
       sprintf(message,"initial opening switch 2 failed");
       logmessage(logfile,message,loglev,4);
     }
  }

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
  ok=operate_switch(stopswitch1,stopswitch2);

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

  int unxs=(int)time(NULL); // unix seconds
  int nxtstatus=20+unxs; // next time to read switch status

  int i2cok=testi2c(); // test i2c data flow to PIC 
  if(i2cok==1)
  {
    strcpy(message,"i2c dataflow test ok"); 
    logmessage(logfile,message,loglev,4);
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
  }

  read_config(); // read configuration file

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

// initialize switches
  ok=operate_switch(initswitch1,initswitch2);

  while(cont==1)
  {
    unxs=(int)time(NULL); 

    if((unxs>=nxtstatus)||((nxtstatus-unxs)>statusint))
    {
      ok=read_status();
      nxtstatus=statusint+unxs;
    }
    sleep(1);
  }

  strcpy(message,"remove PID file");
  logmessage(logfile,message,loglev,4);
  ok=remove(pidfile);

  return ok;
}
