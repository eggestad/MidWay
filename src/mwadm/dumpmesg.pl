#!/usr/bin/perl

msgrcv $ARGV[0], $x, 1024, 0, 0; 
while ($c = chop $x) {
    unshift @message, ord($c);
} 

foreach $x ( @message) {
    printf "%.2x ", $x;
};

print "\n";
