/* Real-browser acceptance with bundled Playwright and an explicit Chromium path. */
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const http=require('node:http');
const {chromium}=require('playwright');
require('../main/web/charset.js');require('../main/web/app.js');
const K=globalThis.Keypad;
const root=path.join(__dirname,'../main/web');
const waiting=fs.readFileSync(path.join(root,'config-wait.html'),'utf8');
const script=waiting.split('<script>')[1].split('</script>')[0];
const hash=crypto.createHash('sha256').update(script).digest('base64');
const implementation=fs.readFileSync(path.join(__dirname,'../main/keyboard/keypad_transfer_web.c'),'utf8');
assert(implementation.includes("sha256-"+hash), 'bootstrap CSP must match its embedded script');

async function main(){
  const browser=await chromium.launch({headless:true,chromiumSandbox:true,executablePath:process.env.KP_BROWSER_EXECUTABLE});
  let server;
  try{
    const context=await browser.newContext();
    context.setDefaultTimeout(10000);
    const events=[],errors=[];
    let bootRequests=0,nextId=0,expirePing=false,offline=false,ended=false,exits=0;
    const online=new Set();
    context.on('page',p=>{p.on('pageerror',e=>errors.push(e.message));p.on('dialog',d=>d.accept());});
    const settings={version:1,home:K.homeDefaults(),wifi:{version:1,ssid:'saved',password:'test-pass'},config:K.defaults()};
    const serve=async route=>{
      const req=route.request(),url=new URL(req.url());
      if(url.pathname==='/config'){
        ++bootRequests;
        if(bootRequests===2)return route.abort('connectionrefused');
        if(bootRequests<=3)return route.fulfill({body:waiting,contentType:'text/html',headers:{'Content-Security-Policy':"default-src 'none'; script-src 'sha256-"+hash+"'; connect-src 'self'; frame-ancestors 'none'"}});
        return route.fulfill({body:fs.readFileSync(path.join(root,'index.html')),contentType:'text/html',headers:{'X-Passport-Config':'ready'}});
      }
      if(url.pathname==='/api/config')return route.fulfill({json:{session:'current-session',connection:'wifi',system:0}});
      if(url.pathname==='/api/presence'){
        assert.equal(req.headers()['x-session'],'current-session');
        const b=req.postDataJSON();events.push(b);
        if(ended)return route.fulfill({status:403,body:'Session ended'});
        if(offline)return route.abort('connectionrefused');
        if(b.action==='join'){const id=++nextId;online.add(id);return route.fulfill({json:{id}});}
        if(b.action==='leave'){online.delete(b.id);return route.fulfill({json:{id:b.id}});}
        assert.equal(b.action,'ping');
        if(expirePing){expirePing=false;online.delete(b.id);return route.fulfill({status:409});}
        return route.fulfill({json:{id:b.id}});
      }
      if(url.pathname==='/api/settings')return route.fulfill({json:settings});
      if(url.pathname==='/api/wifi')return route.fulfill({json:{ssid:'saved',has_password:true,state:2,clock_valid:false}});
      if(url.pathname==='/api/wifi/job')return route.fulfill({json:{id:0,kind:0,state:0,busy:false,connected:false,ssid:'',networks:[]}});
      if(url.pathname==='/api/exit'){++exits;return route.fulfill({json:{ok:true}});}
      const files={'/app.js':'app.js','/charset.js':'charset.js','/app.css':'app.css'};
      if(!files[url.pathname])return route.fulfill({status:404});
      return route.fulfill({body:fs.readFileSync(path.join(root,files[url.pathname])),contentType:url.pathname.endsWith('.js')?'text/javascript':'text/css',headers:{'Content-Security-Policy':"default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'"}});
    };
    // Serve real HTTP: intercepting an unloading page can cancel a mocked
    // keepalive request before Playwright can fulfill it.
    server=http.createServer(async(req,res)=>{
      try{
        const parts=[];for await(const part of req)parts.push(part);
        const body=Buffer.concat(parts).toString();
        await serve({
          request:()=>({url:()=>base+req.url,headers:()=>req.headers,postDataJSON:()=>JSON.parse(body)}),
          fulfill:({status=200,json,body:payload,contentType,headers={}})=>{
            if(json!==undefined){payload=JSON.stringify(json);contentType='application/json';}
            res.writeHead(status,{...(contentType?{'Content-Type':contentType}:{}),...headers});res.end(payload);
          },
          abort:()=>req.socket.destroy()
        });
      }catch(e){errors.push(e.message);res.writeHead(500);res.end();}
    });
    await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
    const base='http://127.0.0.1:'+server.address().port;
    const page=await context.newPage();
    await page.clock.install();
    await page.goto(base+'/config');
    await page.waitForFunction(()=>!document.getElementById('editor')?.disabled&&!!document.getElementById('editor'));
    assert(bootRequests>=5&&online.size===1,'bootstrap retries transport failure, then editor joins');
    async function heartbeat(){
      const response=page.waitForResponse(r=>r.url().endsWith('/api/presence'));
      await page.clock.fastForward(10000);await response;await page.waitForTimeout(30);
    }
    let pings=events.filter(e=>e.action==='ping').length;
    await heartbeat();
    assert(events.filter(e=>e.action==='ping').length>pings,'idle editor sends heartbeat');
    await page.locator('#home-name').fill('DRAFT');
    offline=true;const failed=page.waitForEvent('requestfailed',{predicate:r=>r.url().endsWith('/api/presence')});await page.clock.fastForward(10000);await failed;await page.waitForTimeout(30);
    await page.waitForFunction(()=>document.getElementById('home-name').value==='DRAFT');
    offline=false;expirePing=true;
    await heartbeat();await heartbeat();
    assert.equal(online.size,1);assert.equal(await page.locator('#home-name').inputValue(),'DRAFT');
    assert.equal(events.filter(e=>e.action==='join').length,2,'expired page lease rejoins without resetting draft');
    const second=await context.newPage();
    const joined=second.waitForResponse(r=>r.url().endsWith('/api/presence'));
    await second.goto(base+'/config');await (await joined).finished();
    await second.waitForFunction(()=>!document.getElementById('editor').disabled);
    await second.waitForTimeout(50);
    assert.equal(online.size,2);
    await second.goto('about:blank');await page.waitForTimeout(100);await second.close();assert.equal(online.size,1);
    ended=true;await page.clock.fastForward(10000);
    await page.waitForFunction(()=>document.getElementById('status').textContent.includes('配置会话已结束'));
    assert.equal(await page.locator('#home-name').inputValue(),'DRAFT');
    pings=events.length;await page.clock.fastForward(30000);assert.equal(events.length,pings,'expired session stops heartbeat');
    ended=false;await page.evaluate(()=>dispatchEvent(new PageTransitionEvent('pagehide')));
    await page.reload();await page.waitForFunction(()=>!document.getElementById('editor').disabled);
    await page.locator('#exit').click();
    await page.waitForFunction(()=>document.getElementById('dirty').textContent==='会话已结束');
    assert.equal(exits,1);assert((await page.locator('#status').textContent()).includes('恢复监听'));
    pings=events.length;await page.clock.fastForward(30000);assert.equal(events.length,pings,'explicit exit stops heartbeat');
    assert.deepEqual(errors,[]);
    await context.close();
    console.log('Configuration browser: hashed bootstrap CSP/retries, idle heartbeat, offline draft, lease rejoin, multiple pages/leave and both exit paths: PASS');
  }finally{await browser.close();if(server){server.closeAllConnections();await new Promise(resolve=>server.close(resolve));}}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
