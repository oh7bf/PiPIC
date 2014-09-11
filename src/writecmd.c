#include "writecmd.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include "logmessage.h"
#include "pipichbd.h"

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
  char message[200]="";

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

