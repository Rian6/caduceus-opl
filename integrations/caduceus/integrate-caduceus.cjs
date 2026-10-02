// Apply the companion changes to the user-specified sibling Caduceus checkout.
const fs=require('node:fs'),path=require('node:path');
const root=process.argv[2];
if(!root)throw Error('Pass the Caduceus checkout path');
const compat=path.join(root,'electron','ra-compatibility.ts');
let source=fs.readFileSync(compat,'utf8');
if(!source.includes('lookupHash(hash:string)')){
  const marker='  async check(file?:string)';
  if(!source.includes(marker))throw Error('Compatibility insertion point missing');
  source=source.replace(marker,`  lookupHash(hash:string):RACompatible{
    this.restore();
    if(!this.index){void this.sync().catch(()=>{});return{status:'unknown'}};
    const game=this.hashes.get(hash.toLowerCase());
    return game&&game.count>0?{status:'compatible',hash,id:game.id,title:game.title,count:game.count,checkedAt:this.index.at}:{status:'unmatched',hash,checkedAt:this.index.at};
  }
${marker}`);
  fs.writeFileSync(compat,source);
}
const main=path.join(root,'electron','main.ts');
source=fs.readFileSync(main,'utf8');
if(!source.includes("from './caduceus-ra-bridge'")){
  const marker='app.whenReady().then(async()=>{';
  if(!source.includes(marker))throw Error('Startup insertion point missing');
  source="import {startCaduceusRABridge} from './caduceus-ra-bridge';\n"+source;
  source=source.replace(marker,marker+"const stopRABridge=startCaduceusRABridge(compatibility);app.on('will-quit',stopRABridge);");
  fs.writeFileSync(main,source);
}
fs.copyFileSync(path.join(__dirname,'caduceus-ra-bridge.ts'),path.join(root,'electron','caduceus-ra-bridge.ts'));
console.log('Caduceus compatibility bridge integrated.');
