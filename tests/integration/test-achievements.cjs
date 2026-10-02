const assert=require('node:assert/strict'),dgram=require('node:dgram'),path=require('node:path');
const {startCaduceusAchievements}=require(path.join(process.argv[2],'dist-electron/caduceus-achievements'));
const key='1'.repeat(64),hash='a'.repeat(32),port=18200;
let user='Player',calls=0,fail=false;
const account={status:()=>({user}),games:async page=>{calls++;return{total:105,games:Array.from({length:page?5:100},(_,i)=>({id:page*100+i+1,title:'Game '+(page*100+i+1),console:'PlayStation 2',total:100,earned:25,hardcore:10,image:null}))}},
game:async id=>{calls++;if(fail)throw Error('network');return{title:'Jogo\ncom\ttitulo',achievements:Array.from({length:8},(_,i)=>({id:i+1,title:'Á'.repeat(100),description:'Description'.repeat(40),points:5,earned:i<3,hardcore:i===0,date:i<3?'2026-01-01':'',image:'https://media.retroachievements.org/Badge/123.png'}))}}};
const stop=startCaduceusAchievements(account,{lookupHash:()=>({status:'compatible',id:3057})},async()=>{throw Error('image unavailable')},key,port);
const client=dgram.createSocket('udp4');let nonce=0;
function ask(kind,page=0,filter=0,target='0',auth=key,timeout=1500){
 const id=++nonce;return new Promise((resolve,reject)=>{
  const timer=setTimeout(()=>{client.off('message',receive);reject(Error('timeout'))},timeout);
  function receive(data){const text=data.toString().split('\0')[0];if(!text.startsWith(`CADB1 ${id} `)||text.endsWith(' WAIT'))return;
   clearTimeout(timer);client.off('message',receive);assert(data.length>=128&&data.length<=960&&data.length%64===0);resolve(text.slice(`CADB1 ${id} `.length));}
  client.on('message',receive);client.send(`CADA1 ${id} ${kind} ${page} ${filter} ${target} ${auth}`,port,'127.0.0.1');
 });
}
(async()=>{try{
 await assert.rejects(ask('G',0,0,'0','2'.repeat(64),200),/timeout/);assert.equal(calls,0);
 let result=await ask('G',33);let rows=result.split('\n');assert.equal(rows.length,4);assert(rows[1].startsWith('100\t'));assert(rows[2].startsWith('101\t'));assert(rows[3].startsWith('102\t'));
 result=await ask('A',0,0,hash);rows=result.split('\n');assert(rows[0].startsWith('OK\tA\t0\t8\t3057\t3\t8\tPlayer\tJogo com titulo'));assert.equal(rows.length,4);
 result=await ask('A',0,1,'3057');assert.equal(result.split('\n').length,4);assert(result.includes('OK\tA\t0\t3\t'));
 result=await ask('A',0,2,'3057');assert(result.includes('OK\tA\t0\t5\t'));assert(result.split('\n')[1].startsWith('4\t'));
 result=await ask('A',0,3,'3057');assert(result.includes('OK\tA\t0\t1\t'));
 result=await ask('A',3,0,'3057');assert.equal(result.split('\n')[1],'');
 fail=true;assert.equal(await ask('A',0,0,'3057'),'ERROR');fail=false;
 user=null;assert.equal(await ask('G'),'OFFLINE');
 user='Player';let complete;
 account.games=async()=>new Promise(resolve=>{complete=resolve});
 const changed=ask('G');
 for(let i=0;i<20&&!complete;i++)await new Promise(resolve=>setTimeout(resolve,10));
 assert(complete);user='DifferentAccount';complete({total:0,games:[]});
 assert.equal(await changed,'OFFLINE');
 console.log('PASS: unauthorized requests rejected before account access, cross-page library, hash lookup, filters, bounded packets, missing artwork and offline/error states');
 }finally{client.close();stop()}})().catch(e=>{console.error(e);process.exitCode=1});
