#!/usr/bin/perl 


$clientsize = 13*4 + 65 + 65 +2 ;
$serversize = 68 + 7*4 + 5*12;
$servicesize = 6*4 + 68 + 36; 

shmread 1, $data, 0, 176; 
@ipcmain = unpack "a8iiiiia68iiiiiiiiiillllliiiii", $data;

print "Magic $ipcmain[0] Version $ipcmain[1].$ipcmain[2].$ipcmain[3]\n";
print "mwd: pid=$ipcmain[4] message queue id = $ipcmain[5]\n";
print "system name: $ipcmain[6]\n";
print "heap          shmid = $ipcmain[11]\n";
print "client  table shmid = $ipcmain[12]\n";
print "server  table shmid = $ipcmain[13]\n";
print "service table shmid = $ipcmain[14]\n";
print "gateway table shmid = $ipcmain[15]\n";
print "conv    table shmid = $ipcmain[16]\n";

for ($i=0; $i < $ipcmain[22]; $i++) {
    shmread $ipcmain[12], $cltrow, $i*$clientsize, $clientsize;
    @clt = unpack "iiiiiiia65a65a2iiiii", $cltrow;
    print join("&",@clt),"\n";
#    print "idx=$i type=$clt[0] status=$clt[2] pid=$clt[3]\n";
};
shmread $ipcmain[13], $srvtbl, 0, $ipcmain[23]*$serversize;
shmread $ipcmain[14], $svctbl, 0, $ipcmain[24]*$servicesize;

