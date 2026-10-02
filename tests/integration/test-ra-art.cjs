const {app,nativeImage}=require('electron');
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
app.whenReady().then(async()=>{
  const server=process.argv[2],art=process.argv[3];
  const {RAArtwork}=require(path.join(server,'dist-electron/ra-art'));
  const {writeOplCover}=require(path.join(server,'dist-electron/cover-art'));
  const dir=fs.mkdtempSync(path.join(app.getPath('temp'),'caduceus-art-test-'));
  const bytes=nativeImage.createFromPath(path.join(server,'public/ps2-splash.png')).resize({width:192,height:272}).toPNG();
  let calls=0;
  const fake=async()=>{calls++;await new Promise(r=>setTimeout(r,20));return new Response(bytes,{status:200})};
  const hash='0123456789abcdef0123456789abcdef',url='https://media.retroachievements.org/Images/test.png';
  try{
    const service=new RAArtwork(()=>dir,fake);
    await Promise.all([service.prepare(hash,url),service.prepare(hash,url)]);
    assert.equal(calls,1);
    assert.deepEqual(nativeImage.createFromPath(path.join(dir,hash+'_RA.png')).getSize(),{width:64,height:64});
    await service.prepare(hash,url);assert.equal(calls,1);
    await service.prepare('a'.repeat(32),'http://example.com/icon.png');assert.equal(calls,1);
    const failure=new RAArtwork(()=>dir,async()=>{throw Error('offline')});
    await failure.prepare('b'.repeat(32),url);
    assert(!fs.existsSync(path.join(dir,'b'.repeat(32)+'_RA.png')));
    assert(writeOplCover(dir,'TEST_000.00',bytes));
    assert.deepEqual(nativeImage.createFromPath(path.join(dir,'TEST_000.00_COV.png')).getSize(),{width:192,height:272});
  }finally{for(const name of fs.readdirSync(dir))fs.unlinkSync(path.join(dir,name));fs.rmdirSync(dir)}
  // Use the user's real catalog to prepare Bully's actual icon in the share.
  const data=JSON.parse(fs.readFileSync(path.join(app.getPath('appData'),'roms-desktop/cache/ra-compatibility.json')));
  const bullyHash='3e7814bf67c0618853065c95c5bbb5ae',game=data.games.find(g=>g.hashes.includes(bullyHash));
  if(game)await new RAArtwork(()=>art).prepare(bullyHash,game.image);
  const report={tests:'PASS: deduplication, disk cache, trusted image origin, offline fallback, RA 64x64, cover 192x272',realIcon:fs.existsSync(path.join(art,bullyHash+'_RA.png'))};
  fs.writeFileSync(path.resolve(__dirname,'../../pcsx2-test/ra-art-report.json'),JSON.stringify(report,null,2));
  app.quit();
}).catch(e=>{console.error(e);app.exit(1)});
