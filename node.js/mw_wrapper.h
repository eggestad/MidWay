#include <MidWay.h>

#define CHECK_STATUS					\
   if (status != napi_ok) {				\
      napi_throw_error(env, "1", "N-API call failed");	\
      return 0;				\
   }


namespace MidWay {
   napi_value * valFromString(napi_env env, const char * str, napi_value *val) ;
   int JSValtoString(napi_env env, napi_value val, char * buf, size_t len) ;

   napi_value Attach(napi_env env, napi_callback_info info) ;
   
   int servicecallwrapper (mwsvcinfo * msi) ;
   
   napi_value Provide(napi_env env, napi_callback_info info) ;
   napi_value UnProvide(napi_env env, napi_callback_info info) ;
   napi_value runServer(napi_env env, napi_callback_info info) ;

   napi_value Reply(napi_env env, napi_callback_info info) ;
   napi_value Return(napi_env env, napi_callback_info info) ;
   napi_value Forward(napi_env env, napi_callback_info info) ;

   napi_value ACall(napi_env env, napi_callback_info info) ;
   //napi_value Call(napi_env env, napi_callback_info info) ;
   napi_value Fetch(napi_env env, napi_callback_info info) ;


}
