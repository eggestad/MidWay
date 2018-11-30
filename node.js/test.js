// hello.js
const mw = require('./build/Release/midway');

console.log(mw);

console.log(mw.log("test of log", "arg1", 66, 88.4, "arg4", new Object(), [1,2], function()   { return 3;}  ));


var attach_data = {
    url: "ipc://test",
    name: "nodejs_test",
    type: "server", // "server", "serveronly",
    authtype: "",
    username: "user_name",
    password: "psswd",
    credentials: "so kind of credentials for auth plugin"
}

//console.log(mw.attach());
//console.log(mw.attach(data));
//console.log(mw.attach(function() { console.info(" CB test"); }, "nodejs_test"));
console.log(mw.attach( undefined, "nodejs_test"));


function callback0() {
    console.log("cb0 start");
    mw.reply("testreply from data " + this.data, mw.success, 67);

//    mw.call("cal", "data", function(a, b, c) { console.log("call returned", a, b, c); });

    console.log("cb0 ok");
    return true;
}
function callback1(svcinfo) {
    console.log("got call with ", svcinfo);
    console.log(mw.reply("testreply", mw.success, 67));
    console.log(mw.reply("testreply", mw.success, 67));
}

function callback2(svcinfo) {
    console.log("got call with ", svcinfo);
    console.log(mw.forward("node0", "testforward"));
}

console.log(mw.provide("node0", callback0));
console.log(mw.provide("node1", callback1));
console.log(mw.provide("node2", callback2));

console.log(mw.provide("nodel", function () { console.log(this); this.reply("999", mw.success, 1);console.log("done"); } ));
console.log("RUNNUNG SERVER");
//mw.runServer();
	    
//console.log(mw.unprovide("node0"));
//console.log(mw.unprovide("node1"));

//setInterval(function() {
    mw.call("time", "data", function(a, b, c) { console.log("call returned", a, b, c); });
//}, 1000);


// Prints: 'world'
//console.log(mw.detach());

