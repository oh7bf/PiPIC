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
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

void printusage()
{
  printf("usage: pipictest -a address [-n N] [-i] [-h] [-v] [-V]\n");
}

void printversion()
{
  printf("pipictest v. 20130814, Jaakko Koivuniemi\n");
}


int main(int argc, char **argv)
{  
  int verb=0; // 1=verbosed output
  int testi2c=0; // test i2c data flow 
  int fd;
  char fileName[100]="/dev/i2c-1";
  int  address=0x00;
  unsigned char buf[10];
  unsigned char send[10];
  int i=0,j=0;
  int errcnt=0; // error count
  int nrpt=1; // number of times to repeate test function

  int optch=0;
  while(optch!=-1)
    {
      optch=getopt(argc,argv,"a:n:ihvV");
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

  if(testi2c==1)
  {
     srand((unsigned int)time(NULL));

     errcnt=0;
     if(verb==1) printf("send         received\n");
     for(j=0;j<nrpt;j++)
     {
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
              printf("0x%02x%02x%02x%02x",send[0],send[1],send[2],send[3]);
              printf(" !=");
              printf("0x%02x%02x%02x%02x\n",buf[0],buf[1],buf[2],buf[3]); 
              errcnt++;
           }
        }
     }
     printf("Errors %d\n",errcnt);
  }

  return 0;
}

