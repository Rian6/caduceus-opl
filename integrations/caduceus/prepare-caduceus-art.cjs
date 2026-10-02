// Run with the Caduceus Electron executable (ELECTRON_RUN_AS_NODE unset).
const {app,nativeImage}=require('electron');
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
app.whenReady().then(()=>{
  const server=process.argv[2],art=process.argv[3];
  const {writeOplCover,migrateLegacyCovers,hasOplCover}=require(path.join(server,'dist-electron/cover-art'));
  const fixture=path.resolve(__dirname,'../../pcsx2-test/cover-fixture');
  fs.mkdirSync(fixture,{recursive:true});
  const source=path.join(server,'public/ps2-splash.png');
  const splash=nativeImage.createFromPath(source);
  assert(!splash.isEmpty());
  fs.writeFileSync(path.join(fixture,'TEST_000.00_COV.jpg'),splash.resize({width:140,height:200}).toJPEG(92));
  assert.equal(migrateLegacyCovers(fixture),1);
  assert.equal(migrateLegacyCovers(fixture),0);
  assert(hasOplCover(fixture,'TEST_000.00'));
  assert(fs.existsSync(path.join(fixture,'TEST_000.00_COV.jpg')));
  assert.equal(writeOplCover(fixture,'../escape',Buffer.from('bad')),false);
  const before=fs.readFileSync(path.join(fixture,'TEST_000.00_COV.png'));
  assert.equal(writeOplCover(fixture,'TEST_000.00',Buffer.from('invalid')),false);
  assert.deepEqual(fs.readFileSync(path.join(fixture,'TEST_000.00_COV.png')),before);
  assert.deepEqual(nativeImage.createFromBuffer(before).getSize(),{width:140,height:200});
  const logo=splash.resize({width:512,height:288,quality:'best'}).toPNG();
  fs.writeFileSync(path.resolve(__dirname,'../../opl/gfx/logo.png'),logo);
  const converted=migrateLegacyCovers(art);
  const result={tests:'PASS: PNG dimensions, migration, idempotence, invalid input, existing cover preservation',splash:{width:512,height:288,bytes:logo.length},converted,art};
  fs.writeFileSync(path.resolve(__dirname,'../../pcsx2-test/art-report.json'),JSON.stringify(result,null,2));
  app.quit();
}).catch(error=>{console.error(error);app.exit(1)});
