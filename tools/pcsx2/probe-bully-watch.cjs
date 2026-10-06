// Read-only protocol probe: identifies Bully and downloads its watch list.
// Does not submit snapshots or unlock/award achievements.
const fs=require('node:fs'),path=require('node:path'),dgram=require('node:dgram');
const root=process.argv[2],iso=process.argv[3];
const {hashPS2}=require(path.join(root,'dist-electron/ra-iso.js'));
const socket=dgram.createSocket('udp4');
const ask=req=>new Promise((resolve,reject)=>{
 const receive=body=>{clearTimeout(timer);resolve(body)};
 const timer=setTimeout(()=>{socket.off('message',receive);reject(Error('Xerabora timeout'))},1500);
 socket.once('message',receive);socket.send(req,18194,'127.0.0.1');
});
(async()=>{try{
 const hash=await hashPS2(iso);await new Promise(resolve=>socket.bind(0,'127.0.0.1',resolve));
 const address=`127.0.0.1 ${socket.address().port}`;
 let reply;for(let i=0;i<25;i++){
  reply=(await ask(`RAQ1 ${hash} SLUS_212.69 ${address}`)).toString().split('\0')[0];
  if(!reply.startsWith('RAA1 WAIT'))break;
  await new Promise(resolve=>setTimeout(resolve,500));
 }
 const match=/^RAA1 OK (\d+) (\d+)/.exec(reply);if(!match)throw Error(reply);
 const total=Number(match[1]),chunks=Number(match[2]),parts=[];
 if(total>20480||chunks>24)throw Error('Invalid watch size');
 for(let i=0;i<chunks;i++){
  const data=await ask(`RAG1 ${hash} ${i} ${address}`);
  const header=/^RAC1 (\d+) (\d+) /.exec(data.toString('ascii'));
  if(!header||Number(header[1])!==i)throw Error('Invalid chunk');
  const length=Number(header[2]);if(length>896||header[0].length+length>data.length)throw Error('Invalid chunk length');
  parts.push(data.subarray(header[0].length,header[0].length+length));
 }
 const watch=Buffer.concat(parts);if(watch.length!==total)throw Error('Truncated watch');
 fs.writeFileSync(path.join(__dirname,'../../pcsx2-test/live-bully-watch.bin'),watch);
 console.log(`PASS: real Bully watch downloaded: ${total} bytes, ${chunks} chunks; ${reply}`);
}finally{socket.close()}})().catch(error=>{console.error(error.message);process.exitCode=1});
