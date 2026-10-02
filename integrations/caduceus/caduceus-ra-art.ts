import fs from 'node:fs';
import path from 'node:path';
import {nativeImage} from 'electron';

// Single small texture per game. The PS2 reads it from its normal ART share.
export class RAArtwork {
  private pending=new Map<string,Promise<void>>();
  private attempted=new Map<string,number>();
  constructor(private directory:()=>string,private download:typeof fetch=fetch){}
  async prepare(hash:string,image?:string|null){
    if(!/^[a-f0-9]{32}$/.test(hash)||!image)return;
    const target=path.join(this.directory(),`${hash}_RA.png`);
    if(fs.existsSync(target))return;
    const queued=this.pending.get(hash);if(queued)return queued;
    if(this.pending.size>=4||Date.now()-(this.attempted.get(hash)||0)<60000)return;
    const url=new URL(image);
    if(url.protocol!=='https:'||url.hostname!=='media.retroachievements.org')return;
    if(this.attempted.size>2000)this.attempted.clear();
    this.attempted.set(hash,Date.now());
    const task=(async()=>{
      const res=await this.download(url,{signal:AbortSignal.timeout(3500),redirect:'error'});
      if(!res.ok||Number(res.headers.get('content-length'))>512*1024)return;
      const reader=res.body?.getReader();if(!reader)return;
      const chunks:Buffer[]=[];let size=0;
      for(;;){const part=await reader.read();if(part.done)break;size+=part.value.length;if(size>512*1024){await reader.cancel();return}chunks.push(Buffer.from(part.value))}
      const icon=nativeImage.createFromBuffer(Buffer.concat(chunks));
      if(icon.isEmpty())return;
      fs.mkdirSync(path.dirname(target),{recursive:true});
      const temp=target+'.tmp';
      try{fs.writeFileSync(temp,icon.resize({width:64,height:64,quality:'best'}).toPNG());fs.renameSync(temp,target)}
      finally{if(fs.existsSync(temp))fs.unlinkSync(temp)}
    })().catch(()=>{}).finally(()=>this.pending.delete(hash));
    this.pending.set(hash,task);
    return task;
  }
}
