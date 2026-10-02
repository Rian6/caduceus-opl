const fs=require('node:fs'),path=require('node:path');
const root=process.argv[2],main=path.join(root,'electron/main.ts');
let source=fs.readFileSync(main,'utf8');
if(!source.includes("from './cover-art'")){
  const before='fs.writeFileSync(path.join(artDir(),`${id}_COV.jpg`),image.resize({width:140,height:200,quality:\'best\'}).toJPEG(92));return true';
  if(!source.includes(before))throw Error('Cover installer not found');
  source="import {writeOplCover,hasOplCover,migrateLegacyCovers} from './cover-art';\n"+source;
  source=source.replace(before,'return writeOplCover(artDir(),id,buf)');
  source=source.replace("fs.existsSync(path.join(artDir(),`${id}_COV.jpg`))",'hasOplCover(artDir(),id)');
  source=source.replace("handle('games:installed',async()=>{","handle('games:installed',async()=>{migrateLegacyCovers(artDir());");
  source=source.replace("handle('covers:repair',async()=>{","handle('covers:repair',async()=>{migrateLegacyCovers(artDir());");
  source=source.replace('const ok=await installCover(g.icon,id);','const ok=(await installCover(g.icon,id))||hasOplCover(artDir(),id);');
  source=source.replace('database();ensureRuntime();if(achievements.status().user)','database();ensureRuntime();migrateLegacyCovers(artDir());if(achievements.status().user)');
  fs.writeFileSync(main,source);
}
fs.copyFileSync(path.join(__dirname,'caduceus-cover-art.ts'),path.join(root,'electron/cover-art.ts'));
console.log('PNG cover installation and legacy migration integrated.');
