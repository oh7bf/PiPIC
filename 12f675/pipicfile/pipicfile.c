/**************************************************************************
*
* Read file addresses from PiPIC processor on i2c bus connected to Raspberry Pi.
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
* Thu Aug  1 23:55:08 CEST 2013
* Edit: Fri Aug  2 21:09:27 CEST 2013
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
  printf("usage: pipicfile -a address [-f file address] [-h] [-v] [-V]\n");
}

void printversion()
{
  printf("pipicfile v. 20130802, Jaakko Koivuniemi\n");
}


int main(int argc, char **argv)
{  
  int verb=0; // 1=verbosed output
  int fd;
  char fileName[100]="/dev/i2c-1";
  int  address=0x00;
  int  file=-1;
  unsigned char buf[10];
  int i=0,j=0;
  int byte1=0,byte2=0;

  const char *reg0[32]={"","TMR0","PCL","STATUS","FSR","GPIO","","","","","PCLATH","INTCON","PIR1","","TMR1L","TMR1H","T1CON","","","","","","","","","CMCON","","","","","ADRESH","ADCON0"};

  const char *reg1[32]={"","OPTION_REG","PCL","STATUS","FSR","TRISIO","","","","","PCLATH","INTCON","PIE1","","PCON","","OSCCAL","","","","","WPU","IOC","","","VRCON","EEDATA","EEADR","EECON1","","ADRESL","ANSEL"};

  int optch=0;
  while(optch!=-1)
    {
      optch=getopt(argc,argv,"a:f:hvV");
      if(optch=='a')
	{
	  address=atoi(optarg);
	}
      if(optch=='f')
	{
          file=atoi(optarg);
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

  if((file>=0)&&(file<=255))
  {
     buf[0]=1;
     buf[1]=3;
     if((write(fd, buf, 2)) != 2) 
     {
        perror("Error writing to i2c slave");
        return -1;
     }
  
     if(read(fd, buf,1)!=1) 
     {
        perror("Unable to read from slave");
        return -1;
     }
     else 
     {
        printf("0x%02x\n",buf[0]); 
     } 
  }
  else
  {
     if(verb==1)
     {
        for(i=0;i<32;i++)
        {
           byte1=-1;
           byte2=-1;

           if(reg0[i][0]!='\00') 
           {
             buf[0]=1;
             buf[1]=i;
             if((write(fd, buf, 2)) != 2) 
             {
                perror("Error writing to i2c slave");
                return -1;
             }
             if(read(fd, buf,1)!=1) 
             {
                perror("Unable to read from slave");
                return -1;
             }
             else 
             {
                byte1=buf[0];
             }
           } 
           
           if(reg1[i][0]!='\00') 
           {
             buf[0]=1;
             buf[1]=i+0x80;
             if((write(fd, buf, 2)) != 2) 
             {
                perror("Error writing to i2c slave");
                return -1;
             }
             if(read(fd, buf,1)!=1) 
             {
                perror("Unable to read from slave");
                return -1;
             }
             else 
             {
                byte2=buf[0];
             }
           }

           if((byte1>=0)||(byte2>=0))
           {                
             printf("0x%02X ",i);
             if(byte1>=0) printf("%-10s 0x%02X ",reg0[i],byte1);
             else printf("                ");

             printf("   0x%02X ",i+0x80);
             if(byte2>=0) printf("%-10s 0x%02X\n",reg1[i],byte2);
             else printf("            \n");
           }
       }
    }
    else
    {
      printf("      0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
      for(j=0;j<=1;j++)
      {
        printf("%1d0: ",j);
        for(i=0;i<=15;i++)
        {
           if(reg0[i+j*16][0]!='\00') 
           {
             buf[0]=1;
             buf[1]=i+j*16;
             if((write(fd, buf, 2)) != 2) 
             {
                perror("Error writing to i2c slave");
                return -1;
             }
             if(read(fd, buf,1)!=1) 
             {
                perror("Unable to read from slave");
                return -1;
             }
             else 
             {
                printf(" %02X",buf[0]);
             }
           } 
           else
           {
              printf(" --");
           }
        }
        printf("\n");
      }

      for(j=2;j<=5;j++)
      {
        printf("%1X0: ",j);
        for(i=0;i<=15;i++)
        {
           buf[0]=1;
           buf[1]=i+j*16;
           if((write(fd, buf, 2)) != 2) 
           {
             perror("Error writing to i2c slave");
             return -1;
           }
           if(read(fd, buf,1)!=1) 
           {
             perror("Unable to read from slave");
             return -1;
           }
           else 
           {
             printf(" %02X",buf[0]);
           }
        }
        printf("\n");
      }

      for(j=0;j<=1;j++)
      {
        printf("%1X0: ",j+8);
        for(i=0;i<=15;i++)
        {
           if(reg1[i+j*16][0]!='\00') 
           {
             buf[0]=1;
             buf[1]=i+j*16+0x80;
             if((write(fd, buf, 2)) != 2) 
             {
                perror("Error writing to i2c slave");
                return -1;
             }
             if(read(fd, buf,1)!=1) 
             {
                perror("Unable to read from slave");
                return -1;
             }
             else 
             {
                printf(" %02X",buf[0]);
             }
           } 
           else
           {
              printf(" --");
           }
        }
        printf("\n");
      }

      for(j=2;j<=5;j++)
      {
        printf("%1X0: ",j+8);
        for(i=0;i<=15;i++)
        {
           buf[0]=1;
           buf[1]=i+j*16+0x80;
           if((write(fd, buf, 2)) != 2) 
           {
             perror("Error writing to i2c slave");
             return -1;
           }
           if(read(fd, buf,1)!=1) 
           {
             perror("Unable to read from slave");
             return -1;
           }
           else 
           {
             printf(" %02X",buf[0]);
           }
        }
        printf("\n");
      }

    }
  }       
  
  return 0;
}
