/**************************************************************************
 * 
 * Monitor and control Raspberry Pi power supply using i2c bus. The power
 * supply has a PIC processor to interprete commands send by this daemon.  
 *       
 * Copyright (C) 2013 Jaakko Koivuniemi.
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
 * Edit: Sun Oct 13 20:48:27 CEST 2013
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
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>

const int version=20131013; // program version
const int voltint=300; // battery voltage reading interval [s]
const int buttonint=10; // button reading interval [s]
const int confdelay=10; // delay to wait for confirmation [s]
const int pwrdown=200; // delay to power down in PIC counter cycles
const float picycle=0.462; // length of PIC counter cycles [s]
const int minvolts=600; // power down if read voltage exceeds this value

const char i2cdev[100]="/dev/i2c-1";
const int  address=0x26;

const char wakefile[200]="var/lib/pipicpowerd/wakeup";
const char pdownfile[200]="var/lib/pipicpowerd/pwrdown";
const char resetfile[200]="var/lib/pipicpowerd/resetime";
const char upfile[200]="var/lib/pipicpowerd/waketime";

const int loglev=3;
const char logfile[200]="/var/log/pipicpowerd.log";
char message[200]="";

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
int write_cmd(int cmd, int data, int length)
{
  int ok=0;
  int fd;
  int rnxt=0;
  unsigned char buf[10];

  if((cmd>=0)&&(cmd<=255))
  {
    if((fd=open(i2cdev, O_RDWR)) < 0) 
    {
      sprintf(message,"Failed to open i2c port");
      logmessage(logfile,message,loglev,4);
      return -1;
    }

    if(ioctl(fd, I2C_SLAVE, address) < 0) 
    {
      sprintf(message,"Unable to get bus access to talk to slave");
      logmessage(logfile,message,loglev,4);
      return -1;
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
        return -1;
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
        return -1;
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
        return -1;
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
        return -1;
      }
   }
   close(fd);
   ok=1;
  }
  
  return ok;
}

// read data with i2c from PIC, length is the number of bytes to read 
int read_data(int length)
{
  int rdata=0;
  int fd;
  unsigned char buf[10];

  if((fd=open(i2cdev, O_RDWR)) < 0) 
  {
    sprintf(message,"Failed to open i2c port");
    logmessage(logfile,message,loglev,4);
    return -1;
  }

  if(ioctl(fd, I2C_SLAVE, address) < 0) 
  {
    sprintf(message,"Unable to get bus access to talk to slave");
    logmessage(logfile,message,loglev,4);
    return -1;
  }

  if(length==1)
  {
     if(read(fd, buf,1)!=1) 
     {
       sprintf(message,"Unable to read from slave");
       logmessage(logfile,message,loglev,4);
       return -1;
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
       return -1;
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
       return -1;
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

  ok=write_cmd(0x02,testint,4);
  if(ok!=1)
  {
    strcpy(message,"failed to write 4 test bytes"); 
    logmessage(logfile,message,loglev,4);
  }
  else
  {
    testres=read_data(4);
    if(testres==-1)
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
  int ok=0;

// set GP5=1
  ok=write_cmd(0x25,0,0);
  sleep(1);

// read AN3
  ok=write_cmd(0x43,0,0);
  volts=read_data(2);
  sleep(1);

// read again AN3
  ok=write_cmd(0x43,0,0);
  volts=read_data(2);
  sleep(1);

// reset GP5=0
  ok=write_cmd(0x15,0,0);
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
// read event register
  ok=write_cmd(0xA2,0,0);
  pressed=read_data(1);
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
  int ok=-1;
  int timer=-1;

  ok=write_cmd(0x51,0,0);
  timer=read_data(4);

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
    fprintf(timefile,"%s",tstr);
    fclose(timefile);
  }

  return ok;
}

// write calculated wakeup time calculated from PIC internal timer
int writeuptime(int timer)
{
  int ok=0;
  int s=(int)(timer/picycle);
  int h=(int)(s/3600);
  int m=(int)((s%3600)/60);

  FILE *ufile;


  return ok;
}

int cont=1; /* main loop flag */

void stop(int sig)
{
  sprintf(message,"signal %d catched, stop",sig);
  logmessage(logfile,message,loglev,4);
  cont=0;
}

// shut down and power off if '/var/lib/pipicpowerd/pwrdown' exists
void hup(int sig)
{
  int ok=0;

  sprintf(message,"signal %d catched",sig);
  logmessage(logfile,message,loglev,4);

  if(access(pdownfile,F_OK)!=-1)
  {
    sprintf(message,"shut down and power off");
    logmessage(logfile,message,loglev,4);
    sleep(1);
    ok=powerdown(pwrdown,1);
    sleep(1);
    strcpy(message,"reset PIC timer");
    logmessage(logfile,message,loglev,4);
    ok=resetimer();
    sleep(1);
    cont=0;
    ok=system("/sbin/shutdown -h now");
  }
}


int main()
{  

  int volts=-1; // voltage reading
  int button=0; // button pressed
  int timer=0; // PIC internal timer
  int ok=0;

  sprintf(message,"pipicpowerd v. %d started",version); 
  logmessage(logfile,message,loglev,4);

  signal(SIGINT,&stop); 
  signal(SIGKILL,&stop); 
  signal(SIGTERM,&stop); 
  signal(SIGQUIT,&stop); 
  signal(SIGHUP,&hup); 

  int unxs=(int)time(NULL); // unix seconds
  int nxtvolts=unxs; // next time to read battery voltage
  int nxtbutton=20+unxs; // next time to check button

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
    strcpy(message,"try to reset timer with 'pipic -a 26 -c 50'");
    logmessage(logfile,message,loglev,4); 
    cont=0;
  }
  if(cont==1)
  {
    strcpy(message,"disable event triggered tasks");
    logmessage(logfile,message,loglev,4); 
    ok=event_task_disable();
    strcpy(message,"reset event register");
    logmessage(logfile,message,loglev,4);
    ok=reset_event_register(); 
    sleep(1);
    ok=reset_event_register(); 
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
    ok=writeuptime(timer); 
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
        
  int wtime=0;
  while(cont==1)
  {
    unxs=(int)time(NULL); 
    if(unxs>=nxtvolts)
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
        ok=powerdown(pwrdown,0);
        sleep(1);
        strcpy(message,"reset PIC timer");
        logmessage(logfile,message,loglev,4);
        ok=resetimer();
        sleep(1);
        cont=0;
        ok=system("/sbin/shutdown -h now battery low");
      }
      sprintf(message,"unxs=%d nxtvolts=%d",unxs,nxtvolts);
      logmessage(logfile,message,loglev,2);
    }

    if(unxs>=nxtbutton)
    {
      button=read_button();
      nxtbutton=buttonint+unxs;
      if(button==1)
      {
        strcpy(message,"button pressed");
        logmessage(logfile,message,loglev,4);
        ok=write_cmd(0x25,0,0);
        sleep(1);
        ok=reset_event_register();
        sleep(1);
        ok=reset_event_register();
        sleep(1);
        button=0;
        wtime=0;
        while((wtime<=confdelay)&&(button==0))
        {
          sleep(1);
          wtime++;
          button=read_button();
          if(button==1)
          {
            strcpy(message,"shutdown confirmed");
            logmessage(logfile,message,loglev,4);
            sleep(1);
            ok=powerdown(pwrdown,1);
            sleep(1);
            strcpy(message,"reset PIC timer");
            logmessage(logfile,message,loglev,4);
            ok=resetimer();
            sleep(1);
            cont=0;
            ok=system("/sbin/shutdown -h now");
          }
        }
        sleep(1); 
        ok=write_cmd(0x15,0,0);
      }
      sprintf(message,"unxs=%d nxtbutton=%d",unxs,nxtbutton);
      logmessage(logfile,message,loglev,2);
    }

    sleep(1);
  }

  return 0;
}
