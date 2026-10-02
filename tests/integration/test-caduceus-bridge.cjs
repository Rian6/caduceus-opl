const assert=require('node:assert/strict'),dgram=require('node:dgram'),fs=require('node:fs'),os=require('node:os'),path=require('node:path');
const root=process.argv[2];
const {RACompatibility}=require(path.join(root,'dist-electron/ra-compatibility'));
// Isolate the fixture from an already-running Caduceus instance.
const Module=require('node:module');
const bridgeFile=path.join(root,'dist-electron/caduceus-ra-bridge.js');
const bridgeModule=new Module(bridgeFile,module);
bridgeModule.paths=Module._nodeModulePaths(path.dirname(bridgeFile));
bridgeModule._compile(fs.readFileSync(bridgeFile,'utf8').replace("socket.bind(18197,", "socket.bind(18199,"),bridgeFile);
const {startCaduceusRABridge}=bridgeModule.exports;
(async()=>{
  const dir=fs.mkdtempSync(path.join(os.tmpdir(),'cad-bridge-'));
  const hash='0123456789abcdef0123456789abcdef';
  const catalog=new RACompatibility(()=>path.join(dir,'cache.json'),async()=>[{id:1,title:'Teste de jogo',image:null,count:120,hashes:[hash]}]);
  await catalog.sync();
  assert.equal(catalog.lookupHash(hash).status,'compatible');
  assert.equal(catalog.lookupHash('f'.repeat(32)).status,'unmatched');
  let ready=false;
  const stop=startCaduceusRABridge(catalog,undefined,()=>ready),client=dgram.createSocket('udp4');
  const ask=(hash,version=1)=>new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(Error('Reply timed out')),2000);
    client.once('message',data=>{clearTimeout(timer);resolve(data)});
    client.send(Buffer.from('CADQ'+version+' '+hash),18199,'127.0.0.1');
  });
  try{
    let data=await ask(hash);
    assert(data.length>=128&&data.length%64===0);
    assert.equal(data.toString().split('\0')[0],`CADR1 ${hash} OK 120 Teste de jogo`);
    data=await ask('f'.repeat(32));
    assert.equal(data.toString().split('\0')[0],`CADR1 ${'f'.repeat(32)} NO`);
    data=await ask(hash,2);
    assert.equal(data.toString().split('\0')[0],`CADR2 ${hash} OFFLINE OK 120 Teste de jogo`);
    ready=true;
    data=await ask(hash,2);
    assert.equal(data.toString().split('\0')[0],`CADR2 ${hash} READY OK 120 Teste de jogo`);
    data=await ask('f'.repeat(32),2);
    assert.equal(data.toString().split('\0')[0],`CADR2 ${'f'.repeat(32)} READY NO`);
    assert(data.length>=128&&data.length%64===0);
    console.log('PASS: catalog identity, supported/unmatched, offline/authenticated session and aligned replies');
  }finally{client.close();stop();fs.rmSync(dir,{recursive:true,force:true})}
})().catch(e=>{console.error(e);process.exitCode=1});
