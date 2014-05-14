/**************************************************************************
*
* Send commands to a PIC processor on i2c bus connected to Raspberry Pi and
* read data from the PIC.  
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
* Fri Jul 26 20:28:06 CEST 2013
* Edit: Tue Aug 13 19:47:55 CEST 2013
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
  printf("usage: pipic -a address [-c command [-d data]] [-r b|w|W] [-h] [-v] [-V]\n");
}

void printversion()
{
  printf("pipic v. 20130813, Jaakko Koivuniemi\n");
}


int main(int argc, char **argv)
{  
  int verb=0; // 1=verbosed output
  int rbyte=0; // read byte
  int rword=0; // read 16-bit word
  int rword2=0; // read 32-bit word
  int rdata=0; // read data
  int wbyte=0; // write byte
  int wword=0; // write 16-bit word
  int wword2=0; // write 32-bit word
  int wdata=0; // data to write
  int rnxt=0;
  int fd;
  char fileName[100]="/dev/i2c-1";
  int  address=0x00;
  int  cmd=-1; // no operation
  unsigned char buf[10];

  int optch=0;
  while(optch!=-1)
    {
      optch=getopt(argc,argv,"a:c:d:r:hvV");
      if(optch=='a')
	{
          sscanf(optarg,"%X",&address);
	}
      if(optch=='c')
	{
          sscanf(optarg,"%X",&cmd);        
	}
      if(optch=='d')
        {
          if(optarg[strlen(optarg)-1]=='b') wbyte=1;
          if(optarg[strlen(optarg)-1]=='w') wword=1;
          if(optarg[strlen(optarg)-1]=='W') wword2=1;
          wdata=atoi(optarg);          
        }
      if(optch=='r')
	{
          if(optarg[0]=='b') rbyte=1;
          if(optarg[0]=='w') rword=1;
          if(optarg[0]=='W') rword2=1;
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

  if((address<0x03)||(address>0x77)||(cmd>255))
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

  if((cmd>=0)&&(cmd<=255))
  { 
     buf[0]=cmd;
     if(wbyte==1)
     { 
        buf[1]=wdata;
        if(verb==1) printf("Send 0x%02x%02x\n",buf[0],buf[1]);
        if((write(fd, buf, 2)) != 2) 
        {
           perror("Error writing to i2c slave");
           return -1;
        }
     }
     else if(wword==1)
     { 
        buf[1]=(int)(wdata/256);
        buf[2]=wdata%256;
        if(verb==1) printf("Send 0x%02x%02x%02x\n",buf[0],buf[1],buf[2]); 
        if((write(fd, buf, 3)) != 3) 
        {
           perror("Error writing to i2c slave");
           return -1;
        }
     }
     else if(wword2==1)
     { 
        buf[1]=(int)(wdata/16777216);
        rnxt=wdata%16777216;
        buf[2]=(int)(rnxt/65536);
        rnxt=rnxt%65536;
        buf[3]=(int)(rnxt/256);
        buf[4]=rnxt%256;
        if(verb==1) printf("Send 0x%02x%02x%02x%02x%02x\n",buf[0],buf[1],buf[2],buf[3],buf[4]);
        if((write(fd, buf, 5)) != 5) 
        {
           perror("Error writing to i2c slave");
           return -1;
        }
     }
     else
     {
        if(verb==1) printf("Send 0x%02x\n",buf[0]);
        if((write(fd, buf, 1)) != 1) 
        {
           perror("Error writing to i2c slave");
           return -1;
        }
     }
  }


  if(rbyte==1)
  {
     if(read(fd, buf,1)!=1) 
     {
        perror("Unable to read from slave");
        return -1;
     }
     else 
     {
        if(verb==1) printf("Receive 0x%02x\n",buf[0]);
        rdata=buf[0];
        printf ("%d\n",rdata);
     }
  } 
  else if(rword==1)
  {
     if(read(fd, buf,2)!=2) 
     {
        perror("Unable to read from slave");
        return -1;
     }
     else 
     {
        if(verb==1) printf("Receive 0x%02x%02x\n",buf[0],buf[1]); 
        rdata=256*buf[0]+buf[1];
        printf ("%d\n",rdata);
     }
  }
  else if(rword2==1)
  {
     if(read(fd, buf,4)!=4) 
     {
        perror("Unable to read from slave");
        return -1;
     }
     else 
     {
        if(verb==1) printf("Receive 0x%02x%02x%02x%02x\n",buf[0],buf[1],buf[2],buf[3]);
        rdata=16777216*buf[0]+65536*buf[1]+256*buf[2]+buf[3];
        printf ("%d\n",rdata);
     }
  }

  return 0;
}
