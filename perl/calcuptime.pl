#!/usr/bin/perl -w
# 
# Calculate power up time
# Usage: calcuptime < file.txt

use strict;
use POSIX;
use Date::Parse;

my $line="";
my $dtime1="";
my $dtime2="";
my $unxs1=0;
my $unxs2=0;
my $unxs0=0;
my $dt=0;
my $uptime=0;
my $upercs=0;
my $tot=0;
my $hh=0;
my $mm=0;
my $ss=0;

use integer;

$line=<>;
while($line) 
{
  if($line =~ /(\d{4})-(\d{2})-(\d{2})\s(\d{2}):(\d{2}):(\d{2})\s+pipicpowerd v.*/) 
  {
    $line=<>;
    while(($line)&&($line !~ /read voltage/))
    {
      $line=<>;
    }
    if($line =~/(\d{4})-(\d{2})-(\d{2})\s(\d{2}):(\d{2}):(\d{2})\s+read voltage.*/) 
    {
      $dtime1=$1."-".$2."-".$3." ".$4.":".$5.":".$6;
      $unxs1=str2time("$1-$2-$3 $4:$5:$6");
#      print "$dtime1  $unxs1  start\n";
    }
  }
  if($line =~ /(\d{4})-(\d{2})-(\d{2})\s(\d{2}):(\d{2}):(\d{2})\s+remove PID file.*/)
  { 
    $dtime2=$1."-".$2."-".$3." ".$4.":".$5.":".$6;
    $unxs2=str2time("$1-$2-$3 $4:$5:$6");
    $dt=$unxs2-$unxs1;
    if($dt<30*24*3600)
    {
      $uptime+=$dt; 
      $hh=$dt/3600;
      $mm=$dt%3600;
      $ss=$mm%60;
      $mm/=60;
      print (sprintf "%s %s  %02d:%02d:%02d\n", $dtime1, $dtime2, $hh, $mm, $ss);
      $unxs0=$unxs1 if($unxs0==0);
    }
#    print "$dtime  $unxs2  $dt  $hh:$mm:$ss  stop\n";
  }
  $line=<>;
}

$hh=$uptime/3600;
$mm=$uptime%3600;
$ss=$mm%60;
$mm/=60;
print (sprintf "%02d:%02d:%02d uptime ", $hh, $mm, $ss);

$tot=$unxs2-$unxs0;

$upercs=100*$uptime/$tot;

$hh=$tot/3600;
$mm=$tot%3600;
$ss=$mm%60;
$mm/=60;
print (sprintf "from total %02d:%02d:%02d or %-3.0f%% of up time\n", $hh, $mm, $ss, $upercs);

