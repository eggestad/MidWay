#!/usr/bin/perl

# The purpose here is to convert all manpages to html.
# We expect all manpages to be in MANPATH/man1, MANPATH/man3 MANPATH/man5
# We will also create the index.html for all the manpages.
$manpath = "../../man";
# the basis is the man2html program really intended fro use by a CGI program.
# synopsis: man2html man3/mwacall.3C 


# Convert does tha main job. It takes two params, the man file (input) and 
# the htmlfile (output) and do a substitute on all the HREFS which become wrong.
# If a manfile just includes another manfile by using .so we substitute.
sub manconvert {
    my $manfile, $htmlfile;
    my @l;

    $manfile = $_[0];
    $htmlfile = $_[1];
    
    # test for .so
    open MAN, $manfile;
    $_ = <MAN>;
    close MAN;
    if (/\.so/) {
	($so, $file) = split ;
	print "Man file $manfile is an alias for $manpath/".$file."\n"; 
	$manfile = $manpath."/".$file;
	&manconvert ($manfile,  $htmlfile);
	return;
    };

    print "Converting $manfile => $htmlfile\n";
    open MAN, "man2html $manfile|";
    open HTML, ">$htmlfile";
    
    # skip first two lines: "Content-type: \n\n"
    <MAN>;
    <MAN>;

    while (<MAN>) {
	if (/http:\/\/localhost/) {
	    
	    $_ =~ s/HREF=\"http\:\/\/localhost\/cgi-bin\/man\/man2html\"/HREF=\"list.html\"/;
	    
	    do {
		@l = m/HREF=\"http\:\/\/localhost\/cgi-bin\/man\/man2html\?(\w+)\+(\w+)\"/; 
		($s, $n) = @l;
		$_ =~ s/http\:\/\/localhost\/cgi-bin\/man\/man2html\?$s\+$n/$n.$s.html/; 
	    }
	    while(scalar(@l) == 2);
	};
	print HTML;
    };
    close MAN;
    close HTML;
    return ;
};

# Over lay for the coinvert function, it takes the dir of the man files, and the
# remainder of the arsg are a list of man files to convert.
# for each manfile we call convert.
# The htmlfiles are left in the current dir.
sub convlist {
    my $dir;
    my @htmllist;
    my @l;
    $dir = shift;
    @l = @_;

    print "Processing $dir\n";
    foreach $f (@l) {
	manconvert $dir."/".$f, $f.".html";
	push @htmllist, $f.".html";
    };
    return @htmllist;
};

# for a given man dir gets the list of all manfiles there, calls convlist, and returns the list.
sub manlist {
    opendir S1, $_[0];
    @sec = readdir S1;
    @sec = grep /\.\d/, @sec;
    closedir S1;
    @ll = convlist  $_[0], @sec;
    return @ll
};

# the actual conversion into html happens here.
@fl1 = manlist "$manpath/man1";
@fl3 = manlist "$manpath/man3";
@fl5 = manlist "$manpath/man5";
print join(", ", @fl1) , "\n";
# now we make the index, both a framed and an unframed version
open  INDEX, ">index.html";
open  LIST, ">list.html";

print INDEX "<html><head><title>MidWay Man Pages</title></head>\n";
print INDEX "<frameset cols=\"20%, *\"> \n";
print INDEX "<frame src=\"list.html\"> <frame src=\"list.html\" name=\"manframe\">\n";
print INDEX "</frameset> \n";

print INDEX "<noframes>\n";
print LIST "<html><head><title>MidWay Man Pages</title></head>\n";

sub writeindexchapter {
    my $FRAMEHDL, $NOFRAMEHDL, $heading;
    $NOFRAMEHDL = shift;
    $FRAMEHDL = shift;
    $heading = shift;
    @fl = @_;

    print $$FRAMEHDL "<h2>$heading</h2>";
    print $$NOFRAMEHDL "<h1>$heading</h1>";
    print $$FRAMEHDL "<ul>\n";
    foreach $x (@fl) {
	$n = $x;
	$n =~ s/\.html//;
	print $$FRAMEHDL "<a href=\"$x\" target=\"manframe\">$n</a><br>\n";
	print $$NOFRAMEHDL "<a href=\"$x\">$n</a><br>\n";
    }
    print $$FRAMEHDL "</ul>\n";
}

writeindexchapter \INDEX, \LIST, "User Commands", @fl1;
writeindexchapter \INDEX, \LIST, "Library API", @fl3;
writeindexchapter \INDEX, \LIST, "File Formats", @fl5;

print INDEX "</noframes></html>\n";
print LIST "</body></html>\n";

close INDEX;
close LIST;

