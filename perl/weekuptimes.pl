#!/usr/bin/perl -w
# 
# Calculate power up time for one week periods.
# Usage: weekuptimes < pipicpowers.log

use strict;
use POSIX;

my $line="";
my $dat="";
my $tim="";
my $unxs0=0;
my $unxs1=0;
my $timer0=0;
my $timer1=0;
my ($Vmin,$Vave,$Vmax);
my ($Tmin,$Tave,$Tmax);
my ($Tcpumin,$Tcpuave,$Tcpumax);
my $wifiup=0;

my $now=`/bin/date +%s --date='2014-11-03 00:00:00'`;
my $weekago=$now-7*24*3600;
my $weekno=`/bin/date +%V --date='2014-11-02 23:59:59'`;

my $Vmi=100;
my $Vav=0;
my $Vmx=-100;
my $Tmi=100;
my $Tav=0;
my $Tmx=-100;
my $Tcpumi=100;
my $Tcpuav=0;
my $Tcpumx=-100;
my $dt=0;
my $uptime=0;
my $hh=0;
my @lines=<>;

for(my $j=0;$j<20;$j++)
{
  foreach $line (@lines)
  {
    $dt=0;
    chomp $line; 
    ($dat,$tim,$unxs0,$unxs1,$timer0,$timer1,$dt,$Vmin,$Vave,$Vmax,$Tmin,$Tave,$Tmax,$Tcpumin,$Tcpuave,$Tcpumax,$wifiup) = split /\s+/,$line;
    if(($unxs1>=$weekago)&&($unxs1<$now))
    {
      if($dt>0)
      {
        $uptime+=$dt;
        $Vmi=$Vmin if($Vmi>$Vmin);
        $Vmx=$Vmax if($Vmx<$Vmax);
        $Vav+=$Vave*$dt;
        $Tmi=$Tmin if($Tmi>$Tmin);
        $Tmx=$Tmax if($Tmx<$Tmax);
        $Tav+=$Tave*$dt;
        $Tcpumi=$Tcpumin if($Tcpumi>$Tcpumin);
        $Tcpumx=$Tcpumax if($Tcpumx<$Tcpumax);
        $Tcpuav+=$Tcpuave*$dt;
      }
#      print "$dat $tim $dt $Vmi $Vave $Vmx $Vav $uptime\n";
    }
  }

  if($uptime>0)
  {
    $Vav/=$uptime;
    $Tav/=$uptime;
    $Tcpuav/=$uptime;
  }
  else
  {
    $Vav=0;
    $Vmi=0;
    $Vmx=0;
    $Tav=0;
    $Tmi=0;
    $Tmx=0;
    $Tcpuav=0;
    $Tcpumi=0;
    $Tcpumx=0;
  }

  $hh=$uptime/3600;
  print (sprintf "%02d %5.1f", $weekno, $hh);
  print (sprintf " %4.1f %4.1f %4.1f  %+3.0f %+3.0f %+3.0f  %3.0f %3.0f %3.0f\n",$Vmi,$Vav,$Vmx,$Tmi,$Tav,$Tmx,$Tcpumi,$Tcpuav,$Tcpumx);

  $Vmi=100;
  $Vav=0;
  $Vmx=-100;
  $Tmi=100;
  $Tav=0;
  $Tmx=-100;
  $Tcpumi=100;
  $Tcpuav=0;
  $Tcpumx=-100;

  $uptime=0;
  $now-=7*24*3600; 
  $weekago=$now-7*24*3600;
  $weekno--;
}
