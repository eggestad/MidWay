
struct managed_server_t {

   char * name;
   char * fullpath;
   char * const * envp;
   int status;
   time_t starttime;
   int fd;
   pid_t mgrpid, serverpid;
   pid_t pgid;
};

enum srvstatus {
   server_status_notrunning = 0,
   server_status_startup = 1,
   server_status_running = 2,
   server_status_shutingdown = 9,
   server_status_stopped = 10,
};

enum msgtype {
   msg_type_startup = 1,
   msg_type_restart = 2,
   msg_type_exit = 3,
   msg_type_killed = 4
};

typedef union  {

   struct {
      int mtype;
   } type;

   struct {
      int mtype;
      pid_t pgid;
      pid_t server_pid;
   } startup;

   struct {
      int mtype;
      int exitcode;
      pid_t server_pid;      
   } exit;

   struct {
      int mtype;
      int signal;
      pid_t server_pid;
   } killed;
   
} srvmgr_messages_t;

//
// singelServerManager.c
//
int triggershutdown(int signal);
int initSingleServerManager(struct managed_server_t * server, int mwdfd);
int runSingleServerManager(struct managed_server_t * server, int childfd);
int runSingleServerManagerLoop(struct managed_server_t * server, int childfd);
