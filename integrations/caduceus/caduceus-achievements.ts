import dgram from 'node:dgram';
import {createHash,randomBytes,timingSafeEqual} from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';

type Entry={id:number;title:string;description?:string;console?:string;image?:string|null;total?:number;earned:number|boolean;hardcore:number|boolean;points?:number;date?:string};
type Account={status():{user:string|null};games(page:number):Promise<{total:number;games:Entry[]}>;game(id:number):Promise<{title:string;achievements:Entry[]}>};
type Catalog={lookupHash(hash:string):{status:string;id?:number}};
const clean=(value:unknown,size:number)=>String(value??'').normalize('NFKD').replace(/[\u0300-\u036f]/g,'').replace(/[^\x20-\x7e]/g,' ').slice(0,size);
const number=(value:unknown)=>Math.max(0,Math.min(10000000,Math.floor(Number(value)||0)));

// Capability granted through the user's existing PS2 share. Not an RA credential.
export function loadAchievementsKey(artDirectory:string){
  fs.mkdirSync(artDirectory,{recursive:true});
  const file=path.join(artDirectory,'CADUCEUS.KEY');
  if(fs.existsSync(file)){const key=fs.readFileSync(file,'utf8').trim();if(/^[a-f0-9]{64}$/.test(key))return key;}
  const key=randomBytes(32).toString('hex');fs.writeFileSync(file,key,{mode:0o600});return key;
}

// Read-only account data. Login secrets remain inside Account on the PC.
export function startCaduceusAchievements(account:Account,catalog:Catalog,
  prepareIcon:(key:string,image?:string|null)=>Promise<void>,accessKey:string,port=18198){
  if(!/^[a-f0-9]{64}$/.test(accessKey))throw Error('Chave de pareamento invalida');
  const socket=dgram.createSocket('udp4');
  const pending=new Map<string,{at:number;user:string;body?:string}>();
  let closed=false,start=0,requests=0;
  const send=(body:string,peer:dgram.RemoteInfo)=>{
    if(closed)return;
    const bytes=Buffer.from(body,'ascii');
    if(bytes.length>=960)return;
    const packet=Buffer.alloc(Math.max(128,Math.ceil((bytes.length+1)/64)*64));bytes.copy(packet);
    socket.send(packet,peer.port,peer.address);
  };
  socket.on('error',()=>{closed=true;try{socket.close()}catch{}});
  socket.on('message',(packet,peer)=>{
    if(packet.length>180)return;
    const match=/^CADA1 ([0-9]{1,10}) ([GA]) ([0-9]{1,6}) ([0-3]) ([0-9]{1,8}|[a-f0-9]{32}) ([a-f0-9]{64})$/.exec(packet.toString('ascii'));
    if(!match)return;
    if(!timingSafeEqual(Buffer.from(match[6],'hex'),Buffer.from(accessKey,'hex')))return;
    const [,nonce,kind,pageText,filterText,target]=match;
    const prefix=`CADB1 ${nonce} `,page=Number(pageText),filter=Number(filterText);
    const user=account.status().user;
    const now=Date.now();if(now-start>=1000){start=now;requests=0}if(++requests>40)return;
    if(!user){send(prefix+'OFFLINE',peer);return;}
    for(const [key,value] of pending)if(now-value.at>30000||value.user!==user)pending.delete(key);
    const key=`${peer.address}:${peer.port}:${packet.toString('ascii')}`;
    const existing=pending.get(key);
    if(existing){send(existing.body||prefix+'WAIT',peer);return;}
    if(pending.size>=32){send(prefix+'BUSY',peer);return;}
    const record:{at:number;user:string;body?:string}={at:now,user};pending.set(key,record);
    send(prefix+'WAIT',peer);
    void (async()=>{
      let entries:Entry[],title='Minha biblioteca',total=0,earned=0,maximum=0,gameId=0;
      const offset=page*3;
      if(kind==='G'){
        const block=Math.floor(offset/100),local=offset%100;
        const data=await account.games(block);total=data.total;
        entries=data.games.slice(local,local+3);
        if(entries.length<3&&offset+entries.length<total){const next=await account.games(block+1);entries.push(...next.games.slice(0,3-entries.length));}
      }else{
        if(target.length===32){const game=catalog.lookupHash(target);if(game.status!=='compatible'||!game.id){record.body=prefix+'UNSUPPORTED';return;}gameId=game.id;}
        else gameId=Number(target);
        if(!Number.isSafeInteger(gameId)||gameId<=0){record.body=prefix+'UNSUPPORTED';return;}
        const data=await account.game(gameId);title=data.title;maximum=data.achievements.length;
        earned=data.achievements.filter(a=>a.earned).length;
        const filtered=data.achievements.filter(a=>filter===0||(filter===1?!!a.earned:filter===2?!a.earned:!!a.hardcore));
        total=filtered.length;entries=filtered.slice(offset,offset+3);
      }
      const rows=await Promise.all(entries.map(async entry=>{
        const icon=entry.image?createHash('sha256').update(entry.image).digest('hex').slice(0,32):'-';
        if(entry.image)await prepareIcon(icon,entry.image).catch(()=>{});
        return [number(entry.id),number(entry.total),number(entry.earned),number(entry.hardcore),number(entry.points),icon,
          clean(entry.title,56),clean(entry.description||entry.console,100),clean(entry.date,19)].join('\t');
      }));
      record.body=prefix+`OK\t${kind}\t${page}\t${number(total)}\t${gameId}\t${earned}\t${maximum}\t${clean(user,24)}\t${clean(title,56)}\n`+rows.join('\n');
    })().catch(()=>{record.body=prefix+'ERROR';}).finally(()=>{
      if(account.status().user!==user){pending.delete(key);send(prefix+'OFFLINE',peer);return;}
      if(record.body)send(record.body,peer);
    });
  });
  socket.bind(port,'0.0.0.0');
  return ()=>{closed=true;pending.clear();socket.close();};
}
