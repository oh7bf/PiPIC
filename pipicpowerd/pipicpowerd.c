/**************************************************************************
 * 
 * Monitor and control Raspberry Pi power supply using i2c bus. The power
 * supply has a PIC processor to interprete commands send by this daemon.  
 *       
 * Copyright (C) 2013 - 2014 Jaakko Koivuniemi.
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
 * Mon Sep 30 18:51:20 CEST 2013
 * Edit: Wed Feb 26 19:28:24 CET 2014
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
#include <time.h>
#include <signal.h>
#include <syslog.h>

const int version=20140226; // program version

int voltint=300; // battery voltage reading interval [s]
int buttonint=10; // button reading interval [s]
int confdelay=10; // delay to wait for confirmation [s]
int pwrdown=100; // delay to power down in PIC counter cycles
float picycle=0.445; // length of PIC counter cycles [s]
int minvolts=600; // power down if read voltage exceeds this value
int sleepint=60; // how often to check sleep file [s]
int forcereset=0; // force PIC timer reset if i2c test fails

const char i2cdev[100]="/dev/i2c-1";
const int  address=0x26;
const int  i2lockmax=10; // maximum number of times to try lock i2c port  

const char confile[200]="/etc/pipicpowerd_config";

const char wakefile[200]="/var/lib/pipicpowerd/wakeup";
const char pdownfile[200]="/var/lib/pipicpowerd/pwrdown";
const char resetfile[200]="/var/lib/pipicpowerd/resetime";
const char upfile[200]="/var/lib/pipicpowerd/waketime";
const char pwrupfile[200]="/var/lib/pipicpowerd/pwrup";
const char sleepfile[200]="/var/lib/pipicpowerd/sleeptime";

const char pidfile[200]="/var/run/pipicpowerd.pid";

int loglev=3;
const char logfile[200]="/var/log/pipicpowerd.log";
char message[200]="";

int pwroff=0; // 1==SIGTERM causes power off, 2==no power up in future 
int settime=1; // 1==set system time from PIC counter

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
          if(strncmp(par,"VOLTINT",7)==0)
          {
             voltint=(int)value;
             sprintf(message,"Voltage reading interval set to %d s",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"BUTTONINT",9)==0)
          {
             buttonint=(int)value;
             sprintf(message,"Button reading interval set to %d s",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"CONFDELAY",9)==0)
          {
             confdelay=(int)value;
             sprintf(message,"Confirmation delay set to %d s",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"PWRDOWN",7)==0)
          {
             pwrdown=(int)value;
             sprintf(message,"Delay to power down set to %d cycles",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"PICYCLE",7)==0)
          {
             picycle=value;
             sprintf(message,"PIC cycle %f s",value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"LOWBATTERY",10)==0)
          {
             minvolts=(int)value;
             sprintf(message,"Minimum voltage set to %d [1023-0]",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"SETTIME",7)==0)
          {
             if(value==1)
             {
                settime=1;
                sprintf(message,"Set system time from PIC counter in power up");
                logmessage(logfile,message,loglev,4);
             }
             else
             {
                settime=0;
                sprintf(message,"Do not set system time from PIC counter");
                logmessage(logfile,message,loglev,4);
             }
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

// read wake-up time from file and calculate how many seconds in future
int read_wakeup()
{
  int wtime=0;
  int hh=0,mm=0;
  FILE *wfile;

  time_t now;
  struct tm* tm_info;
  time(&now);
  tm_info=localtime(&now);
  int hour=tm_info->tm_hour;
  int minute=tm_info->tm_min; 

  wfile=fopen(wakefile, "r");
  if(NULL!=wfile)
  {
    if(fscanf(wfile,"%d:%d",&hh,&mm)!=EOF)
    {
      if(mm<minute)
      {
        mm+=60;
        hh--;
      }
      if(hh<hour) hh+=24;
      wtime=3600*(hh-hour)+60*(mm-minute);
    }
    fclose(wfile);
  }

  return wtime;
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


// send 4 test bytes to PiPIC and read them back 
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

// read voltage from PIC AN3
int readvolts()
{
  int volts=-1;

// set GP5=1
  if(write_cmd(0x25,0,0)!=1)
  {
    strcpy(message,"failed to set GP5=1");
    logmessage(logfile,message,loglev,4);
  }
  sleep(1);

// read AN3
  if(write_cmd(0x43,0,0)!=1)
  {
    strcpy(message,"failed send read AN3 command");
    logmessage(logfile,message,loglev,4);
  }
  else volts=read_data(2);
  sleep(1);

// read again AN3
  if(write_cmd(0x43,0,0)!=1)
  {
    strcpy(message,"failed send read AN3 command");
    logmessage(logfile,message,loglev,4);
  }
  else volts=read_data(2);
  sleep(1);

// reset GP5=0
  if(write_cmd(0x15,0,0)!=1)
  {
    strcpy(message,"failed to clear GP5=0");
    logmessage(logfile,message,loglev,4);
  }
  sleep(1);

  return volts;
}

// disable event tasks
int event_task_disable()
{
  int ok=-1;

  ok=write_cmd(0xA0,0,0);

  return ok;
}

// re-enable event tasks to detect push button
int button_powerup()
{
  int ok=-1;

  ok=write_cmd(0xA1,0,0);

  return ok;
}

// reset event register
int reset_event_register()  
{
  int ok=-1;

  ok=write_cmd(0xA3,0,0);

  return ok;
}

// check if power button has been pressed
int read_button()
{
  int ok=-1;
  int pressed=-1;

  ok=write_cmd(0xA2,0,0); // read event register
  if(ok==1) pressed=read_data(1);

  return pressed;
}

// power down after delay, optionally power up in future
int powerdown(int delay, int pwrup)  
{
  int ok;
  int wdelay=0;
  int updelay=0;

  sprintf(message,"power down after %d counts",delay);
  logmessage(logfile,message,loglev,4);

// timed task1
  ok=write_cmd(0x62,delay,4);
  ok=write_cmd(0x63,4607,2);
  ok=write_cmd(0x64,0,1);

// optional timed task2 if '/var/lib/pipicpowerd/wakeup' exists
  if(pwrup==1)
  {
    wdelay=read_wakeup();
    if(wdelay>0)
    {
      updelay=(int)(wdelay/picycle);
      sprintf(message,"power up after %d counts",updelay);
      logmessage(logfile,message,loglev,4);

      ok=write_cmd(0x72,updelay,4);
      ok=write_cmd(0x73,8703,2);
      ok=write_cmd(0x74,0,1);
      ok=write_cmd(0x71,0,0); // start task2
    }
  }
  ok=button_powerup();
  ok=write_cmd(0x61,0,0); // start task1

  return ok;
}

// read internal timer 
int read_timer()
{
  int timer=-1;

  if(write_cmd(0x51,0,0)!=1)
  {
    strcpy(message,"failed to send read timer command");
    logmessage(logfile,message,loglev,4);
  }
  else timer=read_data(4);

  return timer;
}

// reset timer
int resetimer()
{
  int ok=-1;
  time_t now;
  char tstr[25];
  struct tm* tm_info;
  FILE *timefile;

  ok=write_cmd(0x50,0,0);

  time(&now);
  tm_info=localtime(&now);
  strftime(tstr,25,"%H:%M:%S",tm_info);

  timefile=fopen(resetfile, "w");
  if(NULL==timefile)
  {
    sprintf(message,"could not write to file: %s",resetfile);
    logmessage(logfile,message,loglev,4);
  }
  else
  { 
    fprintf(timefile,"%s\n",tstr);
    fclose(timefile);
  }

  return ok;
}

// write calculated wakeup time calculated from PIC internal timer
int writeuptime(int timer)
{
  int ok=0;
  int s=(int)(timer*picycle);
  int h=(int)(s/3600);
  int m=(int)((s%3600)/60);
  char str[250];

  sprintf(str,"date --date='%d hours %d minutes' > /var/lib/pipicpowerd/waketime",h,m);
  logmessage(logfile,str,loglev,4);
  ok=system(str);

  return ok;
}

// create '/var/lib/pipicpowerd/pwrup'
int pwrupfile_create()
{
  int ok=0;
  FILE *pwrup;
  pwrup=fopen(pwrupfile, "w");
  if(NULL==pwrup)
  {
    sprintf(message,"could not create file: %s",pwrupfile);
    logmessage(logfile,message,loglev,4);
  }
  else
  { 
    fclose(pwrup);
  }

  return ok;
}

// remove '/var/lib/pipicpowerd/pwrup'
int pwrupfile_delete()
{
  int ok=0;

  ok=remove(pwrupfile);

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

  if(pwroff==1)
  {
    ok=powerdown(pwrdown,1);
    sleep(1);
    strcpy(message,"reset PIC timer");
    logmessage(logfile,message,loglev,4);
    ok=resetimer();
    ok=pwrupfile_create();
  } 
  else if(pwroff==2)
  {
    ok=powerdown(pwrdown,0);
    sleep(1);
    strcpy(message,"reset PIC timer");
    logmessage(logfile,message,loglev,4);
    ok=resetimer();
    ok=pwrupfile_create();
  }

  sleep(1);
  strcpy(message,"stop");
  logmessage(logfile,message,loglev,4);
  cont=0;
}

// shut down and power off if '/var/lib/pipicpowerd/pwrdown' exists
void hup(int sig)
{
  sprintf(message,"signal %d catched",sig);
  logmessage(logfile,message,loglev,4);

  if(access(pdownfile,F_OK)!=-1)
  {
    sprintf(message,"shut down and power off");
    logmessage(logfile,message,loglev,4);
    sleep(1);
    if(powerdown(pwrdown,1)!=1)
    {
      sprintf(message,"sending timed power down command failed");
      logmessage(logfile,message,loglev,4);
    }
    sleep(1);
    strcpy(message,"reset PIC timer");
    logmessage(logfile,message,loglev,4);
    if(resetimer()!=1)
    {
      sprintf(message,"sending timer reset command failed");
      logmessage(logfile,message,loglev,4);
    }
    sleep(1);
    cont=0;
    if(system("/sbin/shutdown -h now")==-1)
    {
      sprintf(message,"system shutdown failed");
      logmessage(logfile,message,loglev,4);
    }
  }
}

// read sleep time from file if it exists, if the time matches local time
// shutdown and power down is started
void read_sleeptime()
{
  int hh=0,mm=0,mins;
  FILE *sfile;
  time_t now;
  struct tm* tm_info;
  time(&now);
  tm_info=localtime(&now);
  int hour=tm_info->tm_hour;
  int minute=tm_info->tm_min; 
  int minutes=60*hour+minute;
  sfile=fopen(sleepfile, "r");
  if(NULL!=sfile)
  {
    if(fscanf(sfile,"%d:%d",&hh,&mm)!=EOF)
    {
      mins=60*hh+mm;
      if((minutes>=mins)&&((minutes-mins)<3))
      {
        sprintf(message,"time to go to sleep");
        logmessage(logfile,message,loglev,4);
        sprintf(message,"shut down and power off");
        logmessage(logfile,message,loglev,4);
        sleep(1);
        if(powerdown(pwrdown,1)!=1)
        {
          sprintf(message,"sending timed power down command failed");
          logmessage(logfile,message,loglev,4);
        }
        sleep(1);
        strcpy(message,"reset PIC timer");
        logmessage(logfile,message,loglev,4);
        if(resetimer()!=1)
        {
          sprintf(message,"sending timer reset command failed");
          logmessage(logfile,message,loglev,4);
        }
        sleep(1);
        cont=0;
        if(system("/sbin/shutdown -h now")==-1)
        {
          sprintf(message,"system shutdown failed");
          logmessage(logfile,message,loglev,4);
        }
      }
    }
    fclose(sfile);
  }
}


int main()
{  
  int volts=-1; // voltage reading
  int button=0; // button pressed
  int timer=0; // PIC internal timer
  int ok=0;
  char s[100];
  char wd[25],mo[25],tzone[25];
  int da,hh,mm,ss,yy;
  FILE *wakef;

  sprintf(message,"pipicpowerd v. %d started",version); 
  logmessage(logfile,message,loglev,4);

  signal(SIGINT,&stop); 
  signal(SIGKILL,&stop); 
  signal(SIGTERM,&terminate); 
  signal(SIGQUIT,&stop); 
  signal(SIGHUP,&hup); 

  int unxs=(int)time(NULL); // unix seconds
  int nxtvolts=unxs; // next time to read battery voltage
  int nxtbutton=20+unxs; // next time to check button
  int nxtsleep=60+unxs; // next time to check sleep file

  read_config(); // read configuration file

  int i2cok=testi2c(); // test i2c data flow to PIC 
  if(i2cok==1)
  {
    strcpy(message,"i2c dataflow test ok"); 
    logmessage(logfile,message,loglev,4);
  }
  else
  {
    pwrupfile_delete();
    strcpy(message,"i2c dataflow test failed");
    logmessage(logfile,message,loglev,4);
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
        }
      }
    }
    else
    {
      strcpy(message,"try to reset timer with 'pipic -a 26 -c 50'");
      logmessage(logfile,message,loglev,4); 
      strcpy(message,"and restart with 'service pipicpowerd start'");
      logmessage(logfile,message,loglev,4); 
      cont=0;
    }
  }
  if(cont==1)
  {
    strcpy(message,"disable event triggered tasks");
    logmessage(logfile,message,loglev,4); 
    if(event_task_disable()!=1)
    {
      strcpy(message,"failed to disable event triggered tasks");
      logmessage(logfile,message,loglev,4); 
    }
    sleep(1);

    strcpy(message,"reset event register");
    logmessage(logfile,message,loglev,4);
    if(reset_event_register()!=1)
    {
      strcpy(message,"failed to reset event register");
      logmessage(logfile,message,loglev,4); 
    }
    sleep(1);

    if(reset_event_register()!=1)
    {
      strcpy(message,"failed to reset event register");
      logmessage(logfile,message,loglev,4); 
    }
    sleep(1);

    strcpy(message,"disable timed task 1 and 2");
    logmessage(logfile,message,loglev,4); 
    ok=write_cmd(0x60,0,0); // disable timed task 1
    sleep(1);
    ok=write_cmd(0x70,0,0); // disable timed task 2
    sleep(1);
    timer=read_timer();
    sprintf(message,"PIC timer at %d",timer);
    logmessage(logfile,message,loglev,4);

    if(access(pwrupfile,F_OK)!=-1)
    {
      ok=writeuptime(timer);
      wakef=fopen(upfile,"r");
      if(NULL==wakef)
      {
        sprintf(message,"could not read file: %s",upfile);
        logmessage(logfile,message,loglev,4);
      }
      else
      { 
        if(fscanf(wakef,"%s %s %d %d:%d:%d %s %d",wd,mo,&da,&hh,&mm,&ss,tzone,&yy)!=EOF)
        {
          sprintf(s,"/bin/date -s '%s %s %d %02d:%02d:%02d %s %d'",wd,mo,da,hh,mm,ss,tzone,yy);
          logmessage(logfile,s,loglev,4);
          fclose(wakef);
          if(settime==1)
          {
             ok=system(s);
          }
          else
          {
             sprintf(message,"system time can be set with command above");
             logmessage(logfile,s,loglev,4);
          }
        }
      } 
      ok=pwrupfile_delete();
    } 
  }
  else
  {
    printf("start failed\n");
    exit(EXIT_FAILURE);
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

  int wtime=0;
  while(cont==1)
  {
    unxs=(int)time(NULL); 

    if(((unxs>=nxtsleep)||((nxtsleep-unxs)>sleepint))&&(pwroff==0)) 
    {
      nxtsleep=sleepint+unxs;
      read_sleeptime();
    }

    if(((unxs>=nxtvolts)||((nxtvolts-unxs)>voltint))&&(pwroff==0))
    {
      nxtvolts=voltint+unxs;
      volts=readvolts();
      sprintf(message,"read voltage %d",volts);
      logmessage(logfile,message,loglev,4);
      if(volts>minvolts)
      {
        strcpy(message,"battery voltage low, shut down and power off");
        logmessage(logfile,message,loglev,4);
        sleep(1);
        pwroff=2;
        ok=system("/sbin/shutdown -h now battery low");
      }
      sprintf(message,"unxs=%d nxtvolts=%d",unxs,nxtvolts);
      logmessage(logfile,message,loglev,2);
    }

    if(((unxs>=nxtbutton)||((nxtbutton-unxs)>buttonint))&&(pwroff==0))
    {
      button=read_button();
      nxtbutton=buttonint+unxs;
      if((button==0x01)||(button==0x81))
      {
        strcpy(message,"button pressed");
        logmessage(logfile,message,loglev,4);
        ok=write_cmd(0x25,0,0); // turn on red LED
        sleep(1);

        if(reset_event_register()!=1)
        {
          strcpy(message,"failed to reset event register");
          logmessage(logfile,message,loglev,4); 
         }
        sleep(1);

        if(reset_event_register()!=1)
        {
          strcpy(message,"failed to reset event register");
          logmessage(logfile,message,loglev,4); 
         }
        sleep(1);

        button=0;
        wtime=0;
        while((wtime<=confdelay)&&((button==0x00)||(button==0x80)))
        {
          sleep(1);
          wtime++;
          button=read_button();
          if((button==0x01)||(button==0x81))
          {
            strcpy(message,"shutdown confirmed");
            logmessage(logfile,message,loglev,4);
            sleep(1);
            pwroff=1;
            ok=system("/sbin/shutdown -h now");
          }
        }
        sleep(1); 
        ok=write_cmd(0x15,0,0); // turn off red LED
      }
      sprintf(message,"unxs=%d nxtbutton=%d",unxs,nxtbutton);
      logmessage(logfile,message,loglev,2);
    }

    sleep(1);
  }

  strcpy(message,"remove PID file");
  logmessage(logfile,message,loglev,4);
  ok=remove(pidfile);

  return ok;
}
