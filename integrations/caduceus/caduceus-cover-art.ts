import fs from 'node:fs';
import path from 'node:path';
import {nativeImage} from 'electron';

const validId=(id:string)=>/^[A-Z0-9_][A-Z0-9_.-]{1,31}$/i.test(id);
export function hasOplCover(directory:string,id:string){
  if(!validId(id))return false;
  try{return !nativeImage.createFromPath(path.join(directory,`${id}_COV.png`)).isEmpty()}catch{return false}
}
export function writeOplCover(directory:string,id:string,bytes:Buffer){
  if(!validId(id))return false;
  const image=nativeImage.createFromBuffer(bytes);
  if(image.isEmpty())return false;
  fs.mkdirSync(directory,{recursive:true});
  const target=path.join(directory,`${id}_COV.png`),temp=target+'.tmp';
  try{
    fs.writeFileSync(temp,image.resize({width:192,height:272,quality:'best'}).toPNG());
    fs.renameSync(temp,target);
    return true;
  }finally{if(fs.existsSync(temp))fs.unlinkSync(temp)}
}
export function migrateLegacyCovers(directory:string){
  let converted=0;
  if(!fs.existsSync(directory))return converted;
  for(const name of fs.readdirSync(directory)){
    const match=/^(.+)_COV\.jpe?g$/i.exec(name);
    if(!match||hasOplCover(directory,match[1]))continue;
    try{if(writeOplCover(directory,match[1],fs.readFileSync(path.join(directory,name))))converted++}catch{}
  }
  return converted;
}
