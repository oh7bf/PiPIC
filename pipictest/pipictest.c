/**************************************************************************
*
* Test PiPIC processor on i2c bus connected to Raspberry Pi.
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
* Wed Aug 14 22:33:01 CEST 2013
* Edit: Mon Aug 19 13:44:18 EEST 2013
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

int printime()
{
  time_t now;
  char tstr[25];
  struct tm* tm_info;
  int utime=0;

  utime=time(&now);
  tm_info=localtime(&now);
  strftime(tstr,25,"%Y-%m-%d %H:%M:%S ",tm_info);
  printf("%s",tstr);

  return utime;
}

void printusage()
{
  printf("usage: pipictest -a address [-n N] [-i] [-c] [-0] [-1] [-3] [-h] [-v] [-V]\n");
}

void printversion()
{
  printf("pipictest v. 20130819, Jaakko Koivuniemi\n");
}

int main(int argc, char **argv)
{  
  int verb=0; // 1=verbosed output
  int testi2c=0; // test i2c data flow 
  int ccycle=0; // count clock cycles
  int analog0=0; // read in AN0 analog voltage
  int analog1=0; // read in AN1 analog voltage
  int analog3=0; // read in AN3 analog voltage
  int fd;
  char fileName[100]="/dev/i2c-1";
  int  address=0x00;
  unsigned char buf[10];
  unsigned char send[10];
  int i=0,j=0;
  int errcnt=0; // error count
  int nrpt=1; // number of times to repeate test function
  time_t now;
  int rtime=0; // cycle time from PIC
  int t1=0,t2=0,dt=0; // start and end time of cycle count
  int ain0=0,ain1=0,ain3=0; // analog input voltage

  int optch=0;
  while(optch!=-1)
    {
      optch=getopt(argc,argv,"a:n:i013chvV");
      if(optch=='a')
	{
	  sscanf(optarg,"%X",&address);
	}
      if(optch=='n')
	{
	  nrpt=atoi(optarg); 
	}
      if(optch=='i')
	{
          testi2c=1;          
	}
      if(optch=='c')
	{
          ccycle=1;          
	}
      if(optch=='0')
	{
          analog0=1;          
	}
      if(optch=='1')
	{
          analog1=1;          
	}
      if(optch=='3')
	{
          analog3=1;          
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

// open port for reading and writing
  if(verb==1) printf("Open %s\n", fileName);
  if((fd = open(fileName, O_RDWR)) < 0) 
  {
     perror("Failed to open i2c port");
     return -1;
  }

  if(verb==1) printf("Chip address 0x%02x\n", address);
  if(ioctl(fd, I2C_SLAVE, address) < 0) 
  {
     perror("Unable to get bus access to talk to slave");
     return -1;
  }

  if((verb==1)&&(testi2c==1)) printf("                    send         received\n"); 
  if(ccycle==1)
  {
     printf("reset timer\n");
     t1=printime();
     buf[0]=0x50; // reset PIC timer
     if((write(fd, buf, 1)) != 1) 
     {
          perror("Error writing to i2c slave");
          return -1;
     }
     printf("cycles\n");
  } 

  srand((unsigned int)time(NULL));
  for(j=0;j<nrpt;j++)
  {
     if(testi2c==1)
     {
        if(verb==1) printime();
        errcnt=0;
        for(i=0;i<4;i++)
        {
           buf[i+1]=rand();
           send[i]=buf[i+1];
        }
        buf[0]=2;
        if(verb==1) 
        {
           printf("0x%02x%02x%02x%02x",buf[1],buf[2],buf[3],buf[4]);
           printf("   ");
        }
        if((write(fd, buf, 5)) != 5) 
        {
           perror("Error writing to i2c slave");
           return -1;
        }
        if(read(fd, buf,4)!=4) 
        {
           perror("Unable to read from slave");
           return -1;
        }
        else 
        {
           if(verb==1)printf("0x%02x%02x%02x%02x\n",buf[0],buf[1],buf[2],buf[3]); 
           if((send[0]!=buf[0])||(send[1]!=buf[1])||(send[2]!=buf[2])||(send[3]!=buf[3])) 
           {
              printime();
              printf("0x%02x%02x%02x%02x",send[0],send[1],send[2],send[3]);
              printf(" !=");
              printf("0x%02x%02x%02x%02x\n",buf[0],buf[1],buf[2],buf[3]); 
              errcnt++;
           }
        }
     }

     if(ccycle==1)
     {
        if(verb==1) t2=printime();
        buf[0]=0x51;
        if((write(fd, buf, 1)) != 1) 
        {
           perror("Error writing to i2c slave");
           return -1;
        }
        if(read(fd, buf,4)!=4) 
        {
           perror("Unable to read from slave");
           return -1;
        }
        else 
        {
           if(verb==1)printf("0x%02x%02x%02x%02x ",buf[0],buf[1],buf[2],buf[3]); 
           rtime=16777216*buf[0]+65536*buf[1]+256*buf[2]+buf[3];
           if(verb==1) printf ("%d\n",rtime);
        }
     }

     if((analog0==1)||(analog1==1)||(analog3==1)) t2=printime();
     if(analog0==1)
     {
        buf[0]=0x40;
        if((write(fd, buf, 1)) != 1) 
        {
           perror("Error writing to i2c slave");
           return -1;
        }
        if(read(fd, buf,2)!=2) 
        {
           perror("Unable to read from slave");
           return -1;
        }
        else 
        {
           if(verb==1)printf("0x%02x%02x ",buf[0],buf[1]);
           ain0=256*buf[0]+buf[1];
           printf ("%d ",ain0);
        }
     }

     if(analog1==1)
     {
        buf[0]=0x41;
        if((write(fd, buf, 1)) != 1) 
        {
           perror("Error writing to i2c slave");
           return -1;
        }
        if(read(fd, buf,2)!=2) 
        {
           perror("Unable to read from slave");
           return -1;
        }
        else 
        {
           if(verb==1)printf("0x%02x%02x ",buf[0],buf[1]);
           ain1=256*buf[0]+buf[1];
           printf ("%d ",ain1);
        }
     }

     if(analog3==1)
     {
        buf[0]=0x43;
        if((write(fd, buf, 1)) != 1) 
        {
           perror("Error writing to i2c slave");
           return -1;
        }
        if(read(fd, buf,2)!=2) 
        {
           perror("Unable to read from slave");
           return -1;
        }
        else 
        {
           if(verb==1)printf("0x%02x%02x ",buf[0],buf[1]);
           ain3=256*buf[0]+buf[1];
           printf ("%d ",ain3);
        }
     }

     if((analog0==1)||(analog1==1)||(analog3==1)) printf("\n");

  }

  if(ccycle==1)
  {
    t2=printime();
    dt=t2-t1;
    printf("elapsed seconds %d for %d cycles\n",dt,rtime);
    if(dt>0) printf("%f s/cycle\n",((float)dt)/((float)rtime));
  }
  
  if(testi2c==1) printf("i2c errors %d\n",errcnt);

  return 0;
}

