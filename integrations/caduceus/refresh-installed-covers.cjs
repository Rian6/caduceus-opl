const {app}=require('electron');
const fs=require('node:fs'),path=require('node:path');
const {DatabaseSync}=require('node:sqlite');
app.whenReady().then(async()=>{
  const server=process.argv[2],share=process.argv[3];
  const {writeOplCover}=require(path.join(server,'dist-electron/cover-art'));
  const db=new DatabaseSync(path.join(app.getPath('appData'),'roms-desktop/catalog.sqlite3'),{readOnly:true});
  const rows=db.prepare('SELECT title,icon,game_id,original_name FROM games WHERE console=?').all('PS2');db.close();
  const results=[];
  for(const row of rows){
    if(!row.original_name||path.basename(row.original_name)!==row.original_name||!fs.existsSync(path.join(share,'DVD',row.original_name)))continue;
    if(!row.icon||!row.game_id){results.push({title:row.title,status:'no registered cover'});continue}
    try{
      const res=await fetch(row.icon,{signal:AbortSignal.timeout(10000)});
      if(!res.ok)throw Error('HTTP '+res.status);
      const ok=writeOplCover(path.join(share,'ART'),row.game_id,Buffer.from(await res.arrayBuffer()));
      results.push({title:row.title,status:ok?'updated 192x272':'invalid image'});
    }catch(error){results.push({title:row.title,status:'preserved existing cover',reason:error.message})}
  }
  fs.writeFileSync(path.resolve(__dirname,'../../pcsx2-test/cover-upgrade-report.json'),JSON.stringify(results,null,2));app.quit();
}).catch(e=>{console.error(e);app.exit(1)});
