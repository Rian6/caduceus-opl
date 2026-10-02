import dgram from 'node:dgram';
import {RACompatibility} from './ra-compatibility';

// Catalog compatibility only. No credentials, file paths or telemetry leave the PC.
export function startCaduceusRABridge(catalog:RACompatibility,prepareIcon?:(hash:string,image?:string|null)=>Promise<void>,sessionReady:()=>boolean=()=>false){
  const socket=dgram.createSocket('udp4');
  let closed=false,windowStart=0,requests=0;
  socket.on('error',error=>{console.error('Caduceus OPL compatibility:',error.message);closed=true;try{socket.close()}catch{}});
  socket.on('message',(message,peer)=>{
    if(message.length!==38)return;
    const match=/^CADQ([12]) ([a-f0-9]{32})$/.exec(message.toString('ascii'));
    if(!match)return;
    const now=Date.now();
    if(now-windowStart>=1000){windowStart=now;requests=0}
    if(++requests>32)return;
    const version=match[1],hash=match[2];
    const result=catalog.lookupHash(hash);
    const reply=()=>{
    const title=(result.title||'').normalize('NFKD').replace(/[\u0300-\u036f]/g,'').replace(/[^\x20-\x7e]/g,' ').slice(0,90);
    const answer=result.status==='compatible'?`OK ${result.count} ${title}`:result.status==='unmatched'?'NO':'UNKNOWN';
    // PS2 RPC receive path requires aligned replies >=128 bytes, multiples of 64.
    const session=version==='2'?`${sessionReady()?'READY':'OFFLINE'} `:'';
    const body=Buffer.from(`CADR${version} ${hash} ${session}${answer}`,'ascii');
    const packet=Buffer.alloc(Math.max(128,Math.ceil((body.length+1)/64)*64));body.copy(packet);
    if(!closed)socket.send(packet,peer.port,peer.address);
    };
    // Publish artwork before replying so the OPL does not cache a missing file.
    if(result.status==='compatible'&&prepareIcon)void prepareIcon(hash,result.image).then(reply,reply);
    else reply();
  });
  socket.bind(18197,'0.0.0.0');
  return ()=>{if(!closed){closed=true;socket.close()}};
}
