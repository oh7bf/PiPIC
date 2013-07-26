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
* Edit: 
* 
* Jaakko Koivuniemi
**/

#include <iostream>
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
  std::cout << "usage: pipic -a address [-c command|-r b|w|2w] [-h] [-v] [-V]\n";
}

void printversion()
{
  std::cout << "pipic v. 20130726, Jaakko Koivuniemi\n";
}


int main(int argc, char **argv)
{  
  int verb=0; // 1=verbosed output
  int rbyte=0; // read byte
  int rword=0; // read word
  int rword2=0; // read two words
  int fd;
  char fileName[100]="/dev/i2c-1";
  int  address=0x00;
  int  cmd=-1; // no operation
  unsigned char buf[10];
  char nstr[100]="";

  int optch=0;
  while(optch!=-1)
    {
      optch=getopt(argc,argv,"a:c:r:hvV");
      if(optch=='a')
	{
	  address=atoi(optarg);
	}
      if(optch=='c')
	{
          cmd=atoi(optarg);
	}
      if(optch=='r')
	{
          if(strcmp(optarg,"b")==0) rbyte=1;
          if(strcmp(optarg,"w")==0) rword=1;
          if(strcmp(optarg,"2w")==0) rword2=1;
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

  if((address<0x03)||(address>0x77)||(cmd>65535))
    {
      printusage();
      return -1;
    }

// open port for reading and writing
  if(verb==1) std::cout << "Open " << fileName << "\n";
  if((fd = open(fileName, O_RDWR)) < 0) 
  {
     std::cout << "Failed to open i2c port: " << strerror(errno) << "\n";
     return -1;
  }

  sprintf(nstr,"0x%02x",address);
  if(verb==1) std::cout << "Chip address " << nstr << "\n";
  if(ioctl(fd, I2C_SLAVE, address) < 0) 
  {
     std::cout << "Unable to get bus access to talk to slave: " 
     << strerror(errno) << "\n";
     return -1;
  }

  if((cmd>=0)&&(cmd<=255))
  { 
     sprintf(nstr,"0x%02x",cmd);
     if(verb==1) std::cout << "Command " << nstr << "\n";
     buf[0]=cmd;
     if((write(fd, buf, 1)) != 1) 
     {
        std::cout << "Error writing to i2c slave: " << strerror(errno) << "\n";
        return -1;
     }
  }
  else if((cmd>=0)&&(cmd<=65535))
  { 
     sprintf(nstr,"0x%04x",cmd);
     if(verb==1) std::cout << "Command " << nstr << "\n";
     buf[0]=(int)(cmd/256);
     buf[1]=cmd%256;
     if((write(fd, buf, 2)) != 2) 
     {
        std::cout << "Error writing to i2c slave: " << strerror(errno) << "\n";
        return -1;
     }
  }


  if(rbyte==1)
  {
     if(read(fd, buf,1)!=1) 
     {
        std::cout << "Unable to read from slave: " << strerror(errno) << "\n";
        return -1;
     }
     else 
     {
        printf("0x%02x\n",buf[0]);
     }
  } 
  else if(rword==1)
  {
     if(read(fd, buf,2)!=2) 
     {
        std::cout << "Unable to read from slave: " << strerror(errno) << "\n";
        return -1;
     }
     else 
     {
        printf("0x%02x%02x\n",buf[0],buf[1]);
     }
  }
  else if(rword2==1)
  {
     if(read(fd, buf,4)!=4) 
     {
        std::cout << "Unable to read from slave: " << strerror(errno) << "\n";
        return -1;
     }
     else 
     {
        printf("0x%02x%02x%02x%02x\n",buf[0],buf[1],buf[2],buf[3]);
     }
  }

  return 0;
}
