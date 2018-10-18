#include <iostream>
#include <string>
#include <map>

#include <node_api.h>
#include <uv.h>

#include <assert.h>

#include <stdio.h>

#include <MidWay.h>
#include "mw_wrapper.h"

namespace MidWay {


   
   uv_loop_t * eventloop;
   //uv_thread_t mw_server_thread;
   uv_thread_t mw_client_thread; // TODO


   /*************************************
    *
    *  Actuall wrapper funcs 
    *
    *************************************/


   /**
    * mwlog
    */
   napi_value log_to_midway(napi_env env, napi_callback_info info) {
      napi_status status;

      size_t argc = 100;
      napi_value args[argc];
      size_t len = 16000;
      char buf[len];
      int offset = 0;
      
      status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
      CHECK_STATUS;
      
      for (uint32_t i = 0; i < argc; i++) {
	 //printf("-- %d\n", offset);
	 offset += JSValtoString(env, args[i] , buf+offset, len -offset);
	 if (i == argc-1) {
	    buf[offset++] = '\0';

	 } else {
	    buf[offset++] = ' ';
	 }
      }
      mwlog(MWLOG_INFO, (char *) "%s", buf);
      
      napi_value undefinedval;
      status = napi_get_undefined( env, &undefinedval);
      return undefinedval;
   }


   /**
    * Attach.
    * 
    * Arguments, we accept both 1&2 two strings with url and name which may be undefined
    * Or an object with possible propertties : TODO
    */ 
   napi_value Attach(napi_env env, napi_callback_info info) {

      mwlog(MWLOG_DEBUG2,  (char*) "Beginning Attach");

      char * url = NULL;
      char * name = NULL;
      int flags = MWSERVER;

      
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
      size_t namelen = 1000;
      char namebuf[urllen];
      
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
      
      if (argc == 2) {

	 napi_valuetype type;

	 napi_typeof(env, args[0], &type);

	 if (type == napi_string) {
	    size_t rlen;
	    status = napi_get_value_string_utf8(env, args[0], urlbuf, urllen, &rlen);
	    url = urlbuf;
	 } else if (type == napi_undefined) {
	    
	 } else if (type == napi_null) {
	       
	 } else {
	    napi_throw_type_error(env, NULL, "attach got illegal type on first argument." );
	    status = napi_create_int32(env, 0, &rv);
	    assert(status == napi_ok);
	    return rv;
	 }

	 napi_typeof(env, args[1], &type);

	 if (type == napi_string) {
	    size_t rlen;
	    status = napi_get_value_string_utf8(env, args[1], namebuf, namelen, &rlen);
	    name = namebuf;
	 } else if (type == napi_undefined) {
	    
	 } else if (type == napi_null) {
	       
	 } else {
	    napi_throw_type_error(env, NULL, "attach got illegal type on second argument." );
	    status = napi_create_int32(env, 0, &rv);
	    assert(status == napi_ok);
	    return rv;
	 }
      }
      mwlog(MWLOG_DEBUG2, (char*) "calling mwattach with %s %s %d", url, name, flags);

      int rc = mwattach(url, name, flags);

      if (rc == 0) {
	 initClient(env);
	 if (flags | MWSERVER) initServer(env);
      }
	 
      status = napi_create_int32(env, rc, &rv);
      assert(status == napi_ok);

      mwlog(MWLOG_DEBUG2, (char*) "Ending Attach");
      return rv;
   };

   /**
    * DETACH
    */
   napi_value Detach(napi_env env, napi_callback_info info) {
      mwlog(MWLOG_DEBUG2, (char*) "Beginning Detach");
      
      napi_status status;
      napi_value rv;

      int rc = mwdetach();

      void finalizeServer(napi_env env);
      void finalizeClient(napi_env env);
      

      status = napi_create_int32(env, rc, &rv);
      assert(status == napi_ok);

      mwlog(MWLOG_DEBUG2, (char*) "Ending Detach");
      return rv;
   };
   

   /******************************************************
    * 
    * INIT Node JS addon
    * 
    ******************************************************/

#define DECLARE_NAPI_METHOD(name, func)		\
   { name, 0, func, 0, 0, 0, napi_default, 0 }


   napi_value init(napi_env env, napi_value exports) {

      napi_property_descriptor desc[30];
      int pdcount = 0;

      napi_status status;

      // API
      desc[pdcount++] = DECLARE_NAPI_METHOD("log", log_to_midway);
      desc[pdcount++] = DECLARE_NAPI_METHOD("attach", Attach);
      desc[pdcount++] = DECLARE_NAPI_METHOD("detach", Detach);

      // CLIENNT API
      desc[pdcount++] = DECLARE_NAPI_METHOD("call", ACall);
      //desc[pdcount++] = DECLARE_NAPI_METHOD("call", Call);
      //desc[pdcount++] = DECLARE_NAPI_METHOD("fetch", Fetch);

      // SERVER API
      desc[pdcount++] = DECLARE_NAPI_METHOD("provide", Provide);
      desc[pdcount++] = DECLARE_NAPI_METHOD("unprovide", UnProvide);
      desc[pdcount++] = DECLARE_NAPI_METHOD("runServer", runServer);
      desc[pdcount++] = DECLARE_NAPI_METHOD("reply", Reply);
      //desc[pdcount++] = DECLARE_NAPI_METHOD("return", Return);
      desc[pdcount++] = DECLARE_NAPI_METHOD("forward", Forward);

      // Add constants
      napi_value value;
      status = napi_create_int32(env, MWSUCCESS, &value);
      desc[pdcount++]  ={ "success", 0, 0, 0, 0, value, napi_enumerable, 0 };
      status = napi_create_int32(env, MWFAIL, &value);
      desc[pdcount++]  ={ "fail", 0, 0, 0, 0, value, napi_enumerable, 0 };
      status = napi_create_int32(env, MWMORE, &value);
      desc[pdcount++]  ={ "more", 0, 0, 0, 0, value, napi_enumerable, 0 };

      status = napi_create_int32(env, MWNOREPLY, &value);
      desc[pdcount++]  ={ "noreply", 0, 0, 0, 0, value, napi_enumerable, 0 };

      status = napi_create_int32(env, MWMULTIPLE, &value);
      desc[pdcount++]  ={ "multiple", 0, 0, 0, 0, value, napi_enumerable, 0 };

      status = napi_create_int32(env, MWNOBLOCK, &value);
      desc[pdcount++]  ={ "noblock", 0, 0, 0, 0, value, napi_enumerable, 0 };


      status = napi_define_properties(env, exports, pdcount, desc);
      CHECK_STATUS;
      
      mwopenlog((char*) "nodejs", (char*) "./testlog", MWLOG_DEBUG2);

      return exports;
   }

   NAPI_MODULE(NAPI_GYP_MODULE_NAME, init)

}  // namespace MidWay
