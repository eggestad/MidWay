#include <iostream>
#include <string>
#include <map>

#include <node_api.h>
#include <assert.h>

#include <stdio.h>

#include <MidWay.h>

#define CHECK_STATUS					\
   if (status != napi_ok) {				\
      napi_throw_error(env, "1", "N-API call failed");	\
      return 0;				\
   }


namespace MidWay {

   /*
    * hash to store mapping between service name and a refernce to the call back function
    */ 
   std::map<std::string, napi_ref> ref_servicemap;


   /* 
    * just a helper function to make mwlog messages with javascript data type 
    */
   static int JSValtoString(napi_env env, napi_value val, char * buf, size_t len) {

      napi_value v;
      napi_coerce_to_string(env, val, &v);
      val = v;

	 
      napi_valuetype type;
      napi_typeof(env, val, &type);
      napi_status sts;

      switch(type)  {

      case napi_undefined:
	 return snprintf(buf, len, "(undefined)");

      case napi_null:
	 return snprintf(buf, len, "(null)");
	    
      case napi_boolean:
	 bool b;
	 napi_get_value_bool(env, val, &b);
	 return snprintf(buf, len, val ? "true":"false");
	    
      case napi_number:
	 int64_t i;
	 sts =  napi_get_value_int64(env, val, &i);
	 if (sts == napi_ok)
	    return snprintf(buf, len, "%ld", i);
	 double d;
	 sts =  napi_get_value_double(env, val, &d);
	 if (sts == napi_ok)
	    return snprintf(buf, len, "%f", d);
	 return snprintf(buf, len, "(some number)");
	    
      case napi_string:
	 size_t rc;
	 napi_get_value_string_utf8(env, val, buf, len, &rc);
	 return rc;
	    
      case napi_object:
	 return snprintf(buf, len, "[object]");
	   	    
      case napi_function:
	 return snprintf(buf, len, "(function)");

      case napi_external:
	 return snprintf(buf, len, "(external");

	 // case napi_bigint:
	 //    return snprintf(buf, len, "(bigint)");

      default:
	 return snprintf(buf, len, "{unknown javascript type}");
      }
   }

   
   napi_value undefinedval;

   /* helper function meant to deal with object keys
    */ 
   napi_value tmpStrVal;
   napi_value * valFromString(napi_env env, const char * str, napi_value *val) {
      if (val == NULL) val = &tmpStrVal;
      napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, val);
      return val;
   }

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
      
      status  = napi_create_object(env, &svcreqinfo );

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
      mwlog(MWLOG_DEBUG2,  (char*) " status %d", status);
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

      status = napi_define_properties(env, svcreqinfo, pdcount, desc);
      mwlog(MWLOG_DEBUG2,  (char*) " status %d", status);
      if (status != napi_ok) mwlog(MWLOG_ERROR, (char*)
				   "Error in calling JS service callback %d", status); 

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
           
      napi_value global;
      status = napi_get_global(env, &global);
      CHECK_STATUS;


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

      int rc = b ? MWSUCCESS : MWFAIL;
      
      mwlog(MWLOG_DEBUG2,  (char*) "Ending service call wrapper returning %d",  rc);
      return rc;
   }

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

      status = napi_create_int32(env, rc, &rv);
      assert(status == napi_ok);

      mwlog(MWLOG_DEBUG2, (char*) "Ending Detach");
      return rv;
   };
   
   /**
    * PROVIDE
    */
   napi_value Provide(napi_env env, napi_callback_info info) {
      mwlog(MWLOG_DEBUG2, (char*) "Beginning Provide");
      
      napi_status status;
      napi_value rv;
 
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

      // returning
      status = napi_create_int32(env, rc, &rv);
      assert(status == napi_ok);

      mwlog(MWLOG_DEBUG2, (char*) "Ending UnProvide");
      return rv;
   };

   /**
    * RUN SERVER (main loop)
    */
   napi_value runServer(napi_env env, napi_callback_info info) {
      mwlog(MWLOG_DEBUG2, (char*) "Beginning runServer");

      napi_status status;
      napi_value rv;

      envInMainLoop = env;

      int rc = mwMainLoop(0);

      envInMainLoop = NULL;
      status = napi_create_int32(env, rc, &rv);
      assert(status == napi_ok);
      
   
      mwlog(MWLOG_DEBUG2, (char*) "Ending runServer rc %d", rc);
      return rv;
   };
   
   /**
    * REPLY
    */
   napi_value Reply(napi_env env, napi_callback_info info) {
      mwlog(MWLOG_DEBUG2, (char*) "Beginning Reply");

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
      int rc =  mwreply(data, datalen, successflag, appreturncode, 0);

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

      status = napi_get_undefined( env, &undefinedval);

      // API
      desc[pdcount++] = DECLARE_NAPI_METHOD("log", log_to_midway);
      desc[pdcount++] = DECLARE_NAPI_METHOD("attach", Attach);
      desc[pdcount++] = DECLARE_NAPI_METHOD("detach", Detach);
      desc[pdcount++] = DECLARE_NAPI_METHOD("provide", Provide);
      desc[pdcount++] = DECLARE_NAPI_METHOD("unprovide", UnProvide);
      desc[pdcount++] = DECLARE_NAPI_METHOD("runServer", runServer);
      desc[pdcount++] = DECLARE_NAPI_METHOD("reply", Reply);
      desc[pdcount++] = DECLARE_NAPI_METHOD("return", Return);
      desc[pdcount++] = DECLARE_NAPI_METHOD("forward", Forward);

      // Add constants
      napi_value value;
      status = napi_create_int32(env, MWSUCCESS, &value);
      desc[pdcount++]  ={ "success", 0, 0, 0, 0, value, napi_enumerable, 0 };
      status = napi_create_int32(env, MWFAIL, &value);
      desc[pdcount++]  ={ "fail", 0, 0, 0, 0, value, napi_enumerable, 0 };
      status = napi_create_int32(env, MWMORE, &value);
      desc[pdcount++]  ={ "more", 0, 0, 0, 0, value, napi_enumerable, 0 };

      status = napi_define_properties(env, exports, pdcount, desc);
      CHECK_STATUS;
      
      mwopenlog((char*) "nodejs", (char*) "./testlog", MWLOG_DEBUG2);

      return exports;
   }

   NAPI_MODULE(NAPI_GYP_MODULE_NAME, init)

}  // namespace MidWay
