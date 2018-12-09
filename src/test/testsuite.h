
/* this is a structure to give the testsuite servoces instution to how
   they shal simulate a real life service.
*/


struct testdata 
{
  int opcode;

  int msleep;
  int fsyncs;
  int writeamount;
  int randomreadamount;
  int mspinn;

  struct timeval starttv;
  struct timeval endtv;

  int forward;
  char forwardto[64];

};
  
int  chargen (char * buffer, size_t buflen) ;
