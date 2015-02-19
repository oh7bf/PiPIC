#include "testi2c.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <time.h>
#include <syslog.h>
#include "pipichbd.h"
#include "writecmd.h"
#include "readdata.h"

// send 4 test bytes to PiPIC and read them back
int testi2c()
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

  ok=write_cmd(0x02,testint,4);

  if(ok!=1) syslog(LOG_ERR, "failed to write 4 test bytes"); 
  else
  {
    testres=read_data(4);

    if((testres==-1)||(testres==-2)||(testres==-3)||(testres==-4))
      syslog(LOG_ERR, "failed to read 4 test bytes"); 
    else
    {
      if(testint==testres) ok=1; else ok=0;
    }
  }

  return ok;
}

