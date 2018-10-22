#include <iostream>
#include <string>
#include <map>

#include <node_api.h>
#include <uv.h>

#include <assert.h>

#include <stdio.h>
#include <unistd.h>

#include <MidWay.h>
#include "mw_wrapper.h"


#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>

namespace MidWay {

  
   /*
    * hash to store mapping between service name and a refernce to the call back function
    */ 
   std::map<std::string, napi_ref> ref_servicemap;


   uv_thread_t mw_server_thread;
   uv_async_t service_request_event;
   uv_loop_t * loop;

   // sync variables 
   uv_mutex_t thread_wait_mutex;
   uv_cond_t thread_wait_cond;

   
   // used when entering main loop to store the env for use in servicecallwrapper()
   napi_env envInMainLoop;



   /*
    * Now we're getting to brass tax. 
    * This is a the C call back for all service requests. 
    * It looks up the javascript callback and calls its. 
    */ 
   
   int servicecallwrapper (mwsvcinfo * msi) {

      mwlog(MWLOG_DEBUG2,  (char*) "Beginning service call wrapper");
      
      napi_status status;
      napi_value svcreqinfo;
      
      napi_env env = envInMainLoop;
      mwlog(MWLOG_DEBUG2,  (char*) " env %p", env);


      /*

	I'm a bit mystified why it is necessaryto open a handle scope. 
	(STill iffy on what a handlescope is) 
	But with out opening a handle scope we get this stack trace in napi_create_object below

	FATAL ERROR: v8::HandleScope::CreateHandle() Cannot create a handle without a HandleScope
	1: node::Abort() [node]
	2: 0xf8bef2 [node]
	3: v8::Utils::ReportApiFailure(char const*, char const*) [node]
	4: v8::internal::HandleScope::Extend(v8::internal::Isolate*) [node]
	5: v8::Object::New(v8::Isolate*) [node]
	6: napi_create_object [node]
	7: MidWay::servicecallwrapper(mwsvcinfo*) [/home/terje/work/MidWay/node.js/build/Release/midway.node]
	8: _mwCallCServiceFunction [/usr/local/lib64/libMidWay.so.0]
	9: MidWay::server_MidWay_mainlooplet() [/home/terje/work/MidWay/node.js/build/Release/midway.node]
	10: MidWay::service_request_callback(uv_async_s*) [/home/terje/work/MidWay/node.js/build/Release/midway.node]
	11: 0x7ff5b9c2fad8 [/lib64/libuv.so.1]
	12: uv__io_poll [/lib64/libuv.so.1]
	13: uv_run [/lib64/libuv.so.1]
	14: node::Start(uv_loop_s*, int, char const* const*, int, char const* const*) [node]
	15: node::Start(int, char**) [node]
	16: __libc_start_main [/lib64/libc.so.6]
	17: _start [node]

      */
      napi_handle_scope handle_scope;      
      status  = napi_open_handle_scope(env, &handle_scope );
      mwlog(MWLOG_DEBUG2,  (char*) " new handlescope status %d", status);
   
      status  = napi_create_object(env, &svcreqinfo );
      mwlog(MWLOG_DEBUG2,  (char*) " status %d", status);

      /* from MidWay.h
      typedef struct {
	 mwhandle_t  handle;
	 CLIENTID cltid;
	 GATEWAYID gwid;
	 SERVERID srvid;
	 SERVICEID svcid;
	 char * data;
	 size_t datalen;
	 int flags;
	 int appreturncode;
	 char service[MWMAXSVCNAME];

	 time_t deadline; 
	 int   udeadline; 

	 int authentication;
	 char username[MWMAXNAMELEN];
	 char clientname[MWMAXNAMELEN];
	 } mwsvcinfo; 
      */
      napi_value value;

      napi_property_descriptor desc[16];
      int pdcount = 0;
   
      status = napi_create_int64(env, msi->handle, &value);
      desc[pdcount++] = { "handle", 0, 0, 0, 0, value, napi_enumerable, 0 };

      status = napi_create_int64(env, msi->cltid, &value);
      desc[pdcount++] = { "clientid", 0, 0, 0, 0, value, napi_enumerable, 0 };
 
      status = napi_create_int64(env, msi->gwid, &value);
      desc[pdcount++] = { "gatewayid", 0, 0, 0, 0, value, napi_enumerable, 0 };
      
      status = napi_create_int64(env, msi->srvid, &value);
      desc[pdcount++] = { "serverid", 0, 0, 0, 0, value, napi_enumerable, 0 };
      
      status = napi_create_int64(env, msi->svcid, &value);
      desc[pdcount++] = { "serviceid", 0, 0, 0, 0, value, napi_enumerable, 0 };
      
      status = napi_create_int64(env, msi->deadline, &value);
      desc[pdcount++] = { "deadline", 0, 0, 0, 0, value, napi_enumerable, 0 };
     
      status = napi_create_int32(env, msi->udeadline, &value);
      desc[pdcount++] = { "udeadline", 0, 0, 0, 0, value, napi_enumerable, 0 };
     
      status = napi_create_string_utf8(env, msi->data, msi->datalen, &value);
      desc[pdcount++] = { "data", 0, 0, 0, 0, value, napi_enumerable, 0 };

      status = napi_create_string_utf8(env, msi->service, NAPI_AUTO_LENGTH, &value);
      desc[pdcount++] = { "service", 0, 0, 0, 0, value, napi_enumerable, 0 };

      status = napi_create_int32(env, msi->flags, &value);
      desc[pdcount++] = { "flags", 0, 0, 0, 0, value, napi_enumerable, 0 };

      status = napi_create_int32(env, msi->appreturncode, &value);
      if (status != napi_ok) mwlog(MWLOG_ERROR, (char*)
				   "Error in calling JS service callback %d", status); 
      desc[pdcount++] = { "appreturncode", 0, 0, 0, 0, value, napi_enumerable, 0 };


      // add reply to the object so it's available in this
      
      status = napi_create_function(env, "reply", NAPI_AUTO_LENGTH, Reply, NULL, &value);
      desc[pdcount++] = { "reply", 0, 0, 0, 0, value, napi_enumerable, 0 };
      
      status = napi_define_properties(env, svcreqinfo, pdcount, desc);
      mwlog(MWLOG_DEBUG2,  (char*) " status %d", status);
      if (status != napi_ok) mwlog(MWLOG_ERROR, (char*)
				   "Error in calling JS service callback %d", status); 


      // find callback
      napi_value callback ;
      napi_ref cbref = ref_servicemap.find(msi->service)->second;      
      status = napi_get_reference_value(env, cbref, &callback);
      
      { // DEBUGGING??
	 napi_value v;
	 napi_coerce_to_string(env,svcreqinfo , &v);
	 int blen = 1000;
	 char buf[blen];
	 JSValtoString(env, v, buf, blen);

	 mwlog(MWLOG_DEBUG2,  (char*) "found callback %s", buf);
      }

      // get global object
      napi_value global;
      status = napi_get_global(env, &global);

      // TODO: CHECK_STATUS;


      napi_value res;
      napi_value callargs[1];
      callargs[0] = svcreqinfo;

      /*
       * actual call to JS function
       */
      mwlog(MWLOG_DEBUG2,  (char*) "calling js callback");
      status = napi_call_function(env, svcreqinfo, callback, 1, callargs, &res);
      mwlog(MWLOG_DEBUG2,  (char*) "done js callback %d ", status );

      if (status == napi_pending_exception)  {
	 napi_value expt;
	 status = napi_get_and_clear_last_exception(env, &expt);

	 int blen = 1000;
	 char buf[blen];
	 JSValtoString(env, expt, buf, blen);
	 
	 mwlog(MWLOG_ERROR, (char *) "Error: %s", buf);
      } else if (status != napi_ok) {
	 mwlog(MWLOG_ERROR, (char*)
	       "Error in calling JS service callback %d", status);
      }

      {
	 int blen = 100;
	 char buf[blen];
	 JSValtoString(env, res, buf, blen);
	 mwlog(MWLOG_DEBUG2,  (char*) "callback returned %s", buf);
      }
      
      // in javascript we expect true or false
      status = napi_coerce_to_bool(env, res, &res);
      bool b;
      status = napi_get_value_bool(env, res, &b);

      int service_returncode = b ? MWSUCCESS : MWFAIL;
      
      mwlog(MWLOG_DEBUG2,  (char*) "Ending service call wrapper returning %d waking server thread",
	    service_returncode);

      status  = napi_close_handle_scope(env, handle_scope );
      mwlog(MWLOG_DEBUG2,  (char*) "close handlescope status %d", status);
   
      // uv_mutex_lock(&thread_wait_mutex);
      // uv_cond_signal(&thread_wait_cond);
      // uv_mutex_unlock(&thread_wait_mutex);

      return service_returncode;
   }


   /* 
    * this was an attempt to have a midway thread to do actuall mw* calls
    * 
   int servicecallwrapper (mwsvcinfo * msi) {
      int rc = uv_async_send(&service_request_event);
      
      mwlog(MWLOG_DEBUG2, (char*) "sending async message to trigger processing of service call in main thread, %d,  waiting", rc);
      uv_mutex_lock(&thread_wait_mutex);
      uv_cond_wait(&thread_wait_cond, &thread_wait_mutex);
      uv_mutex_unlock(&thread_wait_mutex);
      mwlog(MWLOG_DEBUG2, (char*) "woke up again the service request should now have been completed");
      return 0;
   }
 
   */

   // internal in MidWay.
   // We need it to block on MidWay's sysv message queue until there is something to get
   extern "C"  int _mw_my_mqid();

   void server_thread_main(void * data) {

      int msgqueueid = _mw_my_mqid();
      mwlog(MWLOG_DEBUG2, (char*) "in server thread starting mainloop - mqueue id %d ", msgqueueid );

      int rc;
      char dummybuffer[16000];
      
      while(1) {

	 rc = msgrcv(msgqueueid, dummybuffer, 1, 0, 0);

	 if (rc == -1 && errno == E2BIG) {
	    mwlog(MWLOG_DEBUG2, (char*) "there is a message in the queue");
	    mwlog(MWLOG_DEBUG2, (char*) "sending async message to trigger processing of MidWay messages");
	    int rc2 = uv_async_send(&service_request_event);
	    mwlog(MWLOG_DEBUG2, (char*) "sending async message returned %d", rc2);
	    //	    sleep(1);	    
	 } else {
	    mwlog(MWLOG_ERROR, (char*) " msg rcv returned %d errno = %d, %s", rc, errno, strerror(errno));
	    break;
	 };
	 
      };
       
      mwlog(MWLOG_DEBUG2, (char*) "in server thread mainloop - returned with %d", rc);
   }

   /** 
    * Instead on main loop. We are blocking wait on the message queue above, then we end up here
    * back on node.js main thread via uv_async_send()
    * 
    * we now drain the message queue one pass at the time and reisse the async mmessage allowing node.js
    * mainloop to do other stuff in between
    */
   void service_request_callback(uv_async_t* handle) {
      mwlog(MWLOG_DEBUG2, (char*) "service_request_callback starting...");

      envInMainLoop = (napi_env) handle->data;
        
      int rc ;
      int more_todo = 0;
      
      rc = mwservicerequest(MWNOBLOCK);
      mwlog(MWLOG_DEBUG2, (char*) "mwservicerequest returned, %d", rc);     
      if (rc == 0) more_todo = 1;

      //handling events 
      mwrecvevents();

      // fetching call replies and call handlers
      rc = processACallReplies(envInMainLoop);
      if (rc == 1) more_todo = 1;

      if (more_todo) {
	 mwlog(MWLOG_DEBUG2, (char*) "we have more to do, sending another async event to");
	 int rc2 = uv_async_send(&service_request_event);
	 mwlog(MWLOG_DEBUG2, (char*) "sending async message returned %d", rc2);

      }   
      mwlog(MWLOG_DEBUG2, (char*) "ending service_request_callback ...");      
   };
      
   /*************************************
    *
    *  Actuall wrapper funcs 
    *
    *************************************/


   /**
    * PROVIDE
    */
   napi_value Provide(napi_env env, napi_callback_info info) {
      mwlog(MWLOG_DEBUG2, (char*) "Beginning Provide");

      napi_status status;
      napi_value rv;
 
      napi_value undefinedval;
      status = napi_get_undefined( env, &undefinedval);

      napi_value args[10];
      size_t argc = 10;
      status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
      CHECK_STATUS;

      napi_value global;
      status = napi_get_global(env, &global);
      CHECK_STATUS;

      if (argc != 2) {
	 napi_throw_type_error(env, NULL, "provide  got wrong number of arguments. Should be two" );
	 return undefinedval;	  
      }

      size_t servicenamelen = MWMAXSVCNAME;
      char * servicename = (char *) malloc (sizeof(char) * MWMAXSVCNAME);
      napi_value callback_function;
      
      napi_valuetype type;

      // first arg must be a string with service name
      napi_typeof(env, args[0], &type);
      if (type == napi_string) {
	 size_t rlen;
	 status = napi_get_value_string_utf8(env, args[0], servicename, servicenamelen, &rlen);

      } else {
	 napi_throw_type_error(env, NULL, "provide got illegal type on first argument no a string." );
	 status = napi_create_int32(env, 0, &rv);
	 assert(status == napi_ok);
	 return rv;
      }

      // second arg MUST be a callback function
      napi_typeof(env, args[1], &type);
      if (type == napi_function) {
	 callback_function  = args[1];
      } else {
	 napi_throw_type_error(env, NULL, "provide got illegal type on second argument, not a function." );
	 status = napi_create_int32(env, 0, &rv);
	 assert(status == napi_ok);
	 return rv;
      }

      napi_ref ref;
      status = napi_create_reference(env, callback_function, 1, &ref);
      
	 
      {// DEBUGGING
	 napi_value v;
	 napi_coerce_to_string(env, callback_function, &v);
	 int blen = 100;
	 char buf[blen];
	 JSValtoString(env, v, buf, blen);

	 mwlog(MWLOG_DEBUG2,  (char*) "got callback %s", buf);
      }
  
      int rc = mwprovide(servicename, servicecallwrapper, 0);
      mwlog(MWLOG_DEBUG2, (char*) "mwprovide returned 0x%x", rc);

      if ( ref_servicemap.empty() ) {
	 int rc = uv_thread_create(&mw_server_thread, server_thread_main, NULL);
	 mwlog(MWLOG_DEBUG2, (char*) "server thread start %d", rc);
      }
      
      // TODO check RC before inserting into callback  map
      // not that it really matter. if mwprovide fails, we're never going to get called.
      ref_servicemap.insert(std::pair<std::string, napi_ref>(servicename, ref));
      

      // returning
      status = napi_create_int32(env, rc, &rv);
      assert(status == napi_ok);

      mwlog(MWLOG_DEBUG2, (char*) "Ending Provide");
      return rv;
   };

   /**
    * UNPROVIDE
    */
   napi_value UnProvide(napi_env env, napi_callback_info info) {
      mwlog(MWLOG_DEBUG2, (char*) "Beginning UnProvide");
      
      napi_status status;
      napi_value rv;

      napi_value undefinedval;
      status = napi_get_undefined( env, &undefinedval);

      napi_value args[10];
      size_t argc = 10;
      status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
      CHECK_STATUS;

      napi_value global;
      status = napi_get_global(env, &global);
      CHECK_STATUS;

      if (argc != 1) {
	 napi_throw_type_error(env, NULL, "provide  got wrong number of arguments. Should be one" );
	 return undefinedval;	  
      }

      size_t servicenamelen = MWMAXSVCNAME;
      char * servicename = (char *) malloc (sizeof(char) * MWMAXSVCNAME);
      
      napi_valuetype type;

      // first arg must be a string with service name
      napi_typeof(env, args[0], &type);
      if (type == napi_string) {
	 size_t rlen;
	 status = napi_get_value_string_utf8(env, args[0], servicename, servicenamelen, &rlen);

      } else {
	 napi_throw_type_error(env, NULL, "provide got illegal type on first argument no a string." );
	 status = napi_create_int32(env, 0, &rv);
	 assert(status == napi_ok);
	 return rv;
      }

      // calling Midway
      int rc = mwunprovide(servicename);
      mwlog(MWLOG_DEBUG2, (char*) "mwprovide returned 0x%x", rc);

      // now we derefernce the callback function and remove it from the map
      std::string sname = servicename;      
      napi_ref ref = ref_servicemap.find(servicename)->second;
      uint32_t refcount  = -1;
      status = napi_reference_unref(env, ref, &refcount);
      mwlog(MWLOG_DEBUG2,  (char*) "refcount on callback is now %d", refcount);

      ref_servicemap.erase(sname);
      
      if ( ref_servicemap.empty() ) {
	 mwlog(MWLOG_DEBUG2, (char*)"last service unprovided, joining \n");
      	 int rc = uv_thread_join(&mw_server_thread);
	 mwlog(MWLOG_DEBUG2, (char*)"server thread joined %d", rc);
      }

      // returning
      status = napi_create_int32(env, rc, &rv);
      assert(status == napi_ok);

      mwlog(MWLOG_DEBUG2, (char*) "Ending UnProvide");
      return rv;
   };
   
   /**
    * REPLY
    */
   napi_value Reply(napi_env env, napi_callback_info info) {
      mwlog(MWLOG_DEBUG2, (char*) "Beginning Reply");
      mwlog(MWLOG_DEBUG2,  (char*) "env %p", env);

      char * data = NULL;
      size_t datalen = 0;
      int successflag = MWFAIL;
      int appreturncode = 0;
      
      napi_status status;
      napi_value rv;

      napi_value args[10];
      size_t argc = 10;
      status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
      CHECK_STATUS;
 
      mwlog(MWLOG_DEBUG2, (char*) "argc %ld", argc);

      if (argc < 2) {
	 napi_throw_type_error(env, NULL, "missing arguments data and successflag must be present." );
	 return NULL;
      }
      
      napi_valuetype type;

      // arg 0 is data
      napi_typeof(env, args[0], &type);

      if (type == napi_string) {
	 size_t rlen;
	 status = napi_get_value_string_utf8(env, args[0], NULL, 0,  &rlen);
	 if (status != napi_ok) {
	    mwlog(MWLOG_ERROR, (char *) "status = %d", status);
	    napi_throw_type_error(env, NULL, "unable to get data length." );
	 }
	 mwlog(MWLOG_DEBUG2, (char *) "datalen = %ld", rlen);
	 rlen++;
	 data = (char *) mwalloc(rlen);

	 status = napi_get_value_string_utf8(env, args[0], data, rlen,  &datalen);
	 CHECK_STATUS;
	 
      } else if (type == napi_undefined) {
	 // no return data
      } else {
	 napi_throw_type_error(env, NULL, "data must be a string or undefined." );
	 return NULL;
      }

      // arg 1 is successflag
      napi_typeof(env, args[1], &type);
      if (type == napi_number) {
	 status = napi_get_value_int32( env,args[1],  &successflag);
      } else {
	 napi_throw_type_error(env, NULL, "successflag must be an int." );
	 return NULL;
      }


      // optional arg 2 is appreturncode 
      if (argc >= 3) {
	 napi_typeof(env, args[2], &type);
	 if (type == napi_number) {
	    status = napi_get_value_int32( env,args[2],  &appreturncode);
	 } else {
	    napi_throw_type_error(env, NULL, "appreturncode must be an int." );
	    return NULL;
	 }
      }

      // calling midway
      mwlog(MWLOG_DEBUG2, (char *) "calling mwreply with datalen = %ld", datalen);      
      int rc =  mwreply(data, datalen, successflag, appreturncode, 0);
      mwlog(MWLOG_DEBUG2, (char *) " mwreply returnrd = %d", rc);      
      if (rc < 0) mwfree(data);
      
      status = napi_create_int32(env, rc, &rv);
      assert(status == napi_ok);

      mwlog(MWLOG_DEBUG2, (char*) "Ending Reply %d %d", rc, errno);
      return rv;
   };
   
   /**
    * RETURN
    */
   napi_value Return(napi_env env, napi_callback_info info) {
      mwlog(MWLOG_DEBUG2, (char*) "Beginning Return");
      
      napi_status status;
      napi_value rv;

      int rc = 99;

      status = napi_create_int32(env, rc, &rv);
      assert(status == napi_ok);

      mwlog(MWLOG_DEBUG2, (char*) "Ending Return");
      return rv;
   };
   
   /**
    * FORWARD
    */
   napi_value Forward(napi_env env, napi_callback_info info) {
      mwlog(MWLOG_DEBUG2, (char*) "Beginning Forward");
      
      char * data = NULL;
      size_t datalen = 0;

      char nextsvc[MWMAXSVCNAME] = {0};
      
      napi_status status ;
      napi_value rv = {0};

      napi_value args[10];
      size_t argc = 10;
      status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
      CHECK_STATUS;

      if (argc < 2) {
	 napi_throw_type_error(env, NULL, "missing arguments forwaring service and data  must be present." );
	 return NULL;
      }
      
      napi_valuetype type;
      napi_typeof(env, args[0], &type);

      if (type == napi_string) {
	 size_t rlen;	 
	 status = napi_get_value_string_utf8(env, args[0], nextsvc, MWMAXSVCNAME, &rlen);
	 CHECK_STATUS;
      
      } else {
	 napi_throw_type_error(env, NULL, "servicename  must be a string." );
	 return NULL;
      }
      
      napi_typeof(env, args[1], &type);
      if (type == napi_string) {
	 size_t rlen;
	 status = napi_get_value_string_utf8(env, args[1], NULL, 0,  &rlen);
	 if (status != napi_ok) {
	    mwlog(MWLOG_ERROR, (char *) "status = %d", status);
	    napi_throw_type_error(env, NULL, "unable to get data length." );
	 }

	 mwlog(MWLOG_DEBUG2, (char *) "datalen = %ld", rlen);
	 rlen++;
	 data = (char *) mwalloc(rlen);
	 status = napi_get_value_string_utf8(env, args[1], data, rlen,  &datalen);
	 CHECK_STATUS;
	 
      } else if (type == napi_undefined) {
	 // no return data
      } else {
	 napi_throw_type_error(env, NULL, "data must be a string or undefined." );
	 return NULL;
      }
           
      
      int rc =  mwforward(nextsvc, data, datalen, 0);

      if (rc < 0) mwfree(data);
      
      status = napi_create_int32(env, rc, &rv);
      assert(status == napi_ok);

      mwlog(MWLOG_DEBUG2, (char*) "Ending Forward");
      return rv;

   };


   void initServer(napi_env env) {
      pthread_t me = pthread_self();
      mwlog(MWLOG_DEBUG2, (char*) "init server thread  thread id %lu ", me );
      mwlog(MWLOG_DEBUG2,  (char*) " env %p", env);
      napi_status status;
      
      status = napi_get_uv_event_loop(env, &loop);
      if (status != napi_ok) {
	 napi_throw_error(env, NULL, "Unable to get event loop");
      }


      uv_cond_init(&thread_wait_cond);
      uv_mutex_init(&thread_wait_mutex);
  
      int rc = uv_async_init(loop, &service_request_event, service_request_callback);
      mwlog(MWLOG_DEBUG2, (char*)"async srvreq handle init %d", rc);
      envInMainLoop = env;
      service_request_event.data = env;

      return;

   }

   static void server_close_cb(uv_handle_t* handle) {
      printf("close cb  \n");
      
   }

   void finalizeServer(napi_env env) {
      uv_close( (uv_handle_t *) &service_request_event , server_close_cb);
   };
   
}  // namespace MidWay
