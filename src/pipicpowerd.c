/**************************************************************************
 * 
 * Monitor and control Raspberry Pi power supply using i2c bus. The power
 * supply has a PIC processor to interprete commands send by this daemon.  
 *       
 * Copyright (C) 2013 - 2015 Jaakko Koivuniemi.
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
 * Edit: Fri Feb 20 21:42:00 CET 2015
 *
 * Jaakko Koivuniemi
 **/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
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
#include "pipicpowerd.h"
#include "writecmd.h"
#include "readdata.h"
#include "testi2c.h"

const int version=20150220; // program version

int voltint=300; // battery voltage reading interval [s]
int buttonint=10; // button reading interval [s]
int confdelay=10; // delay to wait for confirmation [s]
int pwrdown=100; // delay to power down in PIC counter cycles
float picycle=0.445; // length of PIC counter cycles [s]
int countint=0; // PIC counter reading interval [s] 
int wifint=0; // WiFi checking interval [s]
int wifitimeout=3600; // time out before any action is taken [s]
int wifiact=0; // WiFi down action: 0=nothing, 1=downup, 2=reboot, 3=pwr cycle
int minvolts=550; // power down if read voltage exceeds this value
float minbattlev=50; // power down if battery charge less than this value [%]
float maxbattvolts=14.4; // maximum battery voltage [V]
float voltcal=0.0216449; // voltage calibration constant
float volttempa=0; // temperature dependent voltage calibration 
float volttempb=0; // temperature dependent voltage calibration 
float volttempc=0; // temperature dependent voltage calibration 
float vdrop=0; // voltage drop between battery and power supply [V]
float battcap=7; // nominal battery capacity [Ah]
const float pkfact=1.1; // Peukert's law exponent
const float phours=20; // Peukert's law discharge time for nominal capacity [h]
float current=0.15; // estimated average current used [A]
int battfull=0; // battery is full 100 %
int solarpwr=0; // solar panel is used for charging
int sleepint=60; // how often to check sleep file [s]
int pdownint=60; // how often to check cyclic power up file [s]
int downmins=10; // minutes for cyclic power down
int forcereset=0; // force PIC timer reset if i2c test fails
int forceoff=0; // force power off after give PIC counter cycles
int forceon=0; // force power up after give PIC counter cycles

const char *i2cdev="/dev/i2c-1";
const int  address=0x26;
const int  i2lockmax=10; // maximum number of times to try lock i2c port  

const char confile[200]="/etc/pipicpowerd_config";

const char wakefile[200]="/var/lib/pipicpowerd/wakeup";
const char pdownfile[200]="/var/lib/pipicpowerd/pwrdown";
const char resetfile[200]="/var/lib/pipicpowerd/resetime";
const char upfile[200]="/var/lib/pipicpowerd/waketime";
const char pwrupfile[200]="/var/lib/pipicpowerd/pwrup";
const char sleepfile[200]="/var/lib/pipicpowerd/sleeptime";
const char batteryfile[200]="/var/lib/pipicpowerd/battery";
const char voltfile[200]="/var/lib/pipicpowerd/volts";
const char batterytime[200]="/var/lib/pipicpowerd/hoursleft";
const char batterylevel[200]="/var/lib/pipicpowerd/battlevel";
const char operationtime[200]="/var/lib/pipicpowerd/ophours";
const char timerfile[200]="/var/lib/pipicpowerd/timer";
const char tempfile[200]="/var/lib/tmp102d/temperature";
const char puptimefile[200]="/var/lib/pipicpowerd/puptime";
const char pdowntimefile[200]="/var/lib/pipicpowerd/pdowntime";
const char cputempfile[200]="/sys/class/thermal/thermal_zone0/temp";
const char wifistate[200]="/sys/class/net/wlan0/operstate";

const char pidfile[200]="/var/run/pipicpowerd.pid";

int loglev=5;
char message[200]="";
int logstats=0;
const char statfile[200]="/var/log/pipicpowers.log";

// optional scripts to execute at power up or power down
const char atpwrup[200]="/usr/sbin/atpwrup";
const char atpwrdown[200]="/usr/sbin/atpwrdown";

int pwroff=0; // 1==SIGTERM causes power off, 2==no power up in future, 3==power cycle, 4==power cycle with 'downmins'
int settime=1; // 1==set system time from PIC counter

// write usage statistics
void writestat(const char statfile[200], unsigned unxstart, unsigned unxstop, int timerstart, int timerstop, float Vmin, float Vave, float Vmax, float Tmin, float Tave, float Tmax, float Tcpumin, float Tcpuave, float Tcpumax, int wifiuptime)
{
  time_t now;
  char tstr[25];
  struct tm* tm_info;
  FILE *sfile;

  time(&now);
  tm_info=localtime(&now);
  strftime(tstr,25,"%Y-%m-%d %H:%M:%S",tm_info);

  int dt=(int)(picycle*(timerstop-timerstart));
  sfile=fopen(statfile, "a");
  if(NULL==sfile)
  {
      perror("could not open statistics file");
  }
  else
  { 
      fprintf(sfile,"%s %u %u",tstr,unxstart,unxstop);
      fprintf(sfile," %d %d %d",timerstart,timerstop,dt);
      fprintf(sfile," %5.2f %5.2f %5.2f",Vmin,Vave,Vmax);
      fprintf(sfile," %+5.2f %+5.2f %+5.2f",Tmin,Tave,Tmax);
      fprintf(sfile," %+5.2f %+5.2f %+5.2f",Tcpumin,Tcpuave,Tcpumax);
      fprintf(sfile," %d\n",wifiuptime);
      fclose(sfile);
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
    syslog(LOG_INFO, "Read configuration file");

    while((read=getline(&line,&len,cfile))!=-1)
    {
       if(sscanf(line,"%s %f",par,&value)!=EOF)
       {
          if(strncmp(par,"LOGLEVEL",8)==0)
          {
             loglev=(int)value;
             sprintf(message,"Log level set to %d",(int)value);
             syslog(LOG_INFO, "%s", message);
             setlogmask(LOG_UPTO (loglev));
          }
          if(strncmp(par,"LOGSTAT",7)==0)
          {
             logstats=(int)value;
             if(value==1) syslog(LOG_INFO, "Log statistics to 'pipicpowers.log'");
          }
          if(strncmp(par,"VOLTINT",7)==0)
          {
             voltint=(int)value;
             sprintf(message,"Voltage reading interval set to %d s",(int)value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"VOLTCAL",7)==0)
          {
             voltcal=value;
             sprintf(message,"Voltage calibration constant set to %f",value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"VOLTTEMPA",9)==0)
          {
             volttempa=value;
             sprintf(message,"Voltage temperature non-linear set to %e",value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"VOLTTEMPB",9)==0)
          {
             volttempb=value;
             sprintf(message,"Voltage temperature coefficient set to %f",value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"VOLTTEMPC",9)==0)
          {
             volttempc=value;
             sprintf(message,"Voltage temperature constant set to %f",value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"VDROP",5)==0)
          {
             vdrop=value;
             sprintf(message,"Voltage drop set to %f",value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"BUTTONINT",9)==0)
          {
             buttonint=(int)value;
             sprintf(message,"Button reading interval set to %d s",(int)value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"CONFDELAY",9)==0)
          {
             confdelay=(int)value;
             sprintf(message,"Confirmation delay set to %d s",(int)value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"PWRDOWN",7)==0)
          {
             pwrdown=(int)value;
             sprintf(message,"Delay to power down set to %d cycles",(int)value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"PICYCLE",7)==0)
          {
             picycle=value;
             sprintf(message,"PIC cycle %f s",value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"COUNTINT",8)==0)
          {
             countint=(int)value;
             sprintf(message,"PIC counter reading interval %d s",(int)value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"WIFINT",6)==0)
          {
             wifint=(int)value;
             sprintf(message,"WiFi check interval %d s",(int)value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"WIFITIMEOUT",11)==0)
          {
             wifitimeout=(int)value;
             sprintf(message,"WiFi timeout %d s",(int)value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"WIFIACT",7)==0)
          {
             wifiact=(int)value;
             if(value==0) sprintf(message,"Do nothing if WiFi down");
             else if(value==1) sprintf(message,"Interface down-up"); 
             else if(value==2) sprintf(message,"Reboot if WiFi down");
             else if(value==2) sprintf(message,"Power cycle if WiFi down"); 
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"FORCEPOWEROFF",13)==0)
          {
             forceoff=(int)value;
             if(forceoff>0)
             {
               sprintf(message,"Force power off after %d PIC cycles",(int)value);
               syslog(LOG_INFO, "%s", message);
             }
          }
          if(strncmp(par,"FORCEPOWERUP",12)==0)
          {
             forceon=(int)value;
             if(forceon>0)
             {
               sprintf(message,"Force power up after %d PIC cycles",(int)value);
               syslog(LOG_INFO, "%s", message);
             }
          }
          if(strncmp(par,"LOWBATTERY",10)==0)
          {
             minvolts=(int)value;
             sprintf(message,"Minimum voltage set to %d [1023-0]",(int)value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"BATTCAP",7)==0)
          {
             battcap=value;
             sprintf(message,"Nominal battery capacity  %f",value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"CURRENT",7)==0)
          {
             current=value;
             sprintf(message,"Current consumption  %f",value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"MINBATTLEVEL",12)==0)
          {
             minbattlev=value;
             sprintf(message,"Minimum battery level for operating %f %%",value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"MAXBATTVOLTS",12)==0)
          {
             maxbattvolts=value;
             sprintf(message,"Maximum safe charging voltage %f V",value);
             syslog(LOG_INFO, "%s", message);
          }
          if(strncmp(par,"SOLARPOWER",10)==0)
          {
             solarpwr=(int)value;
             if(solarpwr==1) syslog(LOG_INFO, "Using solar power to charge battery");
          }
          if(strncmp(par,"SETTIME",7)==0)
          {
             if(value==1)
             {
                settime=1;
                syslog(LOG_INFO, "Set system time from PIC counter in power up");
             }
             else
             {
                settime=0;
                syslog(LOG_INFO, "Do not set system time from PIC counter");
             }
          }
          if(strncmp(par,"FORCERESET",10)==0)
          {
             if(value==1)
             {
                forcereset=1;
                syslog(LOG_INFO, "Force PIC timer reset at start if i2c test fails");
             }
             else
             {
                forcereset=0;
                syslog(LOG_INFO, "Exit in case of i2c test failure");
             }
          }

       }
    }
    fclose(cfile);
  }
  else
  {
    sprintf(message, "Could not open %s", confile);
    syslog(LOG_ERR, "%s", message);
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

// read voltage from PIC AN3
int readvolts()
{
  int volts=-1;

// set GP5=1
  if(write_cmd(0x25,0,0)!=1) syslog(LOG_ERR, "failed to set GP5=1");
  sleep(1);

// read AN3
  if(write_cmd(0x43,0,0)!=1) syslog(LOG_ERR, "failed send read AN3 command");
  else volts=read_data(2);
  sleep(1);

// read again AN3
  if(write_cmd(0x43,0,0)!=1) syslog(LOG_ERR, "failed send read AN3 command");
  else volts=read_data(2);
  sleep(1);

// reset GP5=0
  if(write_cmd(0x15,0,0)!=1) syslog(LOG_ERR, "failed to clear GP5=0");
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
  syslog(LOG_WARNING, "%s", message);

// timed task1
  ok=write_cmd(0x62,delay,4);
  ok*=write_cmd(0x63,4607,2);
  ok*=write_cmd(0x64,0,1);

// optional timed task2 if '/var/lib/pipicpowerd/wakeup' exists
  if(pwrup==1)
  {
    wdelay=read_wakeup();
    if(wdelay>0)
    {
      updelay=(int)(wdelay/picycle);
      sprintf(message,"power up after %d counts",updelay);
      syslog(LOG_WARNING, "%s", message);
      ok*=write_cmd(0x72,updelay,4);
      ok*=write_cmd(0x73,8703,2);
      ok*=write_cmd(0x74,0,1);
      ok*=write_cmd(0x71,0,0); // start task2
    }
  }
  else if(pwrup>0) // watch dog power up for reboot
  {
    sprintf(message,"power up after %d counts",pwrup);
    syslog(LOG_WARNING, "%s", message);
    ok*=write_cmd(0x72,pwrup,4);
    ok*=write_cmd(0x73,8703,2);
    ok*=write_cmd(0x74,0,1);
    ok*=write_cmd(0x71,0,0); // start task2
  }

  ok*=button_powerup();
  ok*=write_cmd(0x61,0,0); // start task1

  return ok;
}

// read internal timer 
int read_timer()
{
  int timer=-1;

  if(write_cmd(0x51,0,0)!=1) 
    syslog(LOG_ERR, "failed to send read timer command");
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
    syslog(LOG_ERR, "%s", message);
  }
  else
  { 
    fprintf(timefile,"%s\n",tstr);
    fclose(timefile);
  }

  return ok;
}

// write timer to file
void write_timer(int timer)
{
  FILE *tfile;
  tfile=fopen(timerfile, "w");
  if(NULL==tfile)
  {
    sprintf(message,"could not write file: %s",timerfile);
    syslog(LOG_ERR, "%s", message);
  }
  else
  { 
    fprintf(tfile,"%d\n",timer);
    fclose(tfile);
  }
}

// read timer file
int read_timer_file()
{
  int timer=0;

  FILE *tfile;
  tfile=fopen(timerfile, "r");
  if(NULL!=tfile)
  {
    if(fscanf(tfile,"%d",&timer)==EOF)
    {
      sprintf(message,"reading %s failed",timerfile);
      syslog(LOG_ERR, "%s", message);
    }
  }

  return timer;
}

// test if ntp is running
int test_ntp()
{
  int ntpruns=0;

  char str[250]="/usr/bin/ntpq -p > /tmp/pipicpowerd_ntp_test";
  int ok=system(str);

  FILE *nfile;
  char *line=NULL;
  char p1[200],p2[200],p3[200],p4[200],p5[200],p6[200],p7[200],p8[200],p9[200],p10[200];
  size_t len;
  ssize_t read;

  nfile=fopen("/tmp/pipicpowerd_ntp_test", "r");
  if((NULL!=nfile)&&(ok!=-1))
  {
    if((read=getline(&line,&len,nfile))!=-1)
    {
      if(sscanf(line,"%s %s %s %s %s %s %s %s %s %s",p1,p2,p3,p4,p5,p6,p7,p8,p9,p10)!=EOF)
      {
        if((strncmp(p1,"remote",6)==0)&&(strncmp(p2,"refid",5)==0))
        {
          if((read=getline(&line,&len,nfile))!=-1)
          {
            if((read=getline(&line,&len,nfile))!=-1)
            {
              if(sscanf(line,"%s %s %s %s %s %s %s %s %s %s",p1,p2,p3,p4,p5,p6,p7,p8,p9,p10)!=EOF)
              {
                ntpruns=1;
                syslog(LOG_INFO, "%s", line);
              }
            }
          }
        }
      }
    }
    fclose(nfile);
  }

  if(ntpruns==0)
    syslog(LOG_INFO, "test 'ntpq -p > /tmp/pipicpowerd_ntp_test' failed");
  else
    syslog(LOG_INFO, "test 'ntpq -p > /tmp/pipicpowerd_ntp_test' success");

  return ntpruns;
}

// write calculated wakeup time calculated from PIC internal timer
int writeuptime(int timer)
{
  int ok=0;
  int timer0=read_timer_file(); 
  int s=(int)((timer-timer0)*picycle);
  int h=(int)(s/3600);
  int m=(int)((s%3600)/60);
  char str[250];

  if(timer<timer0)
  {
    h=0;
    m=0;
    sprintf(str,"timer0=%d has higher value than timer=%d!",timer0,timer);
    syslog(LOG_ERR, "%s", str);
    syslog(LOG_ERR, "force hours=0 and minutes=0");
  }
  sprintf(str,"date --date='%d hours %d minutes' > /var/lib/pipicpowerd/waketime",h,m);
  syslog(LOG_DEBUG, "%s", str);
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
    syslog(LOG_ERR, "%s", message);
  }
  else
  { 
    fclose(pwrup);
    ok=1;
    sprintf(message,"created file: %s",pwrupfile);
    syslog(LOG_NOTICE, "%s", message);
  }
  return ok;
}

// remove '/var/lib/pipicpowerd/pwrup'
int pwrupfile_delete()
{
  int ok=0;

  ok=remove(pwrupfile);
  sprintf(message,"removed file: %s",pwrupfile);
  syslog(LOG_NOTICE, "%s", message);

  return ok;
}

int cont=1; /* main loop flag */

void stop(int sig)
{
  sprintf(message,"signal %d catched, stop",sig);
  syslog(LOG_NOTICE, "%s", message);
  cont=0;
}

void terminate(int sig)
{
  int ok=0;
  int timer=0;

  sprintf(message,"signal %d catched",sig);
  syslog(LOG_NOTICE, "%s", message);

  if(pwroff==1)
  {
    ok=powerdown(pwrdown,1);
    if(ok!=1) syslog(LOG_ERR, "problem in i2c communication");
    sleep(1);
    syslog(LOG_NOTICE, "save PIC timer value to file");
    timer=read_timer(); 
    write_timer(timer);// save last PIC timer value to file 
    ok=pwrupfile_create();
  } 
  else if(pwroff==2)
  {
    ok=powerdown(pwrdown,0);
    if(ok!=1) syslog(LOG_ERR, "problem in i2c communication");
    sleep(1);
    syslog(LOG_NOTICE, "save PIC timer value to file");
    timer=read_timer();
    write_timer(timer); // save last PIC timer value to file
    ok=pwrupfile_create();
  }
  else if(pwroff==3)
  {
    ok=powerdown(pwrdown,pwrdown+60);
    sleep(1);
    syslog(LOG_NOTICE, "save PIC timer value to file");
    timer=read_timer();
    write_timer(timer); // save last PIC timer value to file
    ok=pwrupfile_create();
  }
  else if(pwroff==4)
  {
    ok=powerdown(pwrdown,pwrdown+downmins*60/picycle);
    if(ok!=1) syslog(LOG_ERR, "problem in i2c communication");
    sleep(1);
    syslog(LOG_NOTICE, "save PIC timer value to file");
    timer=read_timer();
    write_timer(timer); // save last PIC timer value to file
    ok=pwrupfile_create();
  }

  sleep(1);
  syslog(LOG_NOTICE, "stop");
  cont=0;
}

// shut down and power off if '/var/lib/pipicpowerd/pwrdown' exists
void hup(int sig)
{
  int timer=0;

  sprintf(message,"signal %d catched",sig);
  syslog(LOG_NOTICE, "%s", message);

  if(access(pdownfile,F_OK)!=-1)
  {
    syslog(LOG_NOTICE, "shut down and power off");
    sleep(1);
    if(powerdown(pwrdown,1)!=1)
      syslog(LOG_ERR, "sending timed power down command failed");
    sleep(1);

    syslog(LOG_NOTICE, "save PIC timer value to file");
    timer=read_timer();
    write_timer(timer); // save last PIC timer value to file

    sleep(1);
    if(pwrupfile_create()!=1)
      syslog(LOG_ERR, "failed to create 'pwrup' file");
    cont=0;
    if(system("/bin/sync")==-1)
      syslog(LOG_ERR, "sync to disk failed");
    if(system("/sbin/shutdown -h now")==-1)
      syslog(LOG_ERR, "system shutdown failed");
  }
}

// read sleep time from file if it exists, if the time matches local time
// shutdown and power down is started
int read_sleeptime()
{
  int sleepnow=0;
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
      if((minutes>=mins)&&((minutes-mins)<3)) sleepnow=1;
    }
    fclose(sfile);
  }

  return sleepnow;
}

// read maximum power up time from file if it exists and compare to uptime
// calculated from PIC counter, this is used for cyclic operation
int read_puptime(int timerstart)
{
  int sleepnow=0;
  int mins=0;
  int timer=0;
  FILE *pfile;

  pfile=fopen(puptimefile, "r");
  if(NULL!=pfile)
  {
    if(fscanf(pfile,"%d",&mins)!=EOF)
    {
      if(mins>2)
      {
        timer=read_timer();      
        if((((timer-timerstart)*picycle)/60-1)>mins) sleepnow=1;
      }
    }
    fclose(pfile);
  }

  return sleepnow;
}

// read power down time in minutes from file for cyclic operation
int read_pdowntime()
{
  int mins=2;
  FILE *pfile;

  pfile=fopen(pdowntimefile, "r");
  if(NULL!=pfile)
  {
    if(fscanf(pfile,"%d",&mins)!=EOF)
    {
      if(mins<2) mins=2;
    }
    fclose(pfile);
  }

  return mins;
}

// write '/var/lib/pipicpowerd/battery', '/var/lib/pipicpowerd/volts'
// and '/var/lib/pipicpowerd/hoursleft'
int write_battery(int b, float v, float h, float t, float l)
{
  int ok=0;
  FILE *bfile;
  bfile=fopen(batteryfile, "w");
  if(NULL==bfile)
  {
    sprintf(message,"could not write file: %s",batteryfile);
    syslog(LOG_ERR, "%s", message);
  }
  else
  { 
    fprintf(bfile,"%d",b);
    fclose(bfile);
  }

  FILE *vfile;
  vfile=fopen(voltfile, "w");
  if(NULL==vfile)
  {
    sprintf(message,"could not write file: %s",voltfile);
    syslog(LOG_ERR, "%s", message);
  }
  else
  { 
    fprintf(vfile,"%4.1f",v);
    fclose(vfile);
  }

  FILE *hfile;
  hfile=fopen(batterytime, "w");
  if(NULL==hfile)
  {
    sprintf(message,"could not write file: %s",batterytime);
    syslog(LOG_ERR, "%s", message);
  }
  else
  { 
    fprintf(hfile,"%4.1f",h);
    fclose(hfile);
  }

  FILE *tfile;
  tfile=fopen(operationtime, "w");
  if(NULL==tfile)
  {
    sprintf(message,"could not write file: %s",operationtime);
    syslog(LOG_ERR, "%s", message);
  }
  else
  { 
    fprintf(tfile,"%4.1f",t);
    fclose(tfile);
  }

  FILE *lfile;
  lfile=fopen(batterylevel, "w");
  if(NULL==lfile)
  {
    sprintf(message,"could not write file: %s",batterylevel);
    syslog(LOG_ERR, "%s", message);
  }
  else
  { 
    fprintf(lfile,"%4.1f",l);
    fclose(lfile);
  }

  return ok;
}

// read CPU temperature from file
float readcputemp()
{
  float temp=-100;
  FILE *tfile;
  tfile=fopen(cputempfile, "r");
  if(NULL==tfile)
    syslog(LOG_ERR, "could not read file: %s", cputempfile);
  else
  { 
    if(fscanf(tfile,"%f",&temp)==EOF) temp=-100000;
    temp/=1000;
    fclose(tfile);
  }
  return temp;
}

// read ambient temperature from file
float readtemp()
{
  float temp=-100;
  FILE *tfile;
  tfile=fopen(tempfile, "r");
  if(NULL==tfile)
    syslog(LOG_ERR, "could not read file: %s", tempfile);
  else
  { 
    if(fscanf(tfile,"%f",&temp)==EOF) temp=-100;
    fclose(tfile);
  }
  return temp;
}

// capacity   voltage[V]
// 100%       12.7
//  90%       12.5
//  80%       12.42
//  70%       12.32
//  60%       12.20
//  50%       12.06
//  40%       11.9
//  30%       11.75
//  20%       11.58
//  10%       11.31
//   0%       10.5
float battlevel(float volts)
{
  float level=0;

  if(volts>=12.7) level=100;
  else if(volts>=12.5) level=10*(volts-12.5)/0.2+90;
  else if(volts>=12.42) level=10*(volts-12.42)/0.08+80;
  else if(volts>=12.32) level=10*(volts-12.32)/0.1+70;
  else if(volts>=12.20) level=10*(volts-12.20)/0.12+60;
  else if(volts>=12.06) level=10*(volts-12.06)/0.14+50;
  else if(volts>=11.9) level=10*(volts-11.9)/0.16+40;
  else if(volts>=11.75) level=10*(volts-11.75)/0.15+30;
  else if(volts>=11.58) level=10*(volts-11.58)/0.17+20;
  else if(volts>=11.31) level=10*(volts-11.31)/0.27+10;
  else if(volts>=10.5) level=10*(volts-10.5)/0.81;

  return level;
}

// estimate time remaining before battery is empty
// http://en.wikipedia.org/wiki/Peukert's_law
float battime(float level, float battcap, float pkfact, float phours, float current)
{
  float hours=0;
  float capleft=level*battcap/100;
  float base=0;

  if(current>0) base=capleft/(current*phours);
  float expo=pkfact-1.0;
  if(current>0) hours=capleft*powf(base,expo)/current;

  return hours;
}

// estimate time remaining before battery level is too low, operation
// can be continued after this time but battery life will be less if this
// is done often
// http://en.wikipedia.org/wiki/Peukert's_law
float optime(float level, float minbattlev, float battcap, float pkfact, float phours, float current)
{
  float hours=0;
  float capleft=0;

  if(level>=minbattlev) capleft=(level-minbattlev)*battcap/100;

  float base=0;
  if(current>0) base=capleft/(current*phours);

  float expo=pkfact-1.0;
  if(current>0) hours=capleft*powf(base,expo)/current;

  return hours;
}

// read WiFi operation state file
int read_wifi()
{
  int wifiup=0;
  char state[200]="";

  FILE *wfile;
  wfile=fopen(wifistate, "r");
  if(NULL==wfile)
    syslog(LOG_ERR, "could not read file: %s",wifistate);
  else
  { 
    if(fscanf(wfile,"%s",state)!=EOF) 
    {
      if(strncmp(state,"up",2)==0) wifiup=1;
      else if(strncmp(state,"down",4)==0) wifiup=-1;
    }
    fclose(wfile);
  }

  return wifiup;
}


int main()
{  
  int volts=-1; // voltage reading
  float voltsV=0; // conversion to Volts
  float battlev=0; // battery level [%]
  float batim=0; // hours left with battery
  float ophours=0; // hours left before recommended low charge level reached
  float temp=-100; // ambient temperature [C]
  float cputemp=-100; // CPU temperature
  float Vmin=100; // minimum voltage for statistics
  float Vmax=-100; // maximun voltage for statistics
  float Vave=0; // average voltage
  float Vaven=0; // number of samples to calculate average voltage
  float Tmin=100; // minimum temperature for statistics
  float Tmax=-100; // maximun temperature for statistics
  float Tave=0; // average temperature
  float Taven=0; // number of samples to calculate average temperature
  float Tcpumin=100; // minimum CPU temperature for statistics
  float Tcpumax=-100; // maximun CPU temperature for statistics
  float Tcpuave=0; // average CPU temperature
  float Tcpuaven=0; // number of samples to calculate average CPU temperature

  int button=0; // button pressed
  int timer=0; // PIC internal timer
  int ntpok=0; // does the ntpd seem to be running?
  int wifiup=0; // WiFi 0=unknown,-1=down, +1=up
  int ok=0;
  char s[100];
  char wd[25],mo[25],tzone[25];
  int da,hh,mm,ss,yy;
  FILE *wakef;

  setlogmask(LOG_UPTO (loglev));
  syslog(LOG_NOTICE, "pipicpowerd v. %d started", version);

  signal(SIGINT,&stop); 
  signal(SIGKILL,&stop); 
  signal(SIGTERM,&terminate); 
  signal(SIGQUIT,&stop); 
  signal(SIGHUP,&hup); 

  unsigned unxs=(int)time(NULL); // unix seconds
  unsigned nxtvolts=unxs; // next time to read battery voltage
  unsigned nxtstart=15+unxs; // next time to read start time
  unsigned nxtbutton=20+unxs; // next time to check button
  unsigned nxtpdown=30+unxs; // next time to check cyclic power down
  unsigned nxtsleep=60+unxs; // next time to check sleep file
  unsigned nxtcounter=300+unxs; // next time to read PIC counter

  int timerstart=0; // first timer value from PIC

  read_config(); // read configuration file

  unsigned nxtwifi=wifint+unxs; // next time to check WiFi status

  int i2cok=testi2c(); // test i2c data flow to PIC 
  if(i2cok==1) syslog(LOG_NOTICE, "PIC i2c dataflow test ok");
  else
  {
    pwrupfile_delete();
    syslog(LOG_NOTICE, "PIC i2c dataflow test failed");
    if(forcereset==1)
    {
      sleep(1); 
      syslog(LOG_NOTICE, "try to reset PIC timer now");
      ok=resetimer();
      sleep(1);
      if(resetimer()!=1)
      {
        syslog(LOG_ERR, "failed to reset PIC timer");
        cont=0; 
      }
      else
      {
        sleep(1);
        i2cok=testi2c(); // test i2c data flow to PIC 
        if(i2cok==1) syslog(LOG_NOTICE, "PIC i2c dataflow test ok");
        else
        {
          syslog(LOG_ERR, "PIC i2c dataflow test failed");
          cont=0;
        }
      }
    }
    else
    {
      syslog(LOG_ERR, "try to reset timer with 'pipic -a 26 -c 50' and restart with 'service pipicpowerd start'");
      cont=0;
    }
  }
  if(cont==1)
  {
    syslog(LOG_NOTICE, "disable PIC event triggered tasks");
    if(event_task_disable()!=1) syslog(LOG_ERR, "failed to disable PIC event triggered tasks");
    sleep(1);

    syslog(LOG_NOTICE, "reset PIC event register");
    if(reset_event_register()!=1)
      syslog(LOG_ERR, "failed to reset PIC event register");
    sleep(1);

    if(reset_event_register()!=1)
      syslog(LOG_ERR, "failed to reset PIC event register");
    sleep(1);

    syslog(LOG_NOTICE, "disable PIC timed task 1 and 2");
    ok=write_cmd(0x60,0,0); // disable timed task 1
    sleep(1);
    ok=write_cmd(0x70,0,0); // disable timed task 2
    sleep(1);
    timer=read_timer();
    timerstart=timer;
    syslog(LOG_INFO, "PIC timer at %d",timer);

    ntpok=test_ntp();
    if((access(pwrupfile,F_OK)!=-1)&&(ntpok==0))
    {
      ok=writeuptime(timer);
      wakef=fopen(upfile,"r");
      if(NULL==wakef)
      {
        sprintf(message,"could not read file: %s",upfile);
        syslog(LOG_ERR, "%s", message);
      }
      else
      { 
        if(fscanf(wakef,"%s %s %d %d:%d:%d %s %d",wd,mo,&da,&hh,&mm,&ss,tzone,&yy)!=EOF)
        {
          sprintf(s,"/bin/date -s '%s %s %d %02d:%02d:%02d %s %d'",wd,mo,da,hh,mm,ss,tzone,yy);
          syslog(LOG_DEBUG, "%s", s);
          fclose(wakef);
          if(settime==1)
             ok=system(s);
          else
             syslog(LOG_INFO, "system time can be set with command above");
        }
      } 
    }
    else if(access(pwrupfile,F_OK)==-1)
      syslog(LOG_NOTICE, "not power up, leave time untouched");
    
    if(access(pwrupfile,F_OK)!=-1)
    {
      ok=pwrupfile_delete();
      if(access(atpwrup,X_OK)!=-1)
      {
        sprintf(message,"execute power up script %s",atpwrup);
        syslog(LOG_NOTICE, "%s", message);
        ok=system(atpwrup);
      }
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
    syslog(LOG_ERR, "failed to create child process"); 
    exit(EXIT_FAILURE);
  }
        
  if((chdir("/")) < 0) 
  {
    syslog(LOG_ERR, "failed to change to root directory"); 
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
    syslog(LOG_ERR, "%s", message);
    exit(EXIT_FAILURE);
  }

  if(flock(fileno(pidf),LOCK_EX||LOCK_NB)==-1)
  {
    sprintf(message,"Could not lock PID lock file %s, exiting", pidfile);
    syslog(LOG_ERR, "%s", message);
    exit(EXIT_FAILURE);
  }

  fprintf(pidf,"%d\n",getpid());
  fclose(pidf);

  if(forceoff>0)
  {
    syslog(LOG_NOTICE, "forced power off active");
    ok=powerdown(forceoff,forceon);
  }

  unsigned unxstart=time(NULL); // for power up statistics
  unsigned unxstop=0;

  int wifidown=0;
  int wifiuptime=0;
  int wtime=0;
  while(cont==1)
  {
    unxs=(int)time(NULL); 

    if((unxs>=nxtstart)&&(nxtstart>0))
    {
       unxstart=time(NULL); 
       unxstart-=15;
       nxtstart=0;
    }

    if(((unxs>=nxtpdown)||((nxtpdown-unxs)>pdownint))&&(pwroff==0)) 
    {
      nxtpdown=pdownint+unxs;
      if((solarpwr==0)||((solarpwr==1)&&(battfull==0)))
      {
        if(read_puptime(timerstart)==1)
        {
          syslog(LOG_NOTICE, "time to go to sleep");
          sleep(1);
          if(access(atpwrdown,X_OK)!=-1)
          {
            sprintf(message,"execute power down script %s",atpwrdown);
            syslog(LOG_NOTICE, message); 
            ok=system(atpwrdown);
            sleep(5);
          }
          downmins=read_pdowntime();
          pwroff=4;
          ok=system("/bin/sync");
          ok=system("/sbin/shutdown -h +1");
         }
      }
      else if((solarpwr==1)&&(battfull==1))
        syslog(LOG_NOTICE, "battery full, no power down");
    }

    if(((unxs>=nxtsleep)||((nxtsleep-unxs)>sleepint))&&(pwroff==0)) 
    {
      nxtsleep=sleepint+unxs;
      if(read_sleeptime()==1)
      {
        syslog(LOG_NOTICE, "time to go to sleep");
        sleep(1);
        if(access(atpwrdown,X_OK)!=-1)
        {
          sprintf(message,"execute power down script %s",atpwrdown);
          syslog(LOG_NOTICE, "%s", message); 
          ok=system(atpwrdown);
          sleep(5);
        }
        pwroff=1;
        ok=system("/bin/sync");
        ok=system("/sbin/shutdown -h now");
      }
    }

    if(((unxs>=nxtvolts)||((nxtvolts-unxs)>voltint))&&(pwroff==0))
    {
      nxtvolts=voltint+unxs;
      volts=readvolts();
      if(volttempa!=0) temp=readtemp();
      if((temp>-100)&&(temp<100)&&(volttempa!=0)) voltcal=volttempa*temp*temp+volttempb*temp+volttempc;
      voltsV=voltcal*(1023-volts)+vdrop;
      if(voltsV<Vmin) Vmin=voltsV;
      if(voltsV>Vmax) Vmax=voltsV;
      Vave+=voltsV;
      Vaven++;
      if((temp>-100)&&(temp<100)) 
      {
        if(temp<Tmin) Tmin=temp;
        if(temp>Tmax) Tmax=temp;
        Tave+=temp;
        Taven++;
      }
      cputemp=readcputemp();
      if(cputemp>-100) 
      {
        if(cputemp<Tcpumin) Tcpumin=cputemp;
        if(cputemp>Tcpumax) Tcpumax=cputemp;
        Tcpuave+=cputemp;
        Tcpuaven++;
      }

      battlev=battlevel(voltsV);
      if(battlev>=95) 
      {
        if((battfull==0)&&(solarpwr==1))
          syslog(LOG_NOTICE, "battery full, stop cyclic power down");
        battfull=1;
      }
      if(battlev<85) 
      {
        if((battfull==1)&&(solarpwr==1))
          syslog(LOG_NOTICE, "battery not full any more, restart cyclic power down");
        battfull=0;
      }
      batim=battime(battlev,battcap,pkfact,phours,current);
      sprintf(message,"read voltage %d (%4.1f V %3.0f %% %4.0f hours)",volts,voltsV,battlev,batim);
      if((temp>-100)&&(temp<100)&&(volttempa!=0)) sprintf(message,"read voltage %d (%4.1f V %3.0f %% %4.0f hours at %4.1f C)",volts,voltsV,battlev,batim,temp);
      syslog(LOG_INFO, "%s", message);
      ophours=optime(battlev,minbattlev,battcap,pkfact,phours,current);
      write_battery(volts,voltsV,batim,ophours,battlev);
      if(volts>minvolts)
      {
        syslog(LOG_WARNING, "battery voltage low, shut down and power off");
        sleep(1);
        if(access(atpwrdown,X_OK)!=-1)
        {
          sprintf(message,"execute power down script %s",atpwrdown);
          syslog(LOG_NOTICE, "%s", message);
          ok=system(atpwrdown);
          sleep(5);
        }
        pwroff=2;
        ok=system("/bin/sync");
        ok=system("/sbin/shutdown -h now battery low");
      }
      if(battlev<minbattlev)
      {
        syslog(LOG_WARNING, "battery charge low, shut down and power off");
        sleep(1);
        if(access(atpwrdown,X_OK)!=-1)
        {
          sprintf(message, "execute power down script %s", atpwrdown);
          syslog(LOG_NOTICE, "%s", message); 
          ok=system(atpwrdown);
          sleep(5);
        }
        pwroff=1;
        ok=system("/bin/sync");
        ok=system("/sbin/shutdown -h +5 battery charge low");
      }
      if(voltsV>maxbattvolts)
      {
        syslog(LOG_WARNING, "too high charging voltage reached");
        sleep(1);
        ok=system("/usr/bin/wall too high charging voltage reached");
      }
      syslog(LOG_DEBUG, "unxs=%d nxtvolts=%d",unxs,nxtvolts);
      if(reset_event_register()!=1)
        syslog(LOG_ERR, "failed to reset event register");
      sleep(1);
      if(reset_event_register()!=1)
        syslog(LOG_ERR, "failed to reset event register");
      sleep(1);
    }

    if(((unxs>=nxtbutton)||((nxtbutton-unxs)>buttonint))&&(pwroff==0))
    {
      button=read_button();
      nxtbutton=buttonint+unxs;
      if((button==0x01)||(button==0x81))
      {
        syslog(LOG_NOTICE, "button pressed");
        ok=write_cmd(0x25,0,0); // turn on red LED
        sleep(1);

        if(reset_event_register()!=1)
          syslog(LOG_ERR, "failed to reset event register");
        sleep(1);

        if(reset_event_register()!=1)
          syslog(LOG_ERR, "failed to reset event register");
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
            syslog(LOG_NOTICE, "shutdown confirmed");
            sleep(1);
            if(access(atpwrdown,X_OK)!=-1)
            {
              sprintf(message,"execute power down script %s",atpwrdown);
              syslog(LOG_NOTICE, "%s", message);
              ok=system(atpwrdown);
              sleep(5);
            }
            pwroff=1;
            ok=system("/bin/sync");
            ok=system("/sbin/shutdown -h now");
          }
        }
        sleep(1); 
        ok=write_cmd(0x15,0,0); // turn off red LED
      }
      sprintf(message,"unxs=%d nxtbutton=%d",unxs,nxtbutton);
      syslog(LOG_DEBUG, "%s", message);
    }

    if(((unxs>=nxtcounter)||((nxtcounter-unxs)>countint))&&(pwroff==0)&&(countint>10)) 
    {
      nxtcounter=countint+unxs;
      timer=read_timer();
      syslog(LOG_INFO, "PIC timer at %d",timer);
      write_timer(timer);
    }

    if(((unxs>=nxtwifi)||((nxtwifi-unxs)>wifint))&&(pwroff==0)&&(wifint>=60))
    {
      nxtwifi=wifint+unxs;
      wifiup=read_wifi();
      syslog(LOG_INFO, "WiFi status %d",wifiup);
      if(wifiup==1) 
      {
        wifiuptime+=wifint;
        wifidown=0;
      }
      else if(wifiup==-1) wifidown+=wifint;

      if((wifiup==-1)&&(wifidown>wifitimeout))
      {
        if(wifiact==1)
        {
          syslog(LOG_NOTICE, "interface down");
          ok=system("/sbin/ifdown wlan0");
          sleep(10);
          syslog(LOG_NOTICE, "interface up");
          ok=system("/sbin/ifup wlan0");
        }
        else if(wifiact==2)
        {
          syslog(LOG_WARNING, "reboot system");
          ok=system("/bin/sync");
          ok=system("/sbin/shutdown -r now");
        }
        else if(wifiact==3)
        {
          syslog(LOG_WARNING, "power cycle system");
          pwroff=3; 
          ok=system("/bin/sync");
          ok=system("/sbin/shutdown -h now");
        }
        wifidown=0;
      }
    }

    sleep(1);
  }

  int timerstop=0;
  unxstop=time(NULL);
  if(logstats==1) 
  {
    syslog(LOG_NOTICE, "write power up statistics");
    timerstop=read_timer();
    Vave/=Vaven;
    Tave/=Taven;
    Tcpuave/=Tcpuaven;
    writestat(statfile,unxstart,unxstop,timerstart,timerstop,Vmin,Vave,Vmax,Tmin,Tave,Tmax,Tcpumin,Tcpuave,Tcpumax,wifiuptime);
  }

  syslog(LOG_NOTICE, "remove PID file");
  ok=remove(pidfile);

  return ok;
}
