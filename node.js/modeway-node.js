const midway = require('./build/Release/addon');



midway.attach([url]);

midway.provide("svcname", function(svcinfo) {
    // svcinfo is an object with
    // svcinfo.handle
    // svcinfo.clientid
    // svcinfo.serverid
    // svcinfo.service
    // svcinfo.data
    // svcinfo.flags
    // svcinfo.deadline // realtime in millis
    //

    midway.reply(data[, midway.success, apprc=0, flags=0]);
    midway.forward"srcname", data, flags=0);
	       
}

midway,unprovide("svcname");
midway.runServer();


// client


midway.call("svcname", data, flags=0, function(data, apprc, rc));


sb_id = midway.subscribe("eventglob", flags= 0, function("eventname", data))
midway.unsubscribe(sb_id)
midway.unsubscribe("eventglob")



midway.mwrecvevents();
midway.startrecvthread();
