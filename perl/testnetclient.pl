#!/usr/bin/perl

use MidWay::NetClient;

$rc = mwattach "srbp://localhost:11000";

print "mwattach => $rc\n";

getc;

@x = mwcall "test", "date";

print "mwcall => (", scalar(@x), "): (", join (",", @x), ")\n";

getc;

@x = mwcall "test", "date", MWNOREPLY();

print "mwcall NOREPLY => (", scalar(@x), "): (", join (",", @x), ")\n";

getc;

mwdetach();
