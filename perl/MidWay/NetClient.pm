#
#  The MidWay Perl Module
#  Copyright (C) 2000 Terje Eggestad
#
#  The MidWay API is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Library General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#  
#  The MidWay API is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Library General Public License for more details.
#  
#  You should have received a copy of the GNU Library General Public
#  License along with the MidWay distribution; see the file COPYING. If not,
#  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
#  Boston, MA 02111-1307, USA. 
#

=head1 NAME

MidWay::NetClient - A MidWay Module for Perl that do SRBP(TCP/IP) client only.


=head1 SYNOPSIS

use MidWay::NetClient;

B<mwattach> "srbp://localhost:12345/mydomain"[, clientname[, userid[, password]]];

B<mwdetach>;

($data, $apprc, $rc ) = B<mwcall> $service[, $data[, $flags]];

$handle = B<mwacall> $service[, $data[, $flags]];

($handle, $data, $apprc, $rc ) = B<mwfetch> [$handle[, $flags]];

=cut

    package MidWay::NetClient;

use IO::Socket;
use Exporter;
use POSIX;

@ISA = ('Exporter');
@EXPORT = qw (&mwattach &mwdetach &mwcall &mwacall &mwfetch 
    &MWSUCCESS &MWFAIL 
    &MWNOREPLY &MWNOBLOCK &MWNOTIME &MWNOTRAN &MWMULTIPLE &MWMORE);



# module wide private variables.
my $connected, $sock, $url;
my $incommingbuffer, $lasthandle;
my $callreplyqueue;

$VERSION = 0.10;
my $SRBVERSION = 0.9;
#my $debugflag = 1;

sub debug {
    if (defined ($debugflag)) {
	print  @_;
    };
};

debug "starting". " now", "\n";;

# a sub for getting the nexthandle, a handle must be between 0x1 and 0x7fffffff
sub nexthandle {
    if (!defined($lasthandle)) {
	$lasthandle = rand 0x7fffffff; 
    };

    # wraparound
    if ( ($lasthandle > 0x7fffffff) || ($lasthandle < 1) ) {
	$lasthandle = 0x1;
    };

    return $lasthandle++;
};

# The url escape and unescape functions are canibalized from CGI.pm
# Copyright 1995-1998 Lincoln D. Stein.  All rights reserved.

# unescape URL-encoded data
sub unescape {
    shift() if ref($_[0]);
    my $todecode = shift;
    return undef unless defined($todecode);
    $todecode =~ tr/+/ /;       # pluses become spaces
    $todecode =~ s/%([0-9a-fA-F]{2})/pack("c",hex($1))/ge;
    return $todecode;
}

# URL-encode data
sub escape {
    shift() if ref($_[0]) || $_[0] eq $DefaultClass;
    my $toencode = shift;
    return undef unless defined($toencode);
    $toencode=~s/([^a-zA-Z0-9_-])/uc sprintf("%%%02x",ord($1))/eg;
    return $toencode;
}

# this funcs are styled after my uelencode C library.
sub urlmapdecode {
    my $command, $list, $pair, @pairs, %list, $key, $value;
    my $str  = $_[0];

    if (!defined ($str)) {
	return undef;
    };  

    $str =~ s/\r\n//;
    ($command, $list) = split (/\?\!\./, $str );    

    if (!defined ($list)) { $list = $map;};
    @pairs = split (/\&/, $list);
    foreach $pair (@pairs) { 
	my $k;
	($key, $value) = split (/=/, $pair);
	$list {lc(unescape($key))} = unescape($value);
    };
    return %list;
};

sub urlmapencode {
    my %list = @_;
    return undef unless defined (%list);
    my @keys, $key, $notfirst = undef, $list;

    
    @keys = keys(%list);
    undef $list;
    foreach $key (@keys) {
	if (defined($notfirst)) {
	    $list .= "&";
	} else {
	    $notfirst = 1;
	};
	if (defined ($list{$key})) {
	    $list .= escape($key)."=".escape($list{$key});
	} else {
	    $list .= escape($key);
	};
    };
    return $list;
};

################################################################################
#
# now we define a function for each message command that may occur, or
# atleast that we wish to deal with.
#
################################################################################

sub term {
    my $marker, $map;
    $marker = shift;
    $map = shift;

    if ( ($marker eq "!") || ($marker eq ".") ) {
	;
    } elsif ($marker eq "?") {
	print $sock "TERM.";
    } else {
	#reject
	debug "reject on term illegal marker \"$marker\"\n";
    };
    debug "TERM send.\n";
    close $sock;
    undef $sock;
};

sub reject {
    my $marker, $map;
    $marker = shift;
    $map = shift;
    # ignore
    debug "we ignore rejects: reject$marker$map\n";
};

sub svccall {
    my $marker, $map, %map, $hdl;
    $marker = shift;
    $map = shift;

    if ($marker ne ".") {
	debug "we ignore call requests: svccall$marker$map\n";
	return;
    };

    %map = urlmapdecode $map;

    if (!defined ($map{'handle'})) {
	debug "svccall missing handle: svccall$marker$map\n";
    } else {
	$hdl = $map{'handle'};
	$callreplyqueue{$hdl} = \%map;
    };
};
    
sub svcdata {
    my $marker, $map, %map;
    $marker = shift;
    $map = shift;
    
    debug "we ignore call data: svcdata$marker$map\n";
};


# this function is pretty paramount. It retrives everything queued in
# the TCP queue, and calls the functions that deals with the
# individual commands.

sub drainsock {
    return undef unless defined ($sock);
    my $br, $buf, $command, $marker, $rest;
    my $m, @messages, %map;

    # if we have an arg, it means that we are in blocking wait, but we
    # only block on the first read.

    debug "drainsock begins.\n";

    if (defined ($_[0])) {
	debug "drainsock switch to blocking mode.\n";
	fcntl ($sock, F_SETFL(), 0);
    };
    
    do {
	$br = sysread ($sock, $buf, 9000);
	if ($br > 0) {
	    $incommingbuffer .= $buf;
	};
	
	if (defined ($_[0])) {
	    fcntl ($sock, F_SETFL(), O_NONBLOCK());
	};

	# test on eof
	if ( (!defined ($br)) && (int($!) != EAGAIN) ) {
	    $br = 0;
	    undef $sock;
	};
    } until ($br == 0);

    # we split on CRNL
    @messages = split (/\r\n/, $incommingbuffer);

    # test to see if the last message is uncomplete
    if (substr($incommingbuffer, -2) ne "\r\n") {
	$incommingbuffer = pop @messages;
    } else {
	undef $incommingbuffer ;
    };

    # foreach message split it on command, maker , and the rest.
    # then call the corresponding subroutine.
    foreach $m (@messages) {
	my @mesgcomp;
	debug "Prossessing message $_\n";
	@mesgcomp = $m =~ m/(\w+)([\!|\?|\.\~])(.*)/s;
	if (@mesgcomp < 2) {
	    die "SRB Protocol ERROR: Unable to parse this SRBP message: \"$_\"";
	};
	($command, $marker, $rest) = @mesgcomp;
	$command = lc $command;

	debug "Command = $command\n";
	#
	# We handle rejects on a general basis
	#
	if ($marker eq "~") { 
	    %map = urlmapdecode $rest;
	    print $sock "TERM!\r\n";
	    close $sock;
	    undef $sock;
	    die "SRB Protocol ERROR: $command verb was rejected from peer with\nREASONCODE="
		.$map{'reasoncode'}." REASON=\"".$map{'reason'}."\"\n"
		    ."CAUSEFIELD=".$map{'causefield'}." CAUSEVALUE=".$map{'causevalue'};
	};
	    
	if (defined (&$command)) {
	    &$command ($marker, $rest);
	} else {
	    #reject
	};
    };
};


################################################################################
#
##########                   MAIN API BEGINS HERE                     ##########
#
################################################################################

=head1 DESCRIPTION

This module is a all Perl coded module, in order to have an API that do not 
require a shared library in native code. A more general module built on the C 
library will provide a much richer functionality, like writing server/services in Perl.
This module only implement the I<S>ervice I<R>equest I<B>roker I<P>rotocol B<SRBP>.

=head2 mwattach

The B<mwattach> function establishes a TCP/IP connection to the Service Request
broker, and perform a user login is needed.
The  I<url> argument is required. The url must be a srbp url
on the format [srbp:][//ipaddress[:port]][/domain]. Either one or both of
ipaddress and domain must be specified.

The clientname argument will be "MidWay::NetClient" if ommitted.

The username and passwords are only needed if the SRB gateway is set up to require
authentication. In this case B<mwattach> will fail.

Only one SRB may be attached at any given time.

=cut
sub mwattach {
    if (defined $sock) {
	return 1;
    };
    
    if ((@_ < 1) || (@_ > 5)) {
	die "mwattach: wrong number of arguments";
    };

    my $url = shift;
    my $cltname = shift;
    my $username = shift;
    my $password = shift;
    my @url;
    my $proto = srb, $host, $port = 12345;

    ########## decoding url ##########

    # trying srb://host:port
    @url = $url =~ m/^(\w+)\:\/\/([\w\.\-]*)\:(\w+)$/s;
    if (scalar(@url) == 3) {
	($proto, $host, $port) = @url;
	debug "1-> $proto://$host:$port <= ", join (";", @url), "\n";
    } else {
	# trying srb://host
	@url = $url =~ m/(\w+)\:\/\/([\w\.\-]*)/s;

	if (scalar(@url) == 2) {
	    ($proto, $host) = @url;
	    debug "2-> $proto://$host:$port <= ", join (";", @url), "\n";
	} else {
	    # trying host:port
	    @url = $url =~ m/^([\w\.\-]*)\:(\w+)$/s;	
	    if (scalar(@url) == 2) {
		($host, $port) = @url;
		debug "3-> $proto://$host:$port <= ", join (";", @url), "\n";;
	    } else {
		# trying host
		@url = $url =~ m/^([\w\.\-]*)$/s;
		if (scalar(@url) == 1) {
		    ($host) = @url;
		    debug "4-> $proto://$host:$port <= ", join (";", @url), "\n";
		} else {
		    die "ullegal url: $url\n";
		};
	    };
	};
    };
    if (lc($proto) ne "srbp") {
	die "Unsupported protocol \"$proto\"";
    };

    ########## connecting socket ##########

    debug "connecting to host:$host on port:$port using protocol:$proto\n";
    $sock = new IO::Socket::INET(PeerAddr => $host,
			     PeerPort => $port,
			     Proto => 'tcp'
			     );
    die "failed to connect socket reason: $!" unless $sock;

    ########## reading SRB INIT ##########
    # note this is blocking wait.
    my $initstr, $command, $marker, $map, %map;

    alarm (10);
    $initstr = <$sock>;
    alarm (0);
    ($command, $marker, $map) = $initstr =~ m/([\w\s]+)(\!|\?|\.)(.*)/s;
    if ( lc($command) ne "srb ready") {
	die "wrong greeting from SRB: probably wrong port";
    };
    if ($marker ne "!") {
	die "wrong greeting from SRB: probably wrong port";
    };

    ########## processing srb init (what shall we do??
    %map = urlmapdecode $map;
    if (defined (%map)) {
	if (scalar(keys(%map)) == 0) {
	    die "error in decoding srb ready message";
	};
	debug "keys: ", keys(%$map), scalar keys(%$map), ",,\n";
    } else {
	die "error in decoding srb ready message";
    };
   
    debug "connected to a SRB server \"".$map{'agent'}."\" of version ",
    $map{'agentversion'}." protcol version ".$map{'version'}, 
    " domain ". $map{'domain'}, "\n";;
    
    ########## generate SRB INIT request
    undef %map;

    $map{'VERSION'} = $SRBVERSION;
    $map{'TYPE'} = "client";
    if (!defined ($cltname)) { $cltname = "perl"; }
    $map{'NAME'} = $cltname;
    if ($username) { 
	$map{'USER'} = $username; 
    }
    if ($password) { 
	$map{'PASSWORD'} = $password; 
	$map{'AUTHENTICATION'} = 'unix'; 
    };
    $map{'AGENT'} = "Perl MidWay::NetClient";
    $map{'AGENTVERSION'} = $VERSION;
    if (defined ($^O)) {
	$map{'os'} = $^O;
    };
    print $sock "SRB INIT?".urlmapencode(%map)."\r\n";

    ########## wait for SRB INIT reply, and process it

    debug "waiting fro reply\n";
    $initstr = <$sock>;
    debug "$initstr\n";
    ($command, $marker, $map) = $initstr =~ m/([\w\s]*)(\!|\?|\.)(.*)/s;

    debug ("command = $command marker = $marker map = $map \n");
    if ( (lc($command) ne "srb init") || ($marker ne ".") ) {
	die "unexpected message from server: $initstr\n";
    };
    
    %map = urlmapdecode $map;
    
    if ($map{'rc'} == 0) {
	fcntl ($sock, F_SETFL(), O_NONBLOCK());
	return 1;
    }
    close $sock;
    undef $sock;
    debug "mwattached rejected from server reason: $map{'rc'}\n";
    return undef;
};

=head2 mwdetach

B<mwdetach> disconnects a SRB connected thru B<mwattach>.

=cut
sub mwdetach {
    if (defined ($sock)) {
	debug "Disconnecting\n";
	print $sock "TERM!\r\n";
	close $sock;
	undef $sock;
    };
};

=head2 mwacall

B<mwacall> is the asyncronous service call. This function return before the reply
is received from the service, it returns an unique handle that may be given to
B<mwfetch> in order to retrive the reply laters. This function allows you to 
issue several service request simultaneously.

The Service name is required and is resticted to 32 bytes, alphanumeric plus B</._>.

The data is optional, some services only return data. In this version, data is
restricted to 3000 bytes.

The legal flags are B<MWMULTIPLE>, B<MWNOBLOCK>, B<MWNOREPLY>, B<MWNOTIME> 
and B<NOTRAN>, see L<"FLAGS">. 

=cut

sub mwacall {
    my $map, %map;
    my $svcname, $data, $flags;
    my $marker, $handle;

    debug "mwacall begins\n";
    $svcname = shift;
    $data = shift;
    $flags = shift;

    undef %map;

    if (!defined ($svcname)) {
	die "mwacall called without any service name";
    };

    # NB: Just until we implement large data
    if (length($data) > 3000) {
	die "data too long (max 3000)";
    };
    
    $map{'svcname'} = $svcname;
    if (defined ($data)) {
	$map{'data'} = $data;
    };
    $map{'hops'} = 0;

    if ($flags & MWMULTIPLE()) {
	$map{'MULTIPLE'} = "YES";
    };
    if ($flags & MWNOREPLY()) {
	$marker = "!";
	$handle = undef;
    } else {
	$marker = "?";
	$handle = sprintf "%8.8x", nexthandle();
	$map{'handle'} = $handle;
    };
    if ($flags & MWMULTIPLE()) {
	$map{'MULTIPLE'} = "YES";
    };
    
    $map = urlmapencode %map;
    
    print $sock "SVCCALL$marker$map\r\n";
    debug "SVCCALL$marker$map\r\n";

    debug "mwacall completes\n";
    
    return $handle;
};

=head2 mwfetch

B<mwfetch> is used to retrive the service reply from a previous B<mwacall>. 
It the I<handle> argument is omitted, the first available reply is returned.

The only legal flags are B<MWNOBLOCK>.

B<mwfetch> returns a list with 4 elements, the handle, data if the
service routine returned any, a return code, and an application
returncode.  The return code is either B<MWFAIL> is the service
routine failed, B<MWSUCCESS> if success and B<MWMORE> is the service
routine are sending more replies (this was not the last).
If the B<MWNOBLOCK> flags was set, and no reply was available, it return undef.
Later version that support deadlines, will also return undef if the deadline 
expired.

=cut

sub mwfetch {
    my $handle, $rdata, $rc, $apprc, $flags, $ref;
    my @handleinqueue, %map;


    $handle = shift;
    $flags = shift;

    debug "mwfetch begins, handle=$handle sock=$sock\n";
    
    # first we empty the socket, non blocking
    drainsock();
    debug "first drainsoc complete sock=$sock\n";

    # we must deal with "first available" seperatly
    if (!defined $handle ) {
	@handleinqueue = keys %callreplyqueue;

	# if no block return undef if there is nothing in the queue
	if ( ($flags & MWNOBLOCK()) && (scalar (@handleinqueue) == 0) ) {
	    return undef;
	};
	# if there are nothingin the queue, do blocking wait on socket
	while (scalar (@handleinqueue) == 0) {
	    drainsock(1);
	};
	$handle = shift @handleinqueue;

    } else {
	debug "mwfetch 3 sock=$sock\n";
	
	if ( ($flags & MWNOBLOCK()) && (!defined ($callreplyqueue{$handle})) ) {
	    return undef;
	} else {
	    while (!defined ($callreplyqueue{$handle})) {
		debug "mwfetch 4.1 sock=$sock\n";
		drainsock(1);
		debug "mwfetch 4.2 sock=$sock\n";
	    };
	};
    };

    $ref = $callreplyqueue{$handle};

    %map = %$ref;
    if ($map{'returncode'} == 0) {
	$rc = MWSUCCESS();
    } else {
	if ($rc == 430) { $@ = "No such service";};
	if ($rc == 431) { $@ = "Service failed";};
	$rc = MWFAIL();
    };

    $apprc = $map{'applicationrc'};
    $data = $map{'data'};

    delete $callreplyqueue{$handle};;
    
    debug "mwfetch completes sock=$sock\n";

    return ($handle, $rc, $apprc, $data);
};

=head2 mwcall

B<mwcall> is the syncronous service request, and will not return until the
reply is received, or an error occured.

B<mwcall> returns a list with 3 elements, data if the
service routine returned any, a return code, and an application
returncode.  The return code is either B<MWFAIL> is the service
routine failed, B<MWSUCCESS> if success.
It returns undef in case of an error.

An error may happen if the B<MWNOBLOCK> flags was set, and a blocking
condition occured B<mwcall> return undef.  Later versions that support
deadlines, will also return undef if the deadline expired.

=cut

sub mwcall {
    my $handle, @rlist;

    $handle = mwacall @_;

    if (defined ($handle)) {
	@rlist = mwfetch $handle;
	if (scalar(@rlist) != 4) {
	    return undef;
	} else {
	    return @rlist[1,2,3];
	};
    } else {
	return undef;
    };
};

=head1 FLAGS AND CONSTANTS

=over 4

=item MWNOREPLY()

Legal only in B<mwacall>, and signal that we expect no reply from the service.
It not way to know if the service even processed the request.

=item MWNOBLOCK()

There are certian conditions that caused the client program to enter blocking wait, 
such as the TCP queue full, or nothing to read from the TCP connection.
This flag specify that the function shall not enter blocking wait, but fail.

=item MWNOTRAN()

This B<mw(a)call> are not apart of any ongoing transaction.
I<NOT YET IMPLEMENTED>.

=item MWNOTIME()

If a deadline has been set, this B<mw(a)call> is not bound by it.

=item MWMULTIPLE()

Used only in B<mwacall>, and signalls the service routing that it is OK
to send multiple replies.

=item MWMORE()

Returncode from B<mwfetch> if the service routine signals that more replies are comming.

=item MWSUCCESS()/MWFAIL()

Returncode from B<mwfetch> or B<mwcall>, the service routine signal
weither or not it succeded.
=back

=cut
# these functions return the flags, must be functions since we don't
# have #define in perl.

sub MWNOREPLY  { return 0x0001; };
sub MWNOBLOCK  { return 0x0002; };
sub MWNOTIME   { return 0x0004; };
sub MWNOTRAN   { return 0x0010; };
sub MWMULTIPLE { return 0x0100; };
sub MWMORE     { return 0x0200; };

sub MWSUCCESS { return 0x1; };
sub MWFAIL    { return 0x0; };


1;


