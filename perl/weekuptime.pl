#!/usr/bin/perl -w
# 
# Calculate power up time from last week or seven days.
# Usage: weekuptime < pipicpowers.log

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

use integer;

my $now=time;
my $weekago=$now-7*24*3600;

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

$line=<>;
while($line) 
{
  chomp $line; 
  ($dat,$tim,$unxs0,$unxs1,$timer0,$timer1,$Vmin,$Vave,$Vmax,$Tmin,$Tave,$Tmax,$Tcpumin,$Tcpuave,$Tcpumax,$wifiup) = split /\s+/,$line;
  $dt=0;
  if($unxs1>=$weekago)
  {
    print "$dat $tim";
    if($unxs0>$weekago)
    {
#      print " $unxs0 $unxs1";
      $dt=$unxs1-$unxs0;
    }
    else
    {
#      print " $weekago $unxs1";
      $dt=$unxs1-$weekago;
    }
    print " $dt\n";
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
  }
  $line=<>;
}

my $hh=$uptime/3600;
my $mm=$uptime%3600;
my $ss=$mm%60;
$mm/=60;
print (sprintf "%02d:%02d:%02d ", $hh, $mm, $ss);

my $tot=7*24*3600;

my $upercs=100*$uptime/$tot;

$hh=$tot/3600;
$mm=$tot%3600;
$ss=$mm%60;
$mm/=60;
print (sprintf "from total %02d:%02d:%02d or %-3.0f%% of up time.", $hh, $mm, $ss, $upercs);

$Vav/=$uptime;
$Tav/=$uptime;
$Tcpuav/=$uptime;
print (sprintf " %4.1f/%4.1f/%4.1f V, %+3.0f/%+3.0f/%+3.0f °C and CPU %3.0f/%3.0f/%3.0f °C.\n",$Vmi,$Vav,$Vmx,$Tmi,$Tav,$Tmx,$Tcpumi,$Tcpuav,$Tcpumx);
 
