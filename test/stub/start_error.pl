#!/usr/bin/env perl
use strict;
use IO::Handle;

STDOUT->autoflush(1);
STDERR->autoflush(1);

my $workDir = $ENV{PASSENGER_SPAWN_WORK_DIR};

my $f;
open($f, "> $workDir/response/finish");
print $f '0';
close($f);

if ($ARGV[0] eq 'freeze') {
	sleep(1000);
} else {
	print("He's dead, Jim!\n");
	print("Relax, I'm a doctor.\n");
}
