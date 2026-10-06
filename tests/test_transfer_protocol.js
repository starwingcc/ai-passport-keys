const assert=require('node:assert/strict'),crypto=require('node:crypto');
const P=require('../main/web/transfer-protocol.js');
for(const size of [0,1,55,56,63,64,65,1000,100000]){
  const data=crypto.randomBytes(size),h=new P.SHA256();
  for(let offset=0;offset<size;offset+=37)h.update(data.subarray(offset,offset+37));
  assert.equal(h.hex(),crypto.createHash('sha256').update(data).digest('hex'));
  assert.throws(()=>h.update(data));
}
assert(P.validFile({id:'test',name:'图片😀.png',mime:'image/png',size:100*1024*1024}));
for(const size of [-1,NaN,Infinity,1.1,P.MAX_FILE+1])assert(!P.validFile({id:'x',name:'x',mime:'',size}));
assert(!P.validFile({id:'<script>',name:'x',mime:'',size:0}));
assert.equal(P.chunkSize(65536),16384);assert.equal(P.chunkSize(4096),4096);assert.equal(P.chunkSize(0),16384);assert.throws(()=>P.chunkSize(100));
assert(P.ackValid(100,10,100));assert(!P.ackValid(101,10,100));assert(!P.ackValid(9,10,100));assert(!P.ackValid(10.1,10,100));
console.log('Transfer protocol: incremental SHA-256, Unicode metadata, sizes, negotiated chunks and acknowledgments: PASS');
