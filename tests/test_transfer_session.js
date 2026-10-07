/* Deterministic browser-session/UI regressions; saved-byte integrity is tested in a browser. */
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
function page(rtc,closeEvent=true,navigator={}){
  let now=0,nextTimer=1,nextNonce=0;
  const timers=new Map(),sockets=[],events={},nodes=new Map(),clipboard=[];
  class Element{
    constructor(tag='div'){this.tag=tag;this.children=[];this.value='';this.textContent='';this.hidden=false;this.disabled=false;this.classList={toggle(){}};this.dataset={};this.style={};this.attributes={};this.isConnected=true;}
    append(...children){for(const child of children)child.parent=this;this.children.push(...children);}
    replaceChildren(...children){this.children=[];this._text='';this.append(...children);}
    get textContent(){return this.children.length?this.children.map(c=>c.textContent).join(''):this._text;}
    set textContent(text){this._text=text;this.children=[];}
    setAttribute(name,value){this.attributes[name]=value;}
    remove(){this.parent.children=this.parent.children.filter(c=>c!==this);this.isConnected=false;}
    select(){this.selected=true;}
    focus(){}
    get firstChild(){return this.children[0];}
    get childElementCount(){return this.children.length;}
    get options(){return this.children;}
  }
  const node=id=>{if(!nodes.has(id))nodes.set(id,new Element());return nodes.get(id);};
  node('target').value='all';
  class Socket{
    static OPEN=1;
    constructor(){this.readyState=0;this.bufferedAmount=0;this.sent=[];sockets.push(this);}
    send(data){this.sent.push(JSON.parse(data));}
    close(){if(this.readyState===3)return;this.readyState=3;if(closeEvent)this.onclose?.({code:1000});}
    receive(message){this.readyState=1;this.onmessage({data:JSON.stringify(message)});}
  }
  const schedule=(fn,ms,interval=0)=>{const id=nextTimer++;timers.set(id,{fn,at:now+ms,interval});return id;};
  const advance=ms=>{const until=now+ms;for(;;){const entry=[...timers].filter(([,t])=>t.at<=until).sort((a,b)=>a[1].at-b[1].at)[0];if(!entry)break;const [id,t]=entry;now=t.at;timers.delete(id);if(t.interval)timers.set(id,{...t,at:now+t.interval});t.fn();}now=until;};
  const context=vm.createContext({
    document:{getElementById:node,createElement:tag=>new Element(tag),createElementNS:(ns,tag)=>{const e=new Element(tag);e.namespaceURI=ns;return e;},createTextNode:text=>{const e=new Element('#text');e.textContent=text;return e;},body:node('body'),execCommand:command=>{assert.equal(command,'copy');clipboard.push(node('body').children.find(e=>e.selected).value);return true;},visibilityState:'visible'},
    navigator:{userAgent:'Chrome',platform:'MacIntel',maxTouchPoints:0,...navigator},
    location:{protocol:'http:',host:'192.168.1.11'},localStorage:{getItem(){return null;},setItem(){}},
    RTCPeerConnection:rtc,WebSocket:Socket,TextEncoder,Uint8Array,ArrayBuffer,Blob,URL,
    Date:class extends Date{static now(){return now;}},crypto:{getRandomValues:a=>a.fill(++nextNonce)},
    setTimeout:(fn,ms)=>schedule(fn,ms),setInterval:(fn,ms)=>schedule(fn,ms,ms),
    clearTimeout:id=>timers.delete(id),clearInterval:id=>timers.delete(id),
    addEventListener:(name,fn)=>events[name]=fn
  });
  for(const name of ['transfer-protocol.js','transfer.js'])vm.runInContext(fs.readFileSync(`main/web/${name}`,'utf8'),context);
  const welcome=(socket=sockets.at(-1))=>{socket.receive({type:'welcome',id:1});socket.receive({type:'peers',peers:[{id:1,name:'本机',rtc:!!rtc},{id:2,name:'电脑',rtc:true}]});advance(100);};
  const find=(root,predicate)=>predicate(root)?root:root.children.map(c=>find(c,predicate)).find(Boolean);
  return {context,node,sockets,events,welcome,advance,clipboard,find};
}
async function receiveBudget(navigator,limit){
  const p=page(function(){},true,navigator);p.welcome();
  p.sockets[0].receive({type:'peers',peers:[1,2,3,4].map(id=>({id,name:String(id),rtc:true}))});
  const app=p.context.PassportTransfer,sent=[];
  for(const id of [2,3,4])app.links.set(id,{
    id,ready:Promise.resolve(),pc:{close(){}},
    dc:{readyState:'open',send:data=>sent.push(JSON.parse(data)),close(){}}
  });
  const file=size=>({id:'movie',name:'movie.bin',mime:'application/octet-stream',size});
  // Metadata only: exercise actual admission/reservation without allocating a movie.
  await assert.rejects(app.receive(2,file(limit+1)),/上限/);
  assert.equal(sent.length,0);
  let connected;
  app.links.get(2).ready=new Promise(resolve=>{connected=resolve;});
  const pending=app.receive(2,file(limit));
  await assert.rejects(app.receive(3,file(1)),/容量已满/);
  connected();await pending;
  assert.equal(sent.filter(m=>m.type==='get').length,1); // exact boundary is accepted
  p.advance(100);
  p.find(p.node('catalog'),e=>e.tag==='button'&&e.textContent==='取消').onclick(); // inline cancel releases budget
  assert.equal(app.history[0].reserved,0);
  await app.receive(2,file(limit*5/8));
  await app.receive(3,file(limit*3/8));
  await assert.rejects(app.receive(4,file(1)),/容量已满/); // total across senders
  p.advance(100);
  p.find(p.node('catalog'),e=>e.tag==='button'&&e.textContent==='取消').onclick();
  await app.receive(4,file(1));
  assert.equal(app.links.get(4).rx.file.size,1);
  assert.match(p.node('save-mode').textContent,new RegExp(`${limit/1024**2} MB`));
}
async function uiRegressions(){
  // Insecure LAN pages and denied Clipboard API access both copy immediately.
  for(const navigator of [{},{clipboard:{writeText:async()=>{throw Error('denied');}}}]){
    const p=page(function(){},true,navigator);p.welcome();
    const text='中文\nemoji 😀\n<script>纯文本</script>';
    p.context.PassportTransfer.texts.unshift({name:'手机',text,status:'已收到'});
    p.sockets[0].receive({type:'peers',peers:[{id:1,name:'本机'},{id:2,name:'手机'}]});p.advance(100);
    const copy=p.find(p.node('texts'),e=>e.tag==='button');await copy.onclick();
    assert.deepEqual(p.clipboard,[text]);assert.equal(copy.textContent,'已复制');
    assert.equal(p.node('body').children.length,0); // fallback field is removed
    p.context.document.execCommand=()=>false;await copy.onclick();assert.match(p.node('notice').textContent,/复制失败/);
  }
  const p=page(function(){});p.welcome();
  p.sockets[0].receive({type:'peers',peers:[{id:1,name:'本机'},{id:2,name:'手机'},{id:3,name:'电脑'}]});
  const app=p.context.PassportTransfer,sent=[];
  for(const id of [2,3])app.links.set(id,{id,ready:Promise.resolve(),pc:{close(){}},dc:{readyState:'open',send:data=>sent.push({peer:id,...JSON.parse(data)}),close(){}}});
  p.node('text').value='一次广播';await p.node('send-text').onclick();p.advance(100);
  assert.equal(app.texts.length,1);assert.equal(p.node('texts').children.length,1);
  assert.equal(sent.filter(m=>m.type==='text').length,2);assert.equal(sent[0].id,sent[1].id);
  assert.match(p.node('texts').textContent,/发送给 手机、电脑，等待手机、电脑接收/);
  p.advance(20000);assert.match(p.node('texts').textContent,/手机、电脑未接收/);
  // A failed target cannot discard the successful target's record or duplicate it.
  app.links.get(3).ready=Promise.reject(Error('电脑已离线'));
  p.node('text').value='部分发送';await p.node('send-text').onclick();p.advance(100);
  assert.equal(app.texts.length,2);assert.equal(app.texts[0].recipients[1].state,'failed');
  assert.match(p.node('texts').children[0].textContent,/等待手机接收；电脑未接收/);

  const q=page(function(){});q.welcome();const target=q.context.PassportTransfer,frames=[];
  target.links.set(2,{id:2,ready:Promise.resolve(),pc:{close(){}},dc:{readyState:'open',send:data=>frames.push(JSON.parse(data)),close(){}}});
  const file={id:'large',name:'large.bin',mime:'application/octet-stream',size:1024*1024**2};
  await target.receive(2,file);const transfer=target.history[0];transfer.offset=file.size;transfer.status='接收中';q.advance(100);
  assert.equal(q.find(q.node('catalog'),e=>e.tag==='progress').value,99);
  assert(q.find(q.node('catalog'),e=>e.tag==='button'&&e.textContent==='取消'));
  assert(!q.find(q.node('catalog'),e=>e.tag==='a')); // byte count alone is not validated completion
  transfer.sha256='a'.repeat(64);transfer.url=URL.createObjectURL(new Blob([]));transfer.status='已校验，等待保存';
  q.sockets[0].receive({type:'peers',peers:[{id:1,name:'本机'},{id:2,name:'电脑'}]});q.advance(100);
  const row=q.node('catalog').children[0],actions=row.children[0].children[1];
  assert.equal(actions.children.length,1);assert.equal(actions.children[0].tag,'a');assert.equal(actions.textContent,'保存文件');
  assert.equal(q.find(row,e=>e.tag==='progress').value,100);
  actions.children[0].onclick();assert(frames.some(m=>m.type==='complete'&&m.saved==='download'));
  q.advance(1000);assert.equal(transfer.reserved,0);assert.equal(transfer.url,null);
  await target.receive(2,file); // saving releases the full budget without a deleted Clear control
  q.advance(100);q.find(q.node('catalog'),e=>e.tag==='button'&&e.textContent==='取消').onclick();
}
async function canceledStream(){
  const channel={readyState:'open',bufferedAmount:0,send(){},close(){}};
  class RTC{
    createDataChannel(){return channel;}
    async createOffer(){return {type:'offer'};}
    async setLocalDescription(value){this.localDescription={toJSON:()=>value};}
    close(){}
  }
  const p=page(RTC);p.welcome();const app=p.context.PassportTransfer;
  p.node('text').value='建立连接';const sending=p.node('send-text').onclick();channel.onopen();await sending;
  const incoming=async data=>{channel.onmessage({data});await new Promise(setImmediate);};
  const file={id:'retry',name:'retry.bin',mime:'application/octet-stream',size:3};
  await app.receive(2,file);let t=app.history[0];
  await incoming(JSON.stringify({type:'begin',id:t.id,file,chunk:1024}));
  await incoming(new Uint8Array([1,2]).buffer);p.advance(100);
  const canceledId=t.id;
  p.find(p.node('catalog'),e=>e.tag==='button'&&e.textContent==='取消').onclick();
  await incoming(new Uint8Array([3]).buffer);assert.equal(p.node('notice').dataset.tone,'connected');
  await app.receive(2,file);t=app.history[0];
  await incoming(JSON.stringify({type:'begin',id:canceledId,file,chunk:1024}));
  await incoming(JSON.stringify({type:'finish',id:canceledId,size:3,sha256:'a'.repeat(64)}));
  assert.equal(t.canceled,undefined);assert.equal(t.chunk,undefined);
  await incoming(new Uint8Array([3]).buffer);assert.equal(t.offset,0);assert.equal(t.canceled,undefined);
  await incoming(JSON.stringify({type:'begin',id:t.id,file,chunk:1024}));
  const bytes=new Uint8Array([4,5,6]);await incoming(bytes.buffer);
  const sha256=require('node:crypto').createHash('sha256').update(bytes).digest('hex');
  await incoming(JSON.stringify({type:'finish',id:t.id,size:3,sha256}));
  assert.equal(t.sha256,sha256);assert(t.url);assert.equal(p.node('notice').dataset.tone,'connected');
  // Unrequested bytes after a completed stream still fail validation.
  await incoming(new Uint8Array([7]).buffer);assert.match(p.node('notice').textContent,/无效文件分块/);assert(t.canceled);
}
async function main(){
  await uiRegressions();
  await canceledStream();
  for(const navigator of [{userAgent:'Android'},{userAgent:'iPhone'},{userAgent:'iPad'},{platform:'MacIntel',maxTouchPoints:5}])
    await receiveBudget(navigator,1024*1024**2);
  await receiveBudget({},1024*1024**2);
  const blocked=page(undefined);blocked.welcome();
  assert(blocked.node('send-text').disabled&&blocked.node('files').disabled);
  assert.match(blocked.node('notice').textContent,/WebRTC/);
  assert.equal(blocked.sockets[0].sent.find(m=>m.type==='hello').rtc,false);
  blocked.advance(60000);assert.equal(blocked.sockets.length,1);assert.equal(blocked.sockets[0].readyState,1);
  assert(blocked.sockets[0].sent.some(m=>m.type==='ping'));
  // An extension can replace the constructor with a nonconstructible function.
  const broken=page(()=>{});broken.welcome();broken.node('text').value='中文';
  await broken.node('send-text').onclick();broken.advance(100);
  assert.match(broken.node('notice').textContent,/WebRTC/);assert(broken.node('send-text').disabled);
  assert.equal(broken.sockets[0].readyState,1);
  const active=page(function(){});active.welcome();
  active.sockets[0].receive({type:'pong'});active.advance(60000);
  assert.equal(active.sockets.length,1); // A one-minute background pause is valid.
  active.events.visibilitychange();assert.equal(active.sockets[0].sent.at(-1).type,'ping');
  active.sockets[0].close();active.advance(1000);assert.equal(active.sockets.length,2);
  assert.match(active.node('notice').textContent,/正在连接/);
  active.welcome();active.sockets[0].onclose({code:1006});
  assert.equal(active.node('connection').textContent,'局域网已连接');
  assert.equal(active.node('notice').dataset.tone,'connected');
  assert.match(active.node('notice').textContent,/局域网已连接/);
  assert.equal(active.context.PassportTransfer.peers.size,2); // Stale callbacks cannot erase a new session.
  active.sockets[1].receive({type:'end'});active.advance(30000);assert.equal(active.sockets.length,2);
  assert.equal(active.node('connection').textContent,'流转已关闭');
  const failing=page(function(){});failing.advance(120000);
  assert.equal(failing.sockets.length,6);assert(failing.sockets.every(s=>s.readyState===3));
  failing.node('reconnect').onclick();assert.equal(failing.sockets.length,7);
  failing.events.pagehide();failing.advance(60000);assert.equal(failing.sockets.length,7);
  const silent=page(function(){},false);silent.advance(120000);
  assert.equal(silent.sockets.length,6); // missing onclose cannot strand connecting forever
  silent.sockets[0].receive({type:'welcome',id:99});silent.advance(100);
  assert.equal(silent.context.PassportTransfer.peers.size,0); // even a late welcome is retired
  const congested=page(function(){},false);congested.welcome();
  congested.sockets[0].bufferedAmount=20000;congested.advance(11000);
  assert.equal(congested.sockets.length,2); // throwing ping cannot bypass liveness checks
  console.log('Transfer sessions: mobile/desktop aggregate receive budgets, pending reservations, cancellation, RTC capability, heartbeat, bounded reconnect, stale callbacks and explicit shutdown: PASS');
}
main().catch(e=>{console.error(e);process.exit(1);});
