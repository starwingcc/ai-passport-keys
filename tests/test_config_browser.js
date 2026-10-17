/* Real browser manual sessions, using the production pages and a bounded LAN broker. */
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),http=require('node:http');
const {chromium}=require('playwright');
const {wsServer:WebSocketServer}=require(path.join(path.dirname(require.resolve('playwright-core')),'lib/utilsBundle.js'));
require('../main/web/charset.js');require('../main/web/app.js');
const K=globalThis.Keypad,root=path.join(__dirname,'../main/web');
const delay=ms=>new Promise(r=>setTimeout(r,ms));
async function main(){
  let browser,server,wss,base;
  const errors=[],events=[],pages=new Map(),sockets=new Map();
  let mode='off',generation=1,nextId=0,ready=true,offline=false,expirePing=false,failQuery=false,saveBusy=false,saveCount=0;
  let settings={version:1,home:K.homeDefaults(),wifi:{version:1,ssid:'saved',password:'test-pass'},config:K.defaults()};
  const send=(s,m)=>{if(s.readyState===1)s.send(JSON.stringify(m));};
  const publish=()=>{for(const s of sockets.values())send(s,{type:'peers',peers:[...sockets].filter(([,s])=>s.joined).map(([id,s])=>({id,name:s.name,rtc:true}))});};
  const change=target=>{
    mode=target;++generation;pages.clear();ready=false;
    for(const s of sockets.values()){send(s,{type:'end'});s.close();}
    if(target!=='off'){
      const epoch=generation;ready=false;failQuery=true;
      (async()=>{await delay(100);while(saveBusy)await delay(20);if(generation===epoch)ready=true;})();
    }
  };
  const leave=id=>{pages.delete(id);};
  const admit=target=>{
    if(mode!==target)return 0;
    if(pages.size===4)return 0;
    const id=++nextId;pages.set(id,{joined:false,generation});return id;
  };
  server=http.createServer(async(req,res)=>{
    try{
      const url=new URL(base+req.url),parts=[];for await(const part of req)parts.push(part);
      const body=Buffer.concat(parts).toString(),data=body?JSON.parse(body):{};
      const json=(value,status=200)=>{res.writeHead(status,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify(value));};
      const fail=(status=403)=>json({error:'Session ended'},status);
      const file=(name,headers={})=>{res.writeHead(200,{'Content-Type':name.endsWith('.js')?'text/javascript':name.endsWith('.css')?'text/css':'text/html',...headers});res.end(fs.readFileSync(path.join(root,name)));};
      if(url.pathname==='/api/session'){
        if(failQuery){failQuery=false;return req.socket.destroy();}
        return json({mode,generation,online:[...pages.values()].filter(p=>p.joined).length,joining:[...pages.values()].filter(p=>!p.joined).length,ready});
      }
      if(url.pathname==='/api/session/open'||url.pathname==='/api/session/resume'){
        assert.equal(req.headers.origin,base);events.push({action:url.pathname,mode:data.mode,generation:data.generation});
        if(!['config','transfer'].includes(data.mode))return fail(400);
        if(mode!==data.mode||(url.pathname.endsWith('/resume')&&data.generation!==generation))return fail(409);
        const id=admit(data.mode);if(!id)return fail(503);
        return json({generation,lease:id});
      }
      if(['/','/config','/transfer'].includes(url.pathname)){
        if(url.pathname==='/'){res.writeHead(302,{Location:mode==='config'?'/config':'/transfer'});return res.end();}
        const target=url.pathname==='/config'?'config':'transfer';
        if(mode!==target)return fail(409);
        return file(target==='config'?'index.html':'transfer.html');
      }
      if(url.pathname.startsWith('/api/')){
        if(mode!=='config')return fail();
        if(url.pathname==='/api/config')return json({session:'token-'+generation,connection:'wifi',system:0});
        // Late leave/ping requests from retired pages are rejected by firmware.
        if(req.headers['x-session']!=='token-'+generation)return fail(403);
        if(url.pathname==='/api/presence'){
          events.push(data);
          if(offline)return req.socket.destroy();
          if(data.action==='join'){
            if(data.generation!==generation||!pages.has(data.lease)||pages.get(data.lease).joined)return fail(409);
            pages.get(data.lease).joined=true;return json({id:data.lease});
          }
          if(data.action==='leave'){leave(data.id);return json({id:data.id});}
          if(expirePing){expirePing=false;pages.delete(data.id);return fail(409);}
          return pages.has(data.id)?json({id:data.id}):fail(409);
        }
        if(url.pathname==='/api/settings'){
          if(req.method==='POST'){saveBusy=true;++saveCount;await delay(500);settings=data;saveBusy=false;return json({ok:true});}
          return json(settings);
        }
        if(url.pathname==='/api/wifi')return json({ssid:'saved',has_password:true,state:2,clock_valid:false});
        if(url.pathname==='/api/wifi/job')return json({id:0,kind:0,state:0,busy:false,connected:false,ssid:'',networks:[]});
        if(url.pathname==='/api/exit'){change('off');return json({ok:true});}
        return fail(404);
      }
      const name=url.pathname.slice(1);
      if(['app.js','charset.js','app.css','transfer.js','transfer-protocol.js','transfer.css'].includes(name))return file(name);
      fail(404);
    }catch(e){errors.push(e.message);if(!res.headersSent)res.writeHead(500);res.end();}
  });
  wss=new WebSocketServer({server,path:'/transfer/ws',verifyClient:(_info,done)=>done(mode==='transfer'&&ready)});
  wss.on('connection',s=>{
    const socketEpoch=generation;s.id=++nextId;s.name='浏览器';sockets.set(s.id,s);send(s,{type:'welcome',id:s.id});
    s.on('message',raw=>{
      if(mode!=='transfer'||generation!==socketEpoch)return s.close();
      const m=JSON.parse(raw);
      if(m.type==='hello'){
        if(!s.joined){if(m.generation!==generation||!pages.has(m.lease)||pages.get(m.lease).joined)return s.close();s.lease=m.lease;pages.get(m.lease).joined=true;s.joined=true;}
        s.name=m.name;publish();for(const other of sockets.values())if(other!==s&&other.joined)send(s,{type:'files',from:other.id,files:other.files||[]});
      }else if(m.type==='ping')send(s,{type:'pong'});
      else if(m.type==='files'){s.files=m.files;for(const other of sockets.values())if(other!==s&&other.joined)send(other,{type:'files',from:s.id,files:m.files});}
      else if(m.type==='signal'){const target=sockets.get(m.to);if(target)send(target,{...m,from:s.id});}
    });
    s.on('close',()=>{sockets.delete(s.id);if(socketEpoch===generation&&s.joined)leave(s.lease);publish();});
  });
  try{
    await new Promise(r=>server.listen(0,'127.0.0.1',r));base='http://127.0.0.1:'+server.address().port;
    browser=await chromium.launch({headless:true,chromiumSandbox:true,executablePath:process.env.KP_BROWSER_EXECUTABLE});
    const context=await browser.newContext();context.setDefaultTimeout(12000);
    context.on('page',p=>{p.on('pageerror',e=>errors.push(e.message));p.on('dialog',d=>d.accept());});
    const denied=await context.request.post(base+'/api/session/open',{headers:{Origin:base},data:{mode:'config'}});
    assert.equal(denied.status(),409);assert.equal(mode,'off');
    change('config'); // Simulate the physical device menu.
    const cfg=await context.newPage();const documents=[];cfg.on('request',r=>{if(r.isNavigationRequest())documents.push(new URL(r.url()).pathname);});await cfg.clock.install();await cfg.goto(base+'/');
    await cfg.waitForFunction(()=>!!document.getElementById('editor')&&!document.getElementById('editor').disabled);
    assert.deepEqual(documents,['/','/config']);
    assert.equal([...pages.values()].filter(p=>p.joined).length,1);
    const heartbeat=async()=>{const r=cfg.waitForResponse(r=>r.url().endsWith('/api/presence'));await cfg.clock.fastForward(10000);await r;await delay(50);};
    await heartbeat();await cfg.locator('#home-name').fill('DRAFT');
    offline=true;await cfg.clock.fastForward(10000);await delay(100);offline=false;
    assert.equal(await cfg.locator('#home-name').inputValue(),'DRAFT');
    expirePing=true;await heartbeat();await heartbeat();
    assert.equal([...pages.values()].filter(p=>p.joined).length,1);
    const cfg2=await context.newPage();await cfg2.goto(base+'/config?generation=999&lease=999');await cfg2.waitForFunction(()=>!!document.getElementById('editor')&&!document.getElementById('editor').disabled);
    await cfg2.goto('about:blank');await cfg2.close();await delay(100);
    assert.equal([...pages.values()].filter(p=>p.joined).length,1);
    const rejected=await context.request.post(base+'/api/session/open',{headers:{Origin:base},data:{mode:'transfer'}});
    assert.equal(rejected.status(),409);assert.equal(mode,'config');
    change('transfer');
    const transfer=await context.newPage();await transfer.goto(base+'/transfer');await transfer.waitForFunction(()=>window.PassportTransfer?.peers.size===1);
    await heartbeat();await cfg.waitForFunction(()=>!document.getElementById('config-reconnect').hidden);
    assert.equal(await cfg.locator('#home-name').inputValue(),'DRAFT');assert.equal(saveCount,0);
    let count=events.length;await cfg.clock.fastForward(40000);assert.equal(events.length,count,'retired config sends no heartbeat or takeover');
    await cfg.locator('#config-reconnect').click();await cfg.waitForFunction(()=>document.getElementById('status').textContent.includes('重新连接失败'));
    assert.equal(mode,'transfer');assert.equal(await cfg.locator('#home-name').inputValue(),'DRAFT');
    change('config');
    await cfg.locator('#config-reconnect').click();await cfg.waitForFunction(()=>document.getElementById('config-reconnect').hidden);
    assert.equal(await cfg.locator('#home-name').inputValue(),'DRAFT');assert.equal(saveCount,0);
    await transfer.waitForFunction(()=>document.getElementById('connection').textContent==='流转已关闭');
    count=events.filter(e=>e.action==='/api/session/open').length;await transfer.waitForTimeout(1500);assert.equal(events.filter(e=>e.action==='/api/session/open').length,count);
    change('transfer');await transfer.locator('#reconnect').click();await transfer.waitForFunction(()=>window.PassportTransfer?.peers.size===1);
    const second=await context.newPage();await second.goto(base+'/transfer');await second.waitForFunction(()=>window.PassportTransfer?.peers.size===2);
    await second.reload();await second.waitForFunction(()=>window.PassportTransfer?.peers.size===2);
    await second.close();await transfer.close();await delay(100);assert.equal(mode,'transfer');assert.equal(pages.size,0);
    const fresh=await context.newPage();await fresh.goto(base+'/transfer?generation=999&lease=999');await fresh.waitForFunction(()=>window.PassportTransfer?.peers.size===1);await fresh.close();
    await heartbeat();change('config');await cfg.locator('#config-reconnect').click();await cfg.waitForFunction(()=>document.getElementById('config-reconnect').hidden);
    // Device-initiated handoff waits for the already accepted save.
    await cfg.locator('#home-name').fill('SAVED');await cfg.locator('#save').click();while(!saveBusy)await delay(10);
    change('transfer');const duringSave=await context.newPage();await duringSave.goto(base+'/transfer');await duringSave.waitForFunction(()=>window.PassportTransfer?.peers.size===1);
    assert.equal(settings.home.name,'SAVED');assert.equal(saveCount,1);await duringSave.close();assert.equal(mode,'transfer');
    await cfg.clock.fastForward(10000);await cfg.waitForFunction(()=>!document.getElementById('config-reconnect').hidden);
    change('config');await cfg.locator('#config-reconnect').click();await cfg.waitForFunction(()=>document.getElementById('config-reconnect').hidden);
    await cfg.locator('#exit').click();await cfg.waitForFunction(()=>document.getElementById('dirty').textContent==='会话已结束');assert.equal(mode,'off');
    // A page loading during a manual switch cannot take the function back.
    saveBusy=true;change('config');
    const superseded=await context.newPage();await superseded.clock.install();
    const admitted=superseded.waitForResponse(r=>r.url().endsWith('/api/session/open'));
    await superseded.goto(base+'/config');await admitted;change('transfer');saveBusy=false;
    await superseded.waitForFunction(()=>document.getElementById('status').textContent.includes('设备已结束'));
    count=events.length;await superseded.clock.fastForward(60000);assert.equal(events.length,count);
    const results=await Promise.all(['config','transfer','config'].map(mode=>context.request.post(base+'/api/session/open',{headers:{Origin:base},data:{mode}})));
    assert.deepEqual(results.map(r=>r.status()),[409,200,409]);assert.equal(mode,'transfer');
    // Closing the last configuration page preserves the manually enabled mode.
    change('config');const final=await context.newPage();await final.goto(base+'/config');await final.waitForFunction(()=>!!document.getElementById('editor')&&!document.getElementById('editor').disabled);
    await final.close();await delay(100);assert.equal(mode,'config');assert.equal(pages.size,0);
    // Initial failure retries inside the formal page without navigation.
    const retry=await context.newPage();let failOnce=true,documentsLoaded=0;retry.on('request',r=>{if(r.isNavigationRequest())++documentsLoaded;});
    await retry.route('**/api/config',route=>{if(failOnce){failOnce=false;return route.abort();}return route.continue();});
    await retry.goto(base+'/config');await retry.locator('#config-reconnect').waitFor({state:'visible'});
    await retry.locator('#config-reconnect').click();await retry.waitForFunction(()=>!document.getElementById('editor').disabled);assert.equal(documentsLoaded,1);
    await retry.setViewportSize({width:390,height:844});await retry.screenshot({path:'build/manual-cleanup-config-mobile.png',fullPage:true});
    await retry.setViewportSize({width:1440,height:1000});await retry.screenshot({path:'build/manual-cleanup-config-desktop.png',fullPage:true});await retry.close();
    assert.deepEqual(errors,[]);await context.close();
    console.log('Manual LAN browser: explicit device start, root routing, no web takeover, multiple pages, heartbeat, close/reopen, drafts, saves, reconnect and explicit exit: PASS');
  }finally{
    if(browser)await browser.close();for(const s of sockets.values())s.terminate();
    if(wss)await new Promise(r=>wss.close(r));if(server){server.closeAllConnections();await new Promise(r=>server.close(r));}
  }
}
main().catch(e=>{console.error(e);process.exitCode=1;});
