/**************************************************************************
 * 
 * Control external power switch using i2c bus on Raspberry Pi. The switch
 * has a PIC processor to interprete commands send by this program.  
 *       
 * Copyright (C) 2014 - 2021 Jaakko Koivuniemi.
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
 * Sun Feb 16 14:29:25 CET 2014
 * Edit: Fri  1 Jan 09:12:54 CST 2021
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
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include "pipicswd.h"
#include "writecmd.h"
#include "readdata.h"
#include "testi2c.h"

#define CHECK_BIT(var,pos) !!((var) & (1<<(pos)))

const int version = 20150221; // program version

int portno = 5001; // socket port number

float picycle = 0.445; // length of PIC counter cycles [s]

const char *i2cdev = "/dev/i2c-1"; // i2c device file
const int  address = 0x27;
const int  i2lockmax = 10; // maximum number of times to try lock i2c port  
int forcereset = 0; // force PIC timer reset if i2c test fails

const char confile[ 200 ] = "/etc/pipicswd_config";

const char pidfile[ 200 ] = "/run/pipicswd.pid";

int loglev = 5;
char message[ 200 ] = "";

char status[ 200 ] = ""; // switch status message

int initswitch1 = 0; // switch 1 at start 0=do nothing, 1=switch on, 2=off
int initswitch2 = 0; // switch 2 at start 0=do nothing, 1=switch on, 2=off
int stopswitch1 = 0; // switch 1 at stop 0=do nothing, 1=switch on, 2=off
int stopswitch2 = 0; // switch 2 at stop 0=do nothing, 1=switch on, 2=off


// read configuration file if it exists
void read_config()
{
  FILE *cfile;
  char *line = NULL;
  char par[ 20 ];
  float value;
  size_t len;
  ssize_t read;

  cfile = fopen( confile, "r" );
  if( NULL != cfile )
  {
    syslog( LOG_INFO | LOG_DAEMON, "Read configuration file" );

    while( ( read = getline( &line, &len, cfile ) ) != -1 )
    {
       if( sscanf( line, "%s %f", par, &value ) != EOF )
       {
          if( strncmp( par, "LOGLEVEL", 8 ) == 0 )
          {
             loglev = (int)value;
             sprintf( message, "Log level set to %d", (int)value );
             syslog( LOG_INFO | LOG_DAEMON, "%s", message );
             setlogmask( LOG_UPTO (loglev) );
          }
          if( strncmp( par, "DCSWITCHPORT", 12 ) == 0 )
          {
             portno = (int)value;
             sprintf( message, "Switch port number set to %d", (int)value );
             syslog( LOG_INFO | LOG_DAEMON, "%s", message );
          }
          if( strncmp( par, "INITSWITCH1", 11 ) == 0 )
          {
             initswitch1 = (int)value;
             sprintf( message, "Switch 1 initialization set to %d", (int)value );
             syslog( LOG_INFO | LOG_DAEMON, "%s", message );
          }
          if( strncmp( par, "INITSWITCH2", 11 ) == 0 )
          {
             initswitch2 = (int)value;
             sprintf( message, "Switch 2 initialization set to %d", (int)value );
             syslog( LOG_INFO | LOG_DAEMON, "%s", message );
          }
          if( strncmp( par, "STOPSWITCH1", 11 ) == 0 )
          {
             stopswitch1 = (int)value;
             sprintf( message, "Switch 1 stop value set to %d", (int)value );
             syslog( LOG_INFO | LOG_DAEMON, "%s", message );
          }
          if( strncmp( par, "STOPSWITCH2", 11 ) == 0 )
          {
             stopswitch2 = (int)value;
             sprintf( message, "Switch 2 stop value set to %d", (int)value );
             syslog( LOG_INFO | LOG_DAEMON, "%s", message );
          }
          if( strncmp( par, "PICYCLE", 7 ) == 0 )
          {
             picycle = value;
             sprintf( message, "PIC cycle %f s", value );
             syslog( LOG_INFO | LOG_DAEMON, "%s", message );
          }
          if( strncmp( par, "FORCERESET", 10 ) == 0 )
          {
             if( value == 1 )
             {
                forcereset = 1;
                syslog( LOG_INFO | LOG_DAEMON, "Force PIC timer reset at start if i2c test fails" );
             }
             else
             {
                forcereset = 0;
                syslog( LOG_INFO | LOG_DAEMON, "Exit in case of i2c test failure" );
             }
          }
       }
    }
    fclose( cfile );
  }
  else
  {
    sprintf( message, "Could not open %s", confile );
    syslog( LOG_ERR | LOG_DAEMON, "%s", message );
  }
}

// read command 1 timer status
int timer1status()
{
  int ok = 0;
  int timeron = 0;
  int gpio = 0;

  ok = write_cmd( 0x01, 0x27, 1 ); 
  if( ok == 1 )
  {
    gpio = read_data( 1 );
    if( CHECK_BIT( gpio, 7 ) == 1 ) timeron = 1;
  }

  return ( ok * timeron );
}

// read command 2 timer status
int timer2status()
{
  int ok = 0;
  int timeron = 0;
  int gpio = 0;

  ok = write_cmd( 0x01, 0x30, 1 ); 
  if( ok == 1 )
  {
    gpio = read_data( 1 );
    if( CHECK_BIT( gpio, 7 ) == 1 ) timeron = 1;
  }

  return ( ok * timeron );
}

// read timer 1 delay
int timer1delay()
{
  int ok = 0;
  int delay = 0;
  ok = write_cmd( 0x01, 0x2D, 1 );
  if( ok == 1 )
  {
    delay += read_data( 1 ) * 65536;
    ok = write_cmd( 0x01, 0x2E, 1 );
    if( ok == 1 )
    {
      delay += read_data( 1 ) * 256;
      ok = write_cmd( 0x01, 0x2F, 1 );
      if( ok == 1 )
      {
        delay += read_data( 1 );
      }
    }
  }
  return ( delay * ok );
}

// read timer 2 delay
int timer2delay()
{
  int ok = 0;
  int delay = 0;

  ok = write_cmd( 0x01, 0x36, 1 );
  if( ok == 1 )
  {
    delay += read_data( 1 ) * 65536;
    ok = write_cmd( 0x01, 0x37, 1 );
    if( ok == 1 )
    {
      delay += read_data( 1 ) * 256;
      ok = write_cmd( 0x01, 0x38, 1 );
      if( ok == 1 )
      {
        delay += read_data( 1 );
      }
    }
  }

  return ( delay * ok );
}

// read timer 1 command
int timer1cmd()
{
  int ok = 0;
  int cmd = 0;

  ok = write_cmd( 0x01, 0x2B, 1 );
  if( ok == 1 )
  {
    cmd = read_data( 1 );
  }

  return cmd;
}

// read timer 2 command
int timer2cmd()
{
  int ok = 0;
  int cmd = 0;
  ok = write_cmd( 0x01, 0x34, 1 );
  if( ok == 1 )
  {
    cmd = read_data( 1 );
  }

  return cmd;
}

// cancel timer 1 command
int timer1cancel()
{
  int ok = 0;

  ok = write_cmd( 0x60, 0x00, 1 );
  if( ok != 1 ) syslog( LOG_ERR | LOG_DAEMON, "failed to cancel timer 1 command" );
 
  return ok;
}

// cancel timer 2 command
int timer2cancel()
{
  int ok = 0;

  ok = write_cmd( 0x70, 0x00, 1 );
  if( ok != 1 ) syslog( LOG_ERR | LOG_DAEMON, "failed to cancel timer 2 command" );

  return ok;
}

// read switch status
int read_status()
{
  int ok = -1;
  int gpio = 0;
  int wtime = 0;
  int cmd = 0;
  const char space[] = ", ";

  ok = write_cmd( 0x01, 0x05, 1 ); 
  if( ok == 1 )
  { 
    gpio = read_data( 1 );
    if( CHECK_BIT( gpio, 4 ) == 1 ) 
    {
      strcpy( message, "switch 1 closed" );
      strcpy( status, "1 closed" ); 
    }
    else 
    {
      strcpy( message, "switch 1 open" );
      strcpy( status, "1 open" );
    }

    if( CHECK_BIT( gpio, 5 ) == 1 ) 
    {
      strcat( message, " switch 2 closed" );
      strcat( status, " 2 closed" );
    }
    else 
    {
      strcat( message, " switch 2 open" );
      strcat( status, " 2 open" );
    }
    syslog( LOG_INFO | LOG_DAEMON, "%s", message );
  }
  else syslog( LOG_ERR | LOG_DAEMON, "failed to read PIC GPIO register" ); 

  if( timer1status() == 1 )
  {
    wtime = picycle * timer1delay();
    cmd = timer1cmd(); 
    if( cmd == 0x24 ) sprintf( message, "close switch 1 after %d s", wtime ); 
    else if( cmd == 0x14 ) sprintf( message, "open switch 1 after %d s", wtime ); 
    else if( cmd == 0x25 ) sprintf( message, "close switch 2 after %d s", wtime );
    else if( cmd == 0x15 ) sprintf( message, "open switch 2 after %d s", wtime ); 
    else sprintf( message, "other command on timer 1 after %d s", wtime ); 
    syslog( LOG_INFO | LOG_DAEMON, "%s", message );
    strncat( status, space, 2 );
    strncat( status, message, 42 );
  }

  if( timer2status() == 1 )
  {
    wtime = picycle * timer2delay();
    cmd = timer2cmd(); 
    if( cmd == 0x24 ) sprintf( message, "close switch 1 after %d s", wtime ); 
    else if( cmd == 0x14 ) sprintf( message, "open switch 1 after %d s", wtime ); 
    else if( cmd == 0x25 ) sprintf( message, "close switch 2 after %d s", wtime );
    else if( cmd == 0x15 ) sprintf( message, "open switch 2 after %d s", wtime ); 
    else sprintf( message, "other command on timer2 after %d s", wtime ); 
    syslog( LOG_INFO | LOG_DAEMON, "%s", message );
    strncat( status, space, 2 );
    strncat( status, message, 42 );
  }

  return ok;
}

// start timed task to open/close switch, task can be 1 or 2 depending
// which is free to use, otherwise no new task is started
int newtask(int sw, int operation, int delay)
{
  int ok = 0;
  int cmd = 0x0000;

  if( sw == 1 && operation == 1 ) cmd = 0x2400;
  else if( sw == 1 && operation == 2 ) cmd = 0x1400; 
  else if( sw == 2 && operation == 1 ) cmd = 0x2500;
  else if( sw == 2 && operation == 2 ) cmd = 0x1500; 

  if( timer1status() == 0 )
  {
    sprintf( message, "task1 delay %d and command 0x%04x", delay, cmd );
    syslog( LOG_NOTICE | LOG_DAEMON, "%s", message );
// timed task1
    ok = write_cmd( 0x62, delay, 4 );
    ok = write_cmd( 0x63, cmd, 2 );
    ok = write_cmd( 0x64, 0, 1 );
    ok = write_cmd( 0x61, 0, 0); // start task1
  }
  else if( timer2status() == 0 )
  {
    sprintf( message, "task2 delay %d and command 0x%04x", delay, cmd );
    syslog( LOG_NOTICE | LOG_DAEMON, "%s", message );
// timed task2
    ok = write_cmd( 0x72, delay, 4 );
    ok = write_cmd( 0x73, cmd, 2 );
    ok = write_cmd( 0x74, 0, 1 );
    ok = write_cmd( 0x71, 0, 0 ); // start task2
  }
  else syslog( LOG_NOTICE | LOG_DAEMON, "tasks 1 and 2 already in use, cancel one of them first" );

  return ok;
}

// operation 1=close or 2=open after seconds given by wtime (0=now)
int operate_switch1(int switch1, int wtime)
{
  int ok = 0;
  int delay = 0;

  if( wtime > 0 ) delay = (int)( wtime / picycle );

  if( switch1 == 1 ) 
  {
     if( delay == 0 )
     {
       ok = write_cmd( 0x24, 0x00, 0 ); 
       if( ok != 1 ) syslog( LOG_ERR | LOG_DAEMON, "closing switch 1 failed" );
       else syslog( LOG_NOTICE | LOG_DAEMON, "switch 1 closed" );
     }
     else if( delay > 0 ) ok = newtask( 1, 1, delay );  
  }
  else if( switch1 == 2 ) 
  {
     if( delay == 0 )
     {
       ok = write_cmd( 0x14, 0x00, 0 ); 
       if( ok != 1 ) syslog( LOG_ERR | LOG_DAEMON, "opening switch 1 failed" );
       else syslog( LOG_NOTICE | LOG_DAEMON, "switch 1 opened" );
     }
     else if( delay > 0 ) ok = newtask( 1, 2, delay ); 
  }

  return ok;
}

int operate_switch2(int switch2, int wtime)
{
  int ok = 0;
  int delay = 0;

  if( wtime > 0 ) delay = (int)( wtime / picycle );

  if( switch2 == 1 ) 
  {
    if( delay == 0 )
    {
      ok = write_cmd( 0x25, 0x00, 0 ); 
      if( ok != 1 ) syslog( LOG_ERR | LOG_DAEMON, "closing switch 2 failed" );
      else syslog( LOG_NOTICE | LOG_DAEMON, "switch 2 closed" );
    }
    else if( delay > 0 ) ok = newtask( 2, 1, delay );  
  }
  else if( switch2 == 2 ) 
  {
    if( delay == 0 )
    {
      ok = write_cmd( 0x15, 0x00, 0 ); 
      if( ok != 1 ) syslog( LOG_ERR | LOG_DAEMON, "opening switch 2 failed" );
      else syslog( LOG_NOTICE | LOG_DAEMON, "switch 2 opened" );
    }
    else if( delay > 0 ) ok = newtask( 2, 2, delay );  
  }

  return ok;
}

// calculate seconds to wait for programmed switch operation
int calcwtime(int hh, int mm)
{
  int wtime = 0;

  time_t now;
  struct tm* tm_info;
  time( &now );
  tm_info = localtime( &now );
  int hour = tm_info->tm_hour;
  int minute = tm_info->tm_min; 
  int second = tm_info->tm_sec;

  if( mm < minute )
  {
    mm += 60;
    hh--;
  }
  if( hh < hour ) hh += 24;
  wtime = 3600 * ( hh - hour ) + 60 * ( mm - minute ) - second;
  if( wtime < 0 ) wtime = 0;

  return wtime;
}



// reset PIC internal timer
int resetimer()
{
  int ok = -1;

  ok = write_cmd( 0x50, 0, 0 );

  return ok;
}

int cont = 1; /* main loop flag */

void stop(int sig)
{
  sprintf( message, "signal %d catched, stop", sig );
  syslog( LOG_NOTICE | LOG_DAEMON, "%s", message );
  cont = 0;
}

void terminate(int sig)
{
  sprintf( message, "signal %d catched", sig );
  syslog( LOG_NOTICE | LOG_DAEMON, "%s", message );

  sleep( 1 );
  operate_switch1( stopswitch1, 0 );
  operate_switch2( stopswitch2, 0 );

  sleep( 1 );
  syslog( LOG_NOTICE | LOG_DAEMON, "stop" );

  cont = 0;

  exit( EXIT_SUCCESS );
}

void hup(int sig)
{
  sprintf( message, "signal %d catched", sig );
  syslog( LOG_NOTICE | LOG_DAEMON, "%s", message );
}


int main()
{  
  int ok = 0;

  setlogmask( LOG_UPTO (loglev) );
  syslog( LOG_NOTICE | LOG_DAEMON, "pipicswd v. %d started", version );

  signal( SIGKILL, &stop ); 
  signal( SIGTERM, &terminate ); 
  signal( SIGQUIT, &stop ); 
  signal( SIGHUP, &hup ); 

  read_config(); // read configuration file

  int i2cok = testi2c(); // test i2c data flow to PIC 
  if( i2cok == 1 ) syslog( LOG_NOTICE | LOG_DAEMON, "i2c dataflow test ok" ); 
  else
  {
    if( forcereset == 1 )
    {
      sleep( 1 ); 
      syslog( LOG_NOTICE | LOG_DAEMON, "try to reset timer now" );
      ok = resetimer();
      sleep( 1 );
      if( resetimer() != 1 )
      {
        syslog( LOG_ERR | LOG_DAEMON, "failed to reset timer" );
        cont = 0; 
      }
      else
      {
        sleep( 1 );
        i2cok = testi2c(); // test i2c data flow to PIC 
        if( i2cok == 1 ) syslog( LOG_NOTICE | LOG_DAEMON, "i2c dataflow test ok" ); 
        else
        {
          syslog( LOG_ERR | LOG_DAEMON, "i2c dataflow test failed" );
          cont = 0;
          exit( EXIT_FAILURE );
        }
      }
    }
    else
    {
      syslog( LOG_ERR | LOG_DAEMON, "i2c dataflow test failed, exit" );
      syslog( LOG_NOTICE | LOG_DAEMON, "try to reset timer with 'pipic -a [address] -c 50' and restart with 'service pipicswd start'" );
      cont = 0;
      exit( EXIT_FAILURE );
    }
  }

  pid_t pid, sid;
        
  pid = fork();
  if( pid < 0 ) 
  {
    exit( EXIT_FAILURE );
  }

  if( pid > 0 ) 
  {
    exit( EXIT_SUCCESS );
  }

  umask( 0 );

  /* Create a new SID for the child process */
  sid = setsid();
  if( sid < 0 ) 
  {
    syslog( LOG_ERR | LOG_DAEMON, "failed to create child process" ); 
    exit( EXIT_FAILURE );
  }
        
  if( chdir("/") < 0 ) 
  {
    syslog( LOG_ERR | LOG_DAEMON, "failed to change to root directory" ); 
    exit( EXIT_FAILURE );
  }
        
  /* Close out the standard file descriptors */
  close( STDIN_FILENO );
  close( STDOUT_FILENO );
  close( STDERR_FILENO );

  FILE *pidf;
  pidf = fopen( pidfile, "w" );

  if( pidf == NULL )
  {
    sprintf( message, "Could not open PID lock file %s, exiting", pidfile );
    syslog( LOG_ERR | LOG_DAEMON, "%s", message );
    exit( EXIT_FAILURE );
  }

  if( flock( fileno(pidf), LOCK_EX | LOCK_NB ) == -1 )
  {
    sprintf( message, "Could not lock PID lock file %s, exiting", pidfile );
    syslog( LOG_ERR | LOG_DAEMON, "%s", message );
    exit( EXIT_FAILURE );
  }

  fprintf( pidf, "%d\n", getpid() );
  fclose( pidf );

// open socket
  int sockfd, connfd, clilen;
  char rbuff[ 25 ];
  char sbuff[ 200] ;
  struct sockaddr_in serv_addr, cli_addr; 

  sockfd = socket( AF_INET, SOCK_STREAM, 0 );
  if( sockfd < 0 ) 
  {
    syslog( LOG_ERR | LOG_DAEMON, "Could not open socket" );
    exit( EXIT_FAILURE );
  }
  else syslog( LOG_NOTICE | LOG_DAEMON, "Socket open" );
  
  memset( &serv_addr, '0', sizeof(serv_addr) );
  memset( sbuff, '0', sizeof(sbuff) ); 

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl( INADDR_ANY );
  serv_addr.sin_port = htons( portno ); 

  if( bind( sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0 )
  {
    syslog( LOG_ERR | LOG_DAEMON, "Could not bind socket" );
    exit( EXIT_FAILURE );
  }
  else syslog( LOG_NOTICE | LOG_DAEMON, "Socket binding successful" );

  listen( sockfd, 1 ); // listen one client only 
  clilen = sizeof( cli_addr );

// initialize switches
  ok = operate_switch1( initswitch1, 0 );
  ok = operate_switch2( initswitch2, 0 );
 
  sleep( 1 );
  ok = read_status();

  int n = 0;
  int hh = 0,mm = 0,wtime = 0;
  while( cont == 1 )
  {
    connfd = accept( sockfd, (struct sockaddr*)&cli_addr, (socklen_t *)&clilen ); 
    if( connfd < 0 ) 
    {
      syslog( LOG_ERR | LOG_DAEMON, "Socket accept failed" );
      exit( EXIT_FAILURE );
    }
    else syslog( LOG_INFO | LOG_DAEMON, "Socket accepted" );

    bzero( rbuff, 25 );
    n = read( connfd, rbuff, 24 );
    if( n < 0 )
    {
      syslog( LOG_ERR | LOG_DAEMON, "Socket reading failed" );
      exit( EXIT_FAILURE );
    }
    else
    {
      sprintf( message, "Received: %s", rbuff );
      syslog( LOG_DEBUG, "%s", message );
    }

    if( strncmp( rbuff, "open 1", 6 ) == 0 )
    {
      if( sscanf( rbuff, "open 1 %d:%d", &hh, &mm ) != EOF )
      {
        wtime = calcwtime( hh, mm );
        ok = operate_switch1( 2, wtime );
      }
      else ok = operate_switch1( 2, 0 ); 
      sleep( 1 );
    } 
    else if( strncmp( rbuff, "close 1", 7 ) == 0 )
    {
      if( sscanf( rbuff, "close 1 %d:%d", &hh, &mm ) != EOF )
      {
        wtime = calcwtime( hh, mm );
        ok = operate_switch1( 1 ,wtime );
      }
      else ok = operate_switch1( 1, 0 );
      sleep( 1 );
    } 
    else if( strncmp( rbuff, "open 2", 6 ) == 0 )
    {
      if( sscanf( rbuff, "open 2 %d:%d", &hh, &mm ) != EOF )
      {
        wtime = calcwtime( hh, mm );
        ok = operate_switch2( 2, wtime );
      }
      else ok = operate_switch2( 2, 0 );
      sleep( 1 );
    } 
    else if( strncmp( rbuff, "close 2", 7 ) == 0 )
    {
      if( sscanf( rbuff, "close 2 %d:%d", &hh, &mm ) != EOF )
      {
        wtime = calcwtime( hh, mm );
        ok = operate_switch2( 1, wtime );
      }
      else ok = operate_switch2( 1, 0 );
      sleep( 1 );
    } 
    else if( strncmp( rbuff, "cancel 1", 8 ) == 0 )
    {
      ok = timer1cancel();
    }
    else if( strncmp( rbuff, "cancel 2", 8) == 0 )
    {
      ok = timer2cancel();
    }

    ok = read_status();
    snprintf( sbuff, sizeof(sbuff), "%s", status );

    n = write( connfd, sbuff, strlen( sbuff ) ); 
    if( n < 0 )
    {
      syslog( LOG_ERR | LOG_DAEMON, "Socket writing failed" );
      exit( EXIT_FAILURE );
    }
    else
    {
      sprintf( message, "Send: %s", sbuff );
      syslog( LOG_DEBUG, "%s", message );
    }

    close( connfd );

    sleep( 1 );
  }

  syslog( LOG_NOTICE | LOG_DAEMON, "remove PID file" );
  ok = remove( pidfile );

  return ok;
}
