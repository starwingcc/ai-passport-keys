/* Local-only protocol primitives. No browser, device or crypto service needed. */
(function (root) {
  'use strict';
  const CHUNK = 16 * 1024, WINDOW = 256 * 1024, TEXT_BYTES = 8000;
  const MAX_FILE = 8 * 1024 ** 3, MAX_FILES = 8;
  const K = [0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
  const rotr = (x,n) => (x >>> n) | (x << (32-n));
  class SHA256 {
    constructor() {
      this.h = new Uint32Array([0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19]);
      this.block = new Uint8Array(64); this.words = new Uint32Array(64); this.used = 0; this.bytes = 0; this.done = false;
    }
    compress() {
      const w=this.words, b=this.block;
      for(let i=0;i<16;i++)w[i]=(b[i*4]<<24)|(b[i*4+1]<<16)|(b[i*4+2]<<8)|b[i*4+3];
      for(let i=16;i<64;i++){
        const a=w[i-15],c=w[i-2];
        w[i]=(w[i-16]+(rotr(a,7)^rotr(a,18)^(a>>>3))+w[i-7]+(rotr(c,17)^rotr(c,19)^(c>>>10)))>>>0;
      }
      let [a,b0,c,d,e,f,g,h]=this.h;
      for(let i=0;i<64;i++){
        const t1=(h+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^(~e&g))+K[i]+w[i])>>>0;
        const t2=((rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b0)^(a&c)^(b0&c)))>>>0;
        h=g;g=f;f=e;e=(d+t1)>>>0;d=c;c=b0;b0=a;a=(t1+t2)>>>0;
      }
      [a,b0,c,d,e,f,g,h].forEach((v,i)=>this.h[i]=(this.h[i]+v)>>>0);
    }
    update(data) {
      if(this.done)throw Error('摘要已结束');
      const a=data instanceof Uint8Array?data:new Uint8Array(data); this.bytes+=a.byteLength;
      let i=0;
      while(i<a.length){const n=Math.min(64-this.used,a.length-i);this.block.set(a.subarray(i,i+n),this.used);this.used+=n;i+=n;if(this.used===64){this.compress();this.used=0;}}
      return this;
    }
    hex() {
      if(this.done)throw Error('摘要已结束');this.done=true;
      this.block[this.used++]=0x80;
      if(this.used>56){this.block.fill(0,this.used);this.compress();this.used=0;}
      this.block.fill(0,this.used,56);
      const bits=this.bytes*8,view=new DataView(this.block.buffer);
      view.setUint32(56,Math.floor(bits/2**32));view.setUint32(60,bits>>>0);this.compress();
      return Array.from(this.h,v=>v.toString(16).padStart(8,'0')).join('');
    }
  }
  function validFile(f) {
    return !!f && /^[a-zA-Z0-9_-]{1,40}$/.test(f.id) && typeof f.name==='string' && f.name.length>0 &&
      new TextEncoder().encode(f.name).length<=180 && typeof f.mime==='string' && f.mime.length<=80 &&
      Number.isSafeInteger(f.size) && f.size>=0 && f.size<=MAX_FILE;
  }
  function chunkSize(max) {
    if(max===0||max===Infinity||max===undefined)return CHUNK;
    if(!Number.isFinite(max)||max<1024)throw Error('浏览器消息容量过小');
    return Math.min(CHUNK,Math.floor(max));
  }
  function ackValid(offset, previous, sent) { return Number.isSafeInteger(offset)&&offset>=previous&&offset<=sent; }
  root.PassportTransferProtocol={SHA256,CHUNK,WINDOW,TEXT_BYTES,MAX_FILE,MAX_FILES,validFile,chunkSize,ackValid};
  if(typeof module!=='undefined')module.exports=root.PassportTransferProtocol;
})(globalThis);
