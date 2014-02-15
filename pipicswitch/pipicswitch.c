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
 * Sat Feb 15 20:35:37 CET 2014
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
#include <getopt.h>
#include <errno.h>
#include <time.h>

#define CHECK_BIT(var,pos) !!((var) & (1<<(pos)))

const int version=20140215; // program version

const char i2cdev[100]="/dev/i2c-1";
int  address=0x27;
const int  i2lockmax=10; // maximum number of times to try lock i2c port  

// write i2c command to PIC optionally followed by data, length is the number 
// of bytes and can be 0, 1, 2 or 4 
// return: 1=ok, -1=open failed, -2=lock failed, -3=bus access failed, 
// -4=i2c slave writing failed
int write_cmd(int cmd, int data, int length, int verb)
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
      printf("Failed to open i2c port\n");
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
      printf("Failed to lock i2c port\n");
      return -2;
    }

    if(ioctl(fd, I2C_SLAVE, address) < 0) 
    {
      printf("Unable to get bus access to talk to slave\n");
      return -3;
    }

    buf[0]=cmd;
    if(length==1)
    { 
      buf[1]=data;
      if(verb==1) printf("Send 0x%02x%02x\n",buf[0],buf[1]);
      if((write(fd, buf, 2)) != 2) 
      {
        printf("Error writing to i2c slave\n");
        return -4;
      }
   }
   else if(length==2)
   { 
      buf[1]=(int)(data/256);
      buf[2]=data%256;
      if(verb==1) printf("Send 0x%02x%02x%02x\n",buf[0],buf[1],buf[2]);
      if((write(fd, buf, 3)) != 3) 
      {
        printf("Error writing to i2c slave\n");
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
      if(verb==1) printf("Send 0x%02x%02x%02x%02x%02x\n",buf[0],buf[1],buf[2],buf[3],buf[4]);
      if((write(fd, buf, 5)) != 5) 
      {
        printf("Error writing to i2c slave\n");
        return -4;
      }
   }
   else
   {
      if(verb==1) printf("Send 0x%02x\n",buf[0]);
      if((write(fd, buf, 1)) != 1) 
      {
        printf("Error writing to i2c slave\n");
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
int read_data(int length, int verb)
{
  int rdata=0;
  int fd,rd;
  int cnt=0;
  unsigned char buf[10];

  if((fd=open(i2cdev, O_RDWR)) < 0) 
  {
    printf("Failed to open i2c port\n");
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
    printf("Failed to lock i2c port\n");
    return -2;
  }

  if(ioctl(fd, I2C_SLAVE, address) < 0) 
  {
    printf("Unable to get bus access to talk to slave\n");
    return -3;
  }

  if(length==1)
  {
     if(read(fd, buf,1)!=1) 
     {
       printf("Unable to read from slave\n");
       return -4;
     }
     else 
     {
       if(verb==1) printf("Receive 0x%02x\n",buf[0]);
       rdata=buf[0];
     }
  } 
  else if(length==2)
  {
     if(read(fd, buf,2)!=2) 
     {
       printf("Unable to read from slave\n");
       return -4;
     }
     else 
     {
       if(verb==1) printf("Receive 0x%02x%02x\n",buf[0],buf[1]);
       rdata=256*buf[0]+buf[1];
     }
  }
  else if(length==4)
  {
     if(read(fd, buf,4)!=4) 
     {
       printf("Unable to read from slave\n");
       return -4;
     }
     else 
     {
        if(verb==1) printf("Receive 0x%02x%02x%02x%02x\n",buf[0],buf[1],buf[2],buf[3]);
        rdata=16777216*buf[0]+65536*buf[1]+256*buf[2]+buf[3];
     }
  }

  close(fd);

  return rdata;
}

// send 4 test bytes to PiPIC and read them back 
int testi2c(int verb)
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

  ok=write_cmd(0x02,testint,4,verb);

  if(ok!=1)
  {
    printf("failed to write 4 test bytes\n"); 
  }
  else
  {
    testres=read_data(4,verb);

    if((testres==-1)||(testres==-2)||(testres==-3)||(testres==-4))
    {
      printf("failed to read 4 test bytes\n"); 
    }
    else
    {
      if(testint==testres) ok=1; else ok=0;
    }
  }

  return ok;
}

// read internal timer 
int read_timer(int verb)
{
  int timer=-1;

  if(write_cmd(0x51,0,0,verb)!=1) printf("failed to send read timer command\n");
  else timer=read_data(4,verb);

  return timer;
}

// reset timer
int resetimer(int verb)
{
  int ok=-1;

  ok=write_cmd(0x50,0,0,verb);

  return ok;
}

void printusage()
{
  printf("usage: pipicswitch -a address [-s 1|2:on|off|toggle] [-r] [-t] [-T] [-h] [-v] [-V]\n");
}

void printversion()
{
  printf("pipicswitch v. 20140215, Jaakko Koivuniemi\n");
}

int main(int argc, char **argv)
{  
  int verb=0; // 1=verbosed output
  int i2ctest=0; // test i2c data flow
  int restimer=0; // reset PIC timer
  int readtimer=0; // read PIC timer
  int swtch=0; // operate switch
  int channel=1; // switch channel
  char operation[200]=""; // switch operation off/on/toggle

  int timer=0;
  int gpio=0;
  int ok=0;

  int optch=0;
  while(optch!=-1)
    {
      optch=getopt(argc,argv,"a:s:rtThvV");
      if(optch=='a')
        {
          sscanf(optarg,"%X",&address);
        }
      if(optch=='s')
        {
          if(sscanf(optarg,"%d:%s",&channel,operation)!=EOF) swtch=1;
        }
      if(optch=='T')
        {
          i2ctest=1;          
        }
      if(optch=='r')
        {
          restimer=1;          
        }
      if(optch=='t')
        {
          readtimer=1;          
        }
      if(optch=='v')
        {
          verb=1;
        }
      if(optch=='h')
        {
          printusage();
          return 0;
        }
      if(optch=='V')
        {
          printversion();
          return 0;
        }
    }

  if((address<0x03)||(address>0x77))
    {
      printusage();
      return -1;
    }
  else if(swtch==1)
    {

    }
  else if(i2ctest==1)
    {
      ok=testi2c(verb);
      printf("%d",ok);
    }
  else if(restimer==1)
    {
      ok|=resetimer(verb);
    }
  else if(readtimer==1)
    {
      timer=read_timer(verb);
      printf("%d\n",timer);
    }
  else
    {
      // read GPIO
      ok=write_cmd(0x01,0x05,1,verb); 
      if(ok==1)
        { 
           gpio=read_data(1,verb);
           if(CHECK_BIT(gpio,4)==1) printf("switch 1 on\n");
           else printf("switch 1 off\n");
           if(CHECK_BIT(gpio,5)==1) printf("switch 2 on\n");
           else printf("switch 2 off\n");
        }
    }

  return ok;
}
