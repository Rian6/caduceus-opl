const fs=require('node:fs'),path=require('node:path');
const root=process.argv[2];
if(!root)throw Error('Informe o clone do Caduceus');
fs.copyFileSync(path.join(__dirname,'caduceus-achievements.ts'),path.join(root,'electron/caduceus-achievements.ts'));
const file=path.join(root,'electron/main.ts');let code=fs.readFileSync(file,'utf8');
if(!code.includes("from './caduceus-achievements'"))code="import {startCaduceusAchievements,loadAchievementsKey} from './caduceus-achievements';\n"+code;
const anchor="app.on('will-quit',stopRABridge);";
if(!code.includes('const stopAchievements=')){
 if(!code.includes(anchor))throw Error('Ponte Caduceus nao encontrada');
 code=code.replace(anchor,anchor+"const stopAchievements=startCaduceusAchievements(achievements,compatibility,(key,image)=>raArtwork.prepare(key,image),loadAchievementsKey(artDir()));app.on('will-quit',stopAchievements);");
}
fs.writeFileSync(file,code);
