#!/usr/bin/perl

use MidWay::NetClient;

$rc = mwattach "srbp://localhost:11000";

print "mwattach => $rc\n";

print "press return to continue --";
getc;

print "Doing call to service \"test\", data \"date\"\n";

@x = mwcall "test", "date";

print "mwcall => (", scalar(@x), "): (", join (",", @x), ")\n";

print "press return to continue --";
getc;

print "Doing call to service \"test\", data \"date\" NOREPLY\n";
@x = mwcall "test", "date", MWNOREPLY();

print "mwcall NOREPLY => (", scalar(@x), "): (", join (",", @x), ")\n";

print "press return to continue --";
getc;

print "detaching...";
mwdetach();
print " bye.\n";

