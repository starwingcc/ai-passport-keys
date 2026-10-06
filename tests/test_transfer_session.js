/* Deterministic browser-session regressions; no RTC/file content emulation. */
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
function page(rtc,closeEvent=true,navigator={}){
  let now=0,nextTimer=1;
  const timers=new Map(),sockets=[],events={},nodes=new Map();
  class Element{
    constructor(tag='div'){this.tag=tag;this.children=[];this.value='';this.textContent='';this.hidden=false;this.disabled=false;this.classList={toggle(){}};}
    append(...children){this.children.push(...children);}
    replaceChildren(...children){this.children=children;}
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
    document:{getElementById:node,createElement:tag=>new Element(tag),visibilityState:'visible'},
    navigator:{userAgent:'Chrome',platform:'MacIntel',maxTouchPoints:0,...navigator},
    location:{protocol:'http:',host:'192.168.1.11'},localStorage:{getItem(){return null;},setItem(){}},
    RTCPeerConnection:rtc,WebSocket:Socket,TextEncoder,Uint8Array,ArrayBuffer,Blob,URL,
    Date:class extends Date{static now(){return now;}},crypto:{getRandomValues:a=>a.fill(1)},
    setTimeout:(fn,ms)=>schedule(fn,ms),setInterval:(fn,ms)=>schedule(fn,ms,ms),
    clearTimeout:id=>timers.delete(id),clearInterval:id=>timers.delete(id),
    addEventListener:(name,fn)=>events[name]=fn
  });
  for(const name of ['transfer-protocol.js','transfer.js'])vm.runInContext(fs.readFileSync(`main/web/${name}`,'utf8'),context);
  const welcome=(socket=sockets.at(-1))=>{socket.receive({type:'welcome',id:1});socket.receive({type:'peers',peers:[{id:1,name:'本机',rtc:!!rtc},{id:2,name:'电脑',rtc:true}]});advance(100);};
  return {context,node,sockets,events,welcome,advance};
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
  p.node('transfers').children[0].children[0].children[1].onclick(); // cancel releases budget
  p.node('clear').onclick();assert.equal(app.history.length,0);
  await app.receive(2,file(limit*5/8));
  await app.receive(3,file(limit*3/8));
  await assert.rejects(app.receive(4,file(1)),/容量已满/); // total across senders
  p.advance(100);
  p.node('transfers').children[0].children[0].children[1].onclick();
  await app.receive(4,file(1));
  assert.equal(app.links.get(4).rx.file.size,1);
  assert.match(p.node('save-mode').textContent,new RegExp(`${limit/1024**2}\\.0 MB`));
}
async function main(){
  for(const navigator of [{userAgent:'Android'},{userAgent:'iPhone'},{userAgent:'iPad'},{platform:'MacIntel',maxTouchPoints:5}])
    await receiveBudget(navigator,64*1024**2);
  await receiveBudget({},128*1024**2);
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
  active.welcome();active.sockets[0].onclose({code:1006});
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
