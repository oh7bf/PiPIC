/****************************************************************************
 *
 * Client for pipicswd server controlling an external power switch using 
 * i2c bus on Raspberry Pi. 
 *       
 * Copyright (C) 2014 Jaakko Koivuniemi.
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
 * Wed Feb 19 21:49:16 CET 2014
 * Edit: Mon Feb 24 21:33:09 CET 2014
 *
 * Jaakko Koivuniemi
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

void printusage()
{
  printf("usage: pipicsw [-h host] [-p port] [command]\n");
}

void printversion()
{
  printf("pipicsw v. 20140224, Jaakko Koivuniemi\n");
}

int main(int argc, char *argv[])
{
    int portno=0; // socket port number
    char host[100]=""; // server host named
    char command[200]=""; // command to send to server

    int i=1;
    while((i<argc)&&(argc<8))
    {
      if((strncmp(argv[i],"-h",2)==0)&&(i<argc+1)) 
      {
        strncpy(host,argv[i+1],100);
        i++;
      }
      else if((strncmp(argv[i],"-p",2)==0)&&(i<argc+1)) 
      {
        portno=atoi(argv[i+1]);
        i++;
      }
      else if((strncmp(argv[i],"help",4)==0))
      {
        printusage();
        return 0;    
      }
      else if((strncmp(argv[i],"version",7)==0))
      {
        printversion();
        return 0;    
      }
      else
      {
        strncat(command,argv[i],20);
        strncat(command," ",1);
      }
      i++;
    }

// set default values if were not set by user
    if(strcmp(command,"")==0)
    {
      strncpy(command,"status",6);
    }
    if(strcmp(host,"")==0)
    {
      strncpy(host,"localhost",9);
    }
    if(portno==0)
    {
      portno=5001;
    }

    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    // open socket
    sockfd=socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd<0) 
    {
        perror("Could not open socket");
        exit(EXIT_FAILURE);
    }
    server=gethostbyname(host);
    if(server==NULL) 
    {
        fprintf(stderr,"No such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
           (char *)&serv_addr.sin_addr.s_addr,
                server->h_length);
    serv_addr.sin_port=htons(portno);

    // connect to server
    if(connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr))<0) 
    {
      perror("Failed to connect");
      exit(1);
    }	

    char buffer[20];
    bzero(buffer,20);

    strncpy(buffer,command,20);

    // send command to socket
    if(write(sockfd,buffer,strlen(buffer))<0)
    {
      perror("Failed to write to socket");
      exit(1);
    }

    // read server response
    bzero(buffer,20);
    n=read(sockfd,buffer,19);
    if(n<0) 
    {
      perror("Failed to read from socket");
      exit(1);
    }
    printf("%.20s\n",buffer);

    return 0;

}


