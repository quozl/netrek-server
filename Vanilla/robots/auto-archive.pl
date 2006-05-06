#!/usr/bin/env perl
#
# $Id: auto-archive.pl,v 1.2 2006/05/04 23:39:58 ahn Exp $
# 
# auto-archive.pl
#
# There is a central game stats archive at http://www.netrek.org/stats/
# By default, all clue game stats are sent to this archive.  This is
# in addition to the local stats repository and the statboy mailer
# handle by end_tourney.pl.  To never send stats to the central archive,
# uncomment $archiver below.

# Note that this requires Internet working sendmail config with DNS
# resolver.

# Game stats (including ltd_dump.txt, pwstats.html and INL_log) are
# sent immediately.  The cambot file (if it exists) is sent afterwards
# in a background process.

use strict;

#################################
###### BEGIN CONFIGURATION ######
#################################

######
# If you don't have Net::Domain, comment it out and change
# $servername below.

use Net::Domain qw(hostfqdn);

######
my $archiver;

######
# Archiver address.  DO NOT CHANGE.
# To never send stats to the central archive, comment it out.

$archiver	= 'stats-archive@orion.netrek.org';

######
# Name of your clue server.  Visit http://www.netrek.org/stats/ and
# make sure this name doesn't clash with another server.  If you have
# multiple servers on one machine, add port numbers.  By default,
# the FQDN is used.  Examples:
# $servername = 'twink.crackaddict.com';
# $servername = 'pickled.fox.cs.cmu.edu_1111_2222';

my $servername	= hostfqdn();
#my $servername	= 'unknown';

######
# By default, cambot files are sent if they exist.  Cambot recordings
# can and do exceed 15MB before compression, so they are first gzipped
# then uuencoded before being sent.  This can take quite some time.
# To never send the cambot file, set it to 0.

my $send_cambot	= 1;

######
# Required programs.  Set to full path if necessary.
# Note: $Mail must be able to take -s parameter for subject.

my $Mail	= 'Mail';
my $uuencode	= 'uuencode';
my $gzip	= 'gzip';
my $tar		= 'tar';


#################################
####### END CONFIGURATION #######
#################################

# No need to change anything below.

#####
# Files to archive

my $ltd_dump	= 'ltd_dump.txt';
my $pw_stats	= 'pwstats';
my $inl_log	= 'INL_log';
my $cambot	= 'cambot.pkt';

#####
# Parse any args

my $fork = 0;
my $id = 0;

foreach my $argv (@ARGV) {
  if ($argv eq "-f") {
    $fork = 1;
  }
  else {
    $id = $argv;
  }
}

if (!$id) {
  print STDERR "Usage: auto-archive.pl [-f] id\n";
  exit 1;
}

if (!$servername) {
  $servername = 'unknown';
}

$ltd_dump	.= ".$id";
$pw_stats	.= ".$id.html";
$inl_log	.= ".$id";
$cambot		.= ".$id";


######
# Sanity checking

if (!$archiver) {
  print STDERR "auto-archive.pl: auto archiving disabled\n";
  exit 0;
}

if (! -e $ltd_dump) {
  print STDERR "auto-archive.pl: warning: no $ltd_dump\n";
  $ltd_dump = '';
}

if (! -e $pw_stats) {
  print STDERR "auto-archive.pl: warning: no $pw_stats\n";
  $pw_stats = '';
}

if (! -e $inl_log) {
  print STDERR "auto-archive.pl: warning: no $inl_log\n";
  $inl_log = '';
}

if (! -e $cambot) {
  $send_cambot = 0;
}

if (!$ltd_dump && !$pw_stats && !$inl_log && !$send_cambot) {
  print STDERR "auto-archive.pl: nothing to archive\n";
  exit 0;
}

######
# Send the smaller files directly.

if ($ltd_dump || $pw_stats || $inl_log) {
  system("$tar cf - $ltd_dump $pw_stats $inl_log | " .
         "$gzip -9c | " .
         "$uuencode $servername.stats.$id.tar.gz | " .
         "$Mail -s \"Game stats from $servername\" $archiver"
         );
}

if ($send_cambot) {
  my $pid;

  if ($pid = fork()) {
    exit 0;
  }
  elsif (defined $pid) {
    $SIG{CHLD} = 'CHILD';

    system("$gzip -9c $cambot | " .
           "$uuencode $servername.cambot.$id.gz | " .
           "$Mail -s \"Cambot recording from $servername\" $archiver");
    exit 0;
  }
  else {
    die("auto-archive.pl: fork failed");
  }
}

