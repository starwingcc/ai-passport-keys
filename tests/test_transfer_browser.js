/* Optional real-browser acceptance; NODE_PATH supplies Playwright, no npm install.
 * KP_RTC_RESOLVE_MDNS is a diagnostic for two tabs on this host ONLY. It resolves
 * candidates to this host's LAN IP, and must never count as normal LAN/phone acceptance.
 */
const assert=require('node:assert/strict'),http=require('node:http'),fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),os=require('node:os');
const {chromium}=require('playwright');
const {wsServer:WebSocketServer}=require(path.join(path.dirname(require.resolve('playwright-core')),'lib/utilsBundle.js'));
async function main(){
  const ip=process.env.KP_TRANSFER_TEST_IP||Object.values(os.networkInterfaces()).flat().find(x=>x.family==='IPv4'&&!x.internal)?.address||'127.0.0.1';
  const server=http.createServer((req,res)=>{const name=req.url==='/'||req.url==='/transfer'?'transfer.html':req.url.slice(1);if(!['transfer.html','transfer.css','transfer.js','transfer-protocol.js'].includes(name)){res.writeHead(404);return res.end();}res.setHeader('Content-Type',name.endsWith('.js')?'text/javascript':name.endsWith('.css')?'text/css':'text/html');res.end(fs.readFileSync(path.join(__dirname,'../main/web',name)));});
  const sessions=new Map();let id=0,rejectedSessions=0;
  const wss=new WebSocketServer({server,path:'/transfer/ws',maxPayload:4096,verifyClient:(_info,done)=>{const allowed=sessions.size<4;if(!allowed)++rejectedSessions;done(allowed,503,'Four browser sessions maximum');}});
  const send=(s,m)=>{if(s.readyState===1)s.send(JSON.stringify(m));};
  const publish=()=>{for(const s of sessions.values())send(s,{type:'peers',peers:[...sessions].map(([id,s])=>({id,name:s.name}))});};
  wss.on('connection',s=>{s.id=++id;s.name='浏览器';sessions.set(s.id,s);send(s,{type:'welcome',id:s.id});s.on('message',raw=>{assert(raw.length<=4096);const m=JSON.parse(raw);if(process.env.KP_RTC_DEBUG)console.log('signal',s.id,m.type,m.to,m.description?.type,m.candidate?.candidate);if(m.type==='hello'){s.name=m.name;publish();for(const other of sessions.values())if(other!==s)send(s,{type:'files',from:other.id,files:other.files||[]});}else if(m.type==='ping')send(s,{type:'pong'});else if(m.type==='signal'){if(process.env.KP_RTC_RESOLVE_MDNS&&m.candidate)m.candidate.candidate=m.candidate.candidate.replace(/[a-f0-9-]+\.local/g,ip);send(sessions.get(m.to),{...m,from:s.id});}else if(m.type==='files'){s.files=m.files;for(const other of sessions.values())if(other!==s)send(other,{type:'files',from:s.id,files:m.files});}else throw Error('Content entered signaling server');});s.on('close',()=>{sessions.delete(s.id);publish();});});
  await new Promise(r=>server.listen(0,'0.0.0.0',r));const url=`http://${ip}:${server.address().port}/transfer`;
  let browser,output;const errors=[];
  try{
    browser=await chromium.launch({headless:true,chromiumSandbox:true,executablePath:process.env.KP_BROWSER_EXECUTABLE,args:[]});
    const context=await browser.newContext({acceptDownloads:true});const a=await context.newPage(),b=await context.newPage();if(process.env.KP_RTC_DEBUG)for(const [i,page] of [a,b].entries()){page.on('console',msg=>console.log('browser',i,msg.text()));await page.addInitScript(()=>{const Original=RTCPeerConnection;window.RTCPeerConnection=class extends Original{constructor(...args){super(...args);for(const type of ['connectionstatechange','iceconnectionstatechange','icecandidate','signalingstatechange'])this.addEventListener(type,e=>console.log(type,this.connectionState,this.iceConnectionState,this.signalingState,e.candidate?.candidate));}};});}for(const page of [a,b])page.on('pageerror',e=>errors.push(e.message));
    await a.goto(url.replace(/\/transfer$/,'/'));await b.goto(url);for(const page of [a,b])await page.waitForFunction(()=>PassportTransfer.peers.size===2);
    await a.locator('#name').fill('我的电脑');await a.locator('#rename').click();
    await b.locator('#name').fill('我的手机');await b.locator('#rename').click();
    await a.waitForFunction(()=>[...PassportTransfer.peers.values()].includes('我的手机'));
    const capacityPages=[await context.newPage(),await context.newPage()];
    for(const page of capacityPages){page.on('pageerror',e=>errors.push(e.message));await page.goto(url);}
    for(const page of [a,b,...capacityPages])await page.waitForFunction(()=>PassportTransfer.peers.size===4);
    const fifth=await context.newPage();fifth.on('pageerror',e=>errors.push(e.message));await fifth.goto(url);
    await fifth.waitForFunction(()=>document.getElementById('notice').textContent.includes('网页会话断开'));
    assert(rejectedSessions>0);assert.equal(sessions.size,4);assert.equal(await fifth.evaluate(()=>PassportTransfer.self),0);
    await capacityPages[0].close();await a.waitForFunction(()=>PassportTransfer.peers.size===3);
    await fifth.locator('#reconnect').click();await fifth.waitForFunction(()=>PassportTransfer.peers.size===4);
    await fifth.close();await capacityPages[1].close();
    for(const page of [a,b])await page.waitForFunction(()=>PassportTransfer.peers.size===2);
    const secure=await a.evaluate(()=>isSecureContext);assert.equal(secure,ip==='127.0.0.1');
    const composer=a.locator('#text');await composer.fill('中文第一行');
    await composer.press('Shift+Enter');await composer.pressSequentially('emoji 😀');
    await composer.press('Shift+Enter');await composer.pressSequentially('<script>纯文本</script>');
    assert.equal(await composer.inputValue(),'中文第一行\nemoji 😀\n<script>纯文本</script>');
    await composer.dispatchEvent('keydown',{key:'Enter',code:'Enter',keyCode:229,isComposing:true});
    assert.equal(await a.evaluate(()=>PassportTransfer.texts.length),0);
    await composer.press('Enter');
    await b.waitForFunction(()=>PassportTransfer.texts.some(t=>t.text.includes('😀'))).catch(async e=>{console.error('A',await a.evaluate(()=>({notice:document.getElementById('notice').textContent,links:[...PassportTransfer.links.values()].map(l=>({state:l.pc.connectionState,ice:l.pc.iceConnectionState,signal:l.pc.signalingState}))})));console.error('B',await b.evaluate(()=>({notice:document.getElementById('notice').textContent,links:[...PassportTransfer.links.values()].map(l=>({state:l.pc.connectionState,ice:l.pc.iceConnectionState,signal:l.pc.signalingState}))})));throw e;});await a.waitForFunction(()=>PassportTransfer.texts.some(t=>t.status==='接收端已确认'));
    assert.equal(await b.locator('.received-text script').count(),0);
    await b.locator('.received-text').getByRole('button',{name:'复制',exact:true}).click();
    await b.locator('.received-text').getByRole('button',{name:'已复制',exact:true}).waitFor({state:'visible'});
    output=fs.mkdtempSync(path.join(os.tmpdir(),'passport-rtc-'));const filePath=path.join(output,'100MiB.bin');
    const block=crypto.randomBytes(1024*1024);const fd=fs.openSync(filePath,'w');for(let i=0;i<100;i++)fs.writeSync(fd,block);fs.closeSync(fd);
    const hash=async p=>{const h=crypto.createHash('sha256');for await(const chunk of fs.createReadStream(p))h.update(chunk);return h.digest('hex');};
    await a.locator('#files').setInputFiles(filePath);await b.locator('#catalog button').first().click();
    await b.waitForFunction(()=>PassportTransfer.history[0]?.url,null,{timeout:240000});
    assert.equal(await b.evaluate(()=>PassportTransfer.history[0].offset),100*1024*1024);assert.equal(await b.evaluate(()=>PassportTransfer.history[0].sha256),await hash(filePath));
    const received=b.locator('#catalog .item').filter({hasText:'100MiB.bin'});
    assert.equal(await received.locator('.item-actions button').count(),0);
    const downloadPromise=b.waitForEvent('download');await received.getByRole('link',{name:'保存文件',exact:true}).click();const download=await downloadPromise;assert.equal(await hash(await download.path()),await hash(filePath));
    await a.waitForFunction(()=>PassportTransfer.history[0]?.done);assert((await a.locator('#catalog').textContent()).includes('接收端已校验'));
    // Cancel a second transfer, then prove both contexts can establish another connection.
    await b.waitForFunction(()=>PassportTransfer.history[0]?.reserved===0);await received.getByRole('button',{name:'接收',exact:true}).click();await b.waitForFunction(()=>PassportTransfer.history[0]?.offset>0);await received.getByRole('button',{name:'取消',exact:true}).click();
    await a.waitForFunction(()=>PassportTransfer.history[0]?.error);assert(await b.evaluate(()=>PassportTransfer.history[0].canceled));
    // Broadcast and specific-recipient text; targeted metadata is not offered to other receivers.
    const c=await context.newPage();c.on('pageerror',e=>errors.push(e.message));await c.goto(url);
    await c.locator('#name').fill('工作电脑');await c.locator('#rename').click();
    for(const page of [a,b,c])await page.waitForFunction(()=>PassportTransfer.peers.size===3);
    await a.locator('#text').fill('广播文字');await a.locator('#send-text').click();
    for(const page of [b,c])await page.waitForFunction(()=>PassportTransfer.texts.some(t=>t.text==='广播文字'));
    await a.waitForFunction(()=>PassportTransfer.texts.find(t=>t.text==='广播文字')?.status==='接收端已确认');
    await a.locator('.received-text').filter({hasText:'广播文字'}).waitFor({state:'visible'});
    assert.equal(await a.locator('.received-text').filter({hasText:'广播文字'}).count(),1);
    assert.equal(await a.evaluate(()=>PassportTransfer.texts.find(t=>t.text==='广播文字').recipients.length),2);
    const bid=await b.evaluate(()=>PassportTransfer.self);
    await a.locator('#target').selectOption(String(bid));await a.locator('#text').fill('仅指定设备');await a.locator('#send-text').click();
    await b.waitForFunction(()=>PassportTransfer.texts.some(t=>t.text==='仅指定设备'));assert(!await c.evaluate(()=>PassportTransfer.texts.some(t=>t.text==='仅指定设备')));
    // A zero-byte file still needs integrity verification and a save acknowledgment.
    await a.locator('#files').setInputFiles({name:'空文件.txt',mimeType:'text/plain',buffer:Buffer.alloc(0)});
    await b.waitForFunction(()=>[...document.querySelectorAll('#catalog strong')].some(e=>e.textContent==='空文件.txt'));
    assert(!await c.locator('#catalog').textContent().then(t=>t.includes('空文件.txt')));
    await b.locator('#catalog .item').filter({hasText:'空文件.txt'}).getByRole('button',{name:'接收',exact:true}).click();
    await b.waitForFunction(()=>PassportTransfer.history[0]?.url&&PassportTransfer.history[0].file.size===0);
    assert.equal(await b.locator('#notice').getAttribute('data-tone'),'connected');
    if(process.env.KP_BROWSER_OUTPUT){fs.mkdirSync(process.env.KP_BROWSER_OUTPUT,{recursive:true});await a.setViewportSize({width:1440,height:1100});await a.screenshot({path:path.join(process.env.KP_BROWSER_OUTPUT,'transfer-desktop.png'),fullPage:true});await b.setViewportSize({width:390,height:844});await b.screenshot({path:path.join(process.env.KP_BROWSER_OUTPUT,'transfer-mobile.png'),fullPage:true});}
    const emptyDownloadPromise=b.waitForEvent('download');await b.locator('#catalog .item').filter({hasText:'空文件.txt'}).getByRole('link',{name:'保存文件',exact:true}).click();
    assert.equal(fs.statSync(await (await emptyDownloadPromise).path()).size,0);
    await a.waitForFunction(()=>PassportTransfer.history[0]?.done);await c.close();
    // A secure receiver retains the existing file-picker streaming path.
    const d=await context.newPage();d.on('pageerror',e=>errors.push(e.message));
    await d.addInitScript(()=>{window.savedParts=[];window.sinkClosed=false;window.showSaveFilePicker=async()=>({createWritable:async()=>({write:async part=>savedParts.push(part.slice()),close:async()=>{sinkClosed=true;},abort:async()=>{savedParts=[];}})});});
    await d.goto(`http://127.0.0.1:${server.address().port}/transfer`);
    for(const page of [a,b,d])await page.waitForFunction(()=>PassportTransfer.peers.size===3);
    await a.locator('#target').selectOption(String(await d.evaluate(()=>PassportTransfer.self)));
    const direct=Buffer.from('直接写文件 / direct file sink\n'.repeat(1024));
    await a.locator('#files').setInputFiles({name:'直接保存.txt',mimeType:'text/plain',buffer:direct});
    const directRow=d.locator('#catalog .item').filter({hasText:'直接保存.txt'});
    await directRow.getByRole('button',{name:'接收',exact:true}).click();
    await d.waitForFunction(()=>sinkClosed&&PassportTransfer.history[0]?.done);
    assert.equal(await d.evaluate(async()=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',await new Blob(savedParts).arrayBuffer())),v=>v.toString(16).padStart(2,'0')).join('')),crypto.createHash('sha256').update(direct).digest('hex'));
    assert.equal(await directRow.getByRole('link',{name:'保存文件',exact:true}).count(),0);
    await directRow.getByText(/已校验并保存/).waitFor({state:'visible'});
    await d.close();
    await a.reload();await b.waitForFunction(()=>PassportTransfer.peers.size===2);await b.waitForFunction(()=>!document.querySelector('#catalog button'));
    assert.equal(await b.evaluate(()=>PassportTransfer.links.size),0);
    // Shutdown must close a live channel, rather than merely an empty map.
    await a.locator('#text').fill('刷新后重新连接');await a.locator('#send-text').click();
    await b.waitForFunction(()=>PassportTransfer.texts.some(t=>t.text==='刷新后重新连接'));
    for(const page of [a,b])assert(await page.evaluate(()=>[...PassportTransfer.links.values()].some(l=>l.pc.connectionState==='connected'&&l.dc.readyState==='open')));
    for(const s of sessions.values())send(s,{type:'end'});for(const page of [a,b])await page.waitForFunction(()=>PassportTransfer.links.size===0&&document.getElementById('connection').textContent==='流转已关闭');
    await a.locator('#reconnect').click();await b.locator('#reconnect').click();
    for(const page of [a,b])await page.waitForFunction(()=>PassportTransfer.peers.size===2);
    await composer.fill('重新连接后回车发送');await composer.press('Enter');
    await b.waitForFunction(()=>PassportTransfer.texts.some(t=>t.text==='重新连接后回车发送'));
    assert.deepEqual(errors,[]);
    console.log(JSON.stringify({result:'PASS',browser:browser.version(),url,secureContext:secure,bytes:100*1024*1024,sha256:await hash(filePath),save:'browser download and file-picker sink',testResolvedMdns:!!process.env.KP_RTC_RESOLVE_MDNS,checks:['root and transfer pages','four-session capacity and released slot','Enter sends / Shift+Enter newline / IME confirmation','Chinese/newline/emoji','direct copy','100 MiB saved hash','inline cancel/save','save releases budget','cancel','single broadcast record','specified recipient','zero-byte saved file','file-picker sink hash','refresh removes source','shutdown closes RTC','page wake reconnect']}));
  }finally{
    try{await browser?.close();}
    finally{
      for(const s of sessions.values())s.terminate();
      await new Promise(r=>wss.close(r));await new Promise(r=>server.close(r));
      if(output)fs.rmSync(output,{recursive:true,force:true});
    }
  }
}
main().catch(e=>{console.error(e);process.exit(1);});
