const fs=require('node:fs'),path=require('node:path');
const root=process.argv[2];
if(!root)throw Error('Usage: node integrate-caduceus-session.cjs <Caduceus clone>');
const file=path.join(root,'electron/main.ts');
let code=fs.readFileSync(file,'utf8');
const before='startCaduceusRABridge(compatibility,(hash,image)=>raArtwork.prepare(hash,image))';
const after="startCaduceusRABridge(compatibility,(hash,image)=>raArtwork.prepare(hash,image),()=>{const state=xera.status();return !!achievements.status().user&&state.running&&!!state.user&&!state.error})";
if(code.includes(before))code=code.replace(before,after);
else if(!code.includes(after))throw Error('Bridge integration location missing');
if(!code.includes('notifyCaduceusUnlock')){
  const original="import {startCaduceusRABridge}";
  const unlock="if(channel==='xera:unlock'){";
  if(!code.includes(original)||!code.includes(unlock))throw Error('Unlock integration location missing');
  code=code.replace(original,"import {startCaduceusRABridge,notifyCaduceusUnlock}");
  code=code.replace(unlock,unlock+'notifyCaduceusUnlock(value);');
}
fs.copyFileSync(path.join(__dirname,'caduceus-ra-bridge.ts'),path.join(root,'electron/caduceus-ra-bridge.ts'));
fs.writeFileSync(file,code);
