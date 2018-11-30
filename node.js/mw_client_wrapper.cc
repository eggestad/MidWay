#include <iostream>
#include <string>
#include <map>

#include <node_api.h>
#include <uv.h>

#include <assert.h>

#include <stdio.h>
#include <string.h>

#include <MidWay.h>
#include "mw_wrapper.h"


namespace MidWay {

   /*
    * hash to store mapping between call handle and a refernce to the call back function
    */ 
   std::map<int, napi_ref> ref_call_map;


   int processACallReplies(napi_env env) {

      napi_status status;

      char * data;
      size_t datalen = 0;
      int handle = 0;
      int apprc = 0;

      mwlog(MWLOG_DEBUG2, (char*) "mwfetch...");

      int rc = mwfetch(&handle, &data, &datalen, &apprc, MWNOBLOCK);

      mwlog(MWLOG_DEBUG2, (char*) "mwfetch returned, %d handle %d %d errno %d %s",
	    rc, handle, apprc, errno, strerror(errno));     

      if (rc != -EFAULT && rc < 0) {
	 return 0;
      }

      
      napi_handle_scope handle_scope;      
      status  = napi_open_handle_scope(env, &handle_scope );
      mwlog(MWLOG_DEBUG2,  (char*) " new handlescope status %d", status);

      mwlog(MWLOG_DEBUG2,  (char*) "callmap size %p", ref_call_map.size());

      // find callback
      napi_value callback ;
      mwlog(MWLOG_DEBUG2,  (char*) "looking callback ref %p", ref_call_map.find(handle)->second);

      napi_ref cbref = ref_call_map.find(handle)->second;
      mwlog(MWLOG_DEBUG2,  (char*) "found callback ref");
	    
      status = napi_get_reference_value(env, cbref, &callback);
      mwlog(MWLOG_DEBUG2,  (char*) "find callback status %d", status);      
      { // DEBUGGING??
	 napi_value v;
	 status = napi_coerce_to_string(env, callback , &v);
	 int blen = 1000;
	 char buf[blen] = {0};
	 JSValtoString(env, v, buf, blen);

	 mwlog(MWLOG_DEBUG2,  (char*) "found callback %s", buf);
      }

      
      // get global object
      napi_value global;
      status = napi_get_global(env, &global);

      // parameters to callback      
      napi_value argv[3];
      status = napi_create_int32(env, rc, &argv[0]);
      status = napi_create_string_utf8(env, data, datalen, &argv[1]);
      status = napi_create_int32(env, apprc, &argv[2]);


      /*
       * actual call to JS function
       */
      napi_value res;

      mwlog(MWLOG_DEBUG2,  (char*) "calling js callback");

      status = napi_call_function(env, global, callback, 3, argv, &res);

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
      
      status  = napi_close_handle_scope(env, handle_scope );
      mwlog(MWLOG_DEBUG2,  (char*) "close handlescope status %d", status);

      if (rc == -EFAULT) return 1;
      if (rc == 0) return 1;
      return 0;

   }

   /*************************************
    *
    *  Actuall wrapper funcs 
    *
    *************************************/


   /**
    * AsyncCall
    * 
    */ 
   napi_value ACall(napi_env env, napi_callback_info info) {

      mwlog(MWLOG_DEBUG2,  (char*) "Beginning Acall");

      char * url = NULL;

      char * data = NULL;
      size_t datalen = 0;
      int flags = 0;
      
      napi_status status;
      napi_value rv;

      napi_value args[10];
      size_t argc = 10;
      status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
      CHECK_STATUS;

      napi_value global;
      status = napi_get_global(env, &global);
      CHECK_STATUS;

      size_t urllen = MWMAXSVCNAME;
      char urlbuf[urllen];
      

      if (argc < 3) {
	 napi_throw_type_error(env, NULL, "midway.acall must have three arg. servicename, data, callbackfunction." );
	 return NULL;
      }

 
	 
      napi_valuetype type;
      napi_typeof(env, args[0], &type);
      if (type == napi_string) {
	 size_t rlen;
	 status = napi_get_value_string_utf8(env, args[0], urlbuf, urllen, &rlen);
	 CHECK_STATUS;
	 url = urlbuf;
      } else {
	 napi_throw_type_error(env, NULL, "acall got illegal type on first argument." );
	 status = napi_create_int32(env, 0, &rv);
	 assert(status == napi_ok);
	 return rv;
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
	 // no  data
      } else {
	 napi_throw_type_error(env, NULL, "data must be a string or undefined." );
	 return NULL;
      }

      napi_value callback_function;
       
      napi_typeof(env, args[2], &type);
      if (type == napi_function) {
	 callback_function  = args[2];
      } else {
	 napi_throw_type_error(env, NULL, "acall must have a function as third argument." );
	 status = napi_create_int32(env, 0, &rv);
	 assert(status == napi_ok);
	 return rv;
      }

      
	 
      mwlog(MWLOG_DEBUG2, (char*) "calling mwacall with %s %s %ld %d", url, data, datalen, flags);

      int rc = mwacall(url, data, datalen, flags);
      mwlog(MWLOG_DEBUG2, (char*) "mwacall returned %d", rc);

      if (rc > 0) {
	 napi_ref cbref;
	 status = napi_create_reference(env, callback_function, 1, &cbref);
      	 ref_call_map.insert(std::pair<int, napi_ref>(rc, cbref));
	 rc = 1;
      } else {
	 char msg[100];
	 snprintf(msg, 100, "acall failed with %d %s", -rc , strerror(-rc));
	 napi_throw_error(env, NULL,  msg);
	 rc = 0;
      }
      
      status = napi_create_int32(env, rc, &rv);
      assert(status == napi_ok);

      mwlog(MWLOG_DEBUG2, (char*) "Ending acall returning %d", rc);
      return rv;

   }
   /**
    * Fetch
    * 
    */ 
   napi_value Fetch(napi_env env, napi_callback_info info) {

      mwlog(MWLOG_DEBUG2,  (char*) "Beginning Fetch");

      char * url = NULL;
      char * name = NULL;
      int flags = MWSERVER;
      int handle = 0;
      
      
      napi_status status;
      napi_value rv;

      napi_value args[10];
      size_t argc = 10;
      status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
      CHECK_STATUS;

      napi_value global;
      status = napi_get_global(env, &global);
      CHECK_STATUS;

      size_t urllen = 1000;
      char urlbuf[urllen];
      
      if (argc == 1) {
	 
	 napi_valuetype type;
	 napi_typeof(env, args[0], &type);
	 if (type == napi_string) {
	    size_t rlen;
	    status = napi_get_value_string_utf8(env, args[0], urlbuf, urllen, &rlen);
	    CHECK_STATUS;
	    url = urlbuf;
	 } else if (type == napi_object) {
	    bool hasprop;
	    napi_value obj = args[0], key, val;

	    valFromString(env, "url", &key);
	    status = napi_has_property(env, obj, key, &hasprop);	   
	    CHECK_STATUS;
	    if (hasprop) {
	       size_t rlen;
	       status = napi_get_property(env, obj, key, &val);
	       CHECK_STATUS;
	       status = napi_get_value_string_utf8(env, val, urlbuf, urllen, &rlen);
	       CHECK_STATUS;
	       url = urlbuf;
	    };
	    
	    valFromString(env, "name", &key);
	    status = napi_has_property(env, obj, key, &hasprop);	   
	    CHECK_STATUS;
	    if (hasprop) {
	       size_t rlen;
	       status = napi_get_property(env, obj, key, &val);
	       CHECK_STATUS;
	       status = napi_get_value_string_utf8(env, val, urlbuf, urllen, &rlen);
	       CHECK_STATUS;
	       url = urlbuf;
	    }
	    
	 
	 } else if (type == napi_undefined) {
	    
	 } else if (type == napi_null) {
	       
	 } else {
	    napi_throw_type_error(env, NULL, "attach got illegal type on first argument." );
	    status = napi_create_int32(env, 0, &rv);
	    assert(status == napi_ok);
	    return rv;
	 }
      }
      mwlog(MWLOG_DEBUG2, (char*) "calling mwattach with %s %s %d", url, name, flags);

      int rc = mwattach(url, name, flags);
      
      status = napi_create_int32(env, rc, &rv);
      assert(status == napi_ok);

      mwlog(MWLOG_DEBUG2, (char*) "Ending Attach");
      return rv;
   };



   void initClient(napi_env env) {
   };
 

   void finalizeClient(napi_env env) {
   };
   
}  // namespace MidWay
