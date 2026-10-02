const fs=require('node:fs'),path=require('node:path');
const root=process.argv[2];
fs.copyFileSync(path.join(__dirname,'caduceus-ra-bridge.ts'),path.join(root,'electron/caduceus-ra-bridge.ts'));
const file=path.join(root,'electron/main.ts');
let code=fs.readFileSync(file,'utf8');
const before='startCaduceusRABridge(compatibility,(hash,image)=>raArtwork.prepare(hash,image))';
const after="startCaduceusRABridge(compatibility,(hash,image)=>raArtwork.prepare(hash,image),()=>{const state=xera.status();return !!achievements.status().user&&state.running&&!!state.user&&!state.error})";
if(code.includes(before))fs.writeFileSync(file,code.replace(before,after));
else if(!code.includes(after))throw Error('Bridge integration location missing');
