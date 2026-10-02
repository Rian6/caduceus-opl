const fs=require('node:fs'),path=require('node:path');
const root=process.argv[2];
for(const [from,to] of [['caduceus-ra-art.ts','ra-art.ts'],['caduceus-ra-bridge.ts','caduceus-ra-bridge.ts']])fs.copyFileSync(path.join(__dirname,from),path.join(root,'electron',to));
const compat=path.join(root,'electron/ra-compatibility.ts');
let source=fs.readFileSync(compat,'utf8');
source=source.replace('title:game.title,count:game.count,checkedAt:this.index.at','title:game.title,image:game.image,count:game.count,checkedAt:this.index.at');
fs.writeFileSync(compat,source);
const main=path.join(root,'electron/main.ts');source=fs.readFileSync(main,'utf8');
if(!source.includes("from './ra-art'")){
  source="import {RAArtwork} from './ra-art';\n"+source;
  source=source.replace('startCaduceusRABridge(compatibility)','startCaduceusRABridge(compatibility,(hash,image)=>raArtwork.prepare(hash,image))');
  source=source.replace("handle('ra:compatibility-sync'","const raArtwork=new RAArtwork(()=>artDir());\nhandle('ra:compatibility-sync'");
}
fs.writeFileSync(main,source);
const covers=path.join(root,'electron/cover-art.ts');source=fs.readFileSync(covers,'utf8').replace('width:140,height:200','width:192,height:272');fs.writeFileSync(covers,source);
console.log('RA artwork export and 192x272 covers integrated.');
