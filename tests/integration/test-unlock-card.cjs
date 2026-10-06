const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
let compiled;
if(process.argv[2]==='--compiled')compiled=fs.readFileSync(process.argv[3],'utf8');
else {const ts=require(process.argv[2]);const source=fs.readFileSync(path.join(__dirname,'../../integrations/caduceus/caduceus-ra-bridge.ts'),'utf8');compiled=ts.transpileModule(source,{compilerOptions:{module:ts.ModuleKind.CommonJS,esModuleInterop:true}}).outputText;}
let receive;const sends=[];
const socket={on:(event,fn)=>{if(event==='message')receive=fn;return socket},bind:()=>{},close:()=>{},send:(body,port,address)=>sends.push({body:body.toString(),port,address})};
const moduleObject={exports:{}};
vm.runInNewContext(compiled,
 {exports:moduleObject.exports,module:moduleObject,require:name=>{assert.equal(name,'node:dgram');return {createSocket:()=>socket}},Buffer,Date,console});
const {startCaduceusRABridge,notifyCaduceusUnlock}=moduleObject.exports;
const stop=startCaduceusRABridge({lookupHash:()=>({status:'compatible',count:120,title:'Bully'})});
notifyCaduceusUnlock({id:123,points:1,title:'Get Off, You Psycho!'});assert.equal(sends.length,0);
receive(Buffer.from('CADQ2 '+ 'a'.repeat(32)),{address:'192.168.1.100',port:1234});
notifyCaduceusUnlock({id:123,points:1,title:'Get Off, You Psycho!'});
assert.deepEqual(sends.at(-1),{body:'RAU1 123 1 Get Off, You Psycho!',port:18195,address:'192.168.1.100'});
notifyCaduceusUnlock({id:124,points:2,title:'\u00c1'.repeat(100)+'\n'});assert.equal(sends.at(-1).body,'RAU1 124 2 '+'A'.repeat(63));
const n=sends.length;notifyCaduceusUnlock({id:NaN,title:'bad',points:1});assert.equal(sends.length,n);
stop();notifyCaduceusUnlock({id:125,title:'closed',points:1});assert.equal(sends.length,n);
console.log('PASS: unlock title/points, console address, ASCII bound, invalid id and shutdown');
