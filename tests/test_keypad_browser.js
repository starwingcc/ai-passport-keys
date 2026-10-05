/* Run with Playwright available via NODE_PATH; this is additional UI acceptance. */
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const {chromium}=require('playwright');
require('../main/web/charset.js');require('../main/web/app.js');
const K=globalThis.Keypad;
const clone=x=>JSON.parse(JSON.stringify(x));
async function main(){
  const browser=await chromium.launch({headless:true,chromiumSandbox:true,executablePath:process.env.KP_BROWSER_EXECUTABLE});
  const page=await browser.newPage({viewport:{width:1120,height:900}});
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  let device=K.defaults(),posts=0,failSave=false;
  let job={id:0,kind:0,state:0,busy:false,connected:false,ssid:'',networks:[]},scanPosts=0,testPosts=0,testBody,testState=6,pollFailures=0,lostTestResponse=false;
  let scanNetworks=[{ssid:'nearby-test-network',rssi:-35,secured:true},{ssid:'open-test-network',rssi:-55,secured:false},{ssid:'<b>literal-name</b>',rssi:-65,secured:true}];
  let wifi={ssid:"",has_password:false,state:5,clock_valid:false},wifiPosts=0,wifiBody,failWifi=false;
  let home=K.homeDefaults(),homePosts=0,timePosts=0,failHome=false;
  K.addProfile(device,'浏览',0);const original=clone(device);
  await page.route('http://passport.test/**',async route=>{
    const request=route.request(),url=new URL(request.url());
    if(url.pathname==='/api/wifi/job'){
      assert.equal(request.headers()['x-session'],'test-session');
      if(pollFailures){--pollFailures;return route.fulfill({status:503,body:'temporary link interruption'});}
      return route.fulfill({json:job});
    }
    if(url.pathname==='/api/wifi/scan'){
      assert.equal(request.headers()['x-session'],'test-session');++scanPosts;
      job={id:job.id+1,kind:1,state:5,busy:false,connected:false,ssid:'',networks:scanNetworks};return route.fulfill({json:{id:job.id}});
    }
    if(url.pathname==='/api/wifi/test'){
      assert.equal(request.headers()['x-session'],'test-session');++testPosts;testBody=request.postDataJSON();
      job={id:job.id+1,kind:2,state:testState,busy:false,connected:testState===6||testState===10,ssid:testBody.ssid,networks:[]};
      if(lostTestResponse){lostTestResponse=false;return route.fulfill({status:503,body:'lost response'});}
      return route.fulfill({json:{id:job.id}});
    }
    if(url.pathname==='/api/wifi'){
      assert.equal(request.headers()['x-session'],'test-session');
      if(request.method()==='GET')return route.fulfill({json:wifi});
      wifiPosts++;wifiBody=request.postDataJSON();
      if(failWifi){failWifi=false;return route.fulfill({status:500,body:'Wi-Fi 保存失败'});}
      wifi={...wifi,ssid:wifiBody.ssid,has_password:!!wifiBody.password||wifiBody.keep_password};
      return route.fulfill({json:{ok:true}});
    }
    if(url.pathname==='/api/home'){
      if(request.method()==='GET')return route.fulfill({json:home});
      homePosts++;assert.equal(request.headers()['x-session'],'test-session');
      if(failHome){failHome=false;return route.fulfill({status:500,body:'工牌保存失败'});}
      home=request.postDataJSON();K.validateHome(home);return route.fulfill({json:{ok:true}});
    }
    if(url.pathname==='/api/time'){
      timePosts++;assert.equal(request.headers()['x-session'],'test-session');
      assert(Math.abs(request.postDataJSON().epoch-Math.floor(Date.now()/1000))<10);
      return route.fulfill({json:{ok:true}});
    }
    if(url.pathname==='/api/config'){
      if(request.method()==='GET')return route.fulfill({json:{config:device,session:'test-session'}});
      ++posts;assert.equal(request.headers()['x-session'],'test-session');
      if(failSave){failSave=false;return route.fulfill({status:500,body:'保存失败，仍使用原配置'});}
      device=request.postDataJSON();K.validate(device);return route.fulfill({json:{ok:true}});
    }
    const files={'/':'index.html','/app.js':'app.js','/charset.js':'charset.js','/app.css':'app.css'};
    const file=files[url.pathname];if(!file)return route.fulfill({status:404});
    return route.fulfill({body:fs.readFileSync(path.join(__dirname,'../main/web',file)),contentType:file.endsWith('.js')?'text/javascript':file.endsWith('.css')?'text/css':'text/html',headers:{'Content-Security-Policy':"default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'"}});
  });
  await page.goto('http://passport.test/');await page.locator('#editor').waitFor({state:'visible'});
  await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  await page.waitForFunction(()=>!document.getElementById('wifi-fields').disabled);
  assert.equal(await page.locator('#wifi-password').getAttribute('type'),'text');
  assert(await page.locator('#wifi-show-password').isChecked());
  await page.locator('#wifi-ssid').fill('test-network');await page.locator('#wifi-password').fill('test-pass');
  await page.locator('#wifi-show-password').uncheck();assert.equal(await page.locator('#wifi-password').getAttribute('type'),'password');
  assert.equal(await page.locator('#wifi-password').inputValue(),'test-pass');
  await page.locator('#wifi-show-password').check();assert.equal(await page.locator('#wifi-password').getAttribute('type'),'text');
  failWifi=true;await page.locator('#save-wifi').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('Wi-Fi 保存失败'));
  assert.equal(wifi.ssid,'');assert.equal(await page.locator('#wifi-password').inputValue(),'test-pass');
  await page.locator('#save-wifi').click();await page.waitForFunction(()=>document.getElementById('wifi-status').textContent.includes('网络已保存'));
  assert.equal(wifi.ssid,'test-network');assert.equal(wifi.has_password,true);assert.equal(await page.locator('#wifi-password').inputValue(),'test-pass');
  assert.equal(await page.locator('#wifi-password').getAttribute('type'),'text');
  await page.locator('#wifi-show-password').uncheck();await page.locator('#save-wifi').click();
  await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  assert.equal(await page.locator('#wifi-password').inputValue(),'test-pass');assert.equal(await page.locator('#wifi-password').getAttribute('type'),'password');
  assert.equal(wifiBody.password,'test-pass');assert.equal(wifiBody.keep_password,false);
  await page.locator('#wifi-show-password').check();await page.locator('#wifi-password').fill('');
  await page.locator('#save-wifi').click();await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  assert.equal(wifiBody.keep_password,true);assert.equal(wifiBody.password,'');
  await page.locator('#wifi-ssid').fill('中'.repeat(11));await page.locator('#save-wifi').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('32 字节'));
  assert.equal(wifiPosts,4);await page.locator('#wifi-ssid').fill('test-network');
  await page.reload();await page.waitForFunction(()=>!document.getElementById('wifi-fields').disabled);
  assert.equal(await page.locator('#wifi-ssid').inputValue(),'test-network');assert.equal(await page.locator('#wifi-password').inputValue(),'');
  await page.locator('#clear-wifi').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('已清除网络'));
  assert.equal(wifi.ssid,'');assert.equal(wifi.has_password,false);assert.equal(posts,0);assert.equal(await page.locator('#wifi-password').inputValue(),'');
  // Scan selection, malicious-looking SSIDs, connection results, no implicit
  // save, repeated tests, input retention, and polling after link interruption.
  await page.locator('#scan-wifi').click();await page.waitForFunction(()=>document.getElementById('wifi-scan-status').textContent.includes('找到 3'));
  assert.equal(await page.locator('#wifi-networks option').count(),4);assert.equal(await page.locator('#wifi-networks b').count(),0);
  assert((await page.locator('#wifi-networks').textContent()).includes('<b>literal-name</b>'));
  await page.locator('#wifi-networks').selectOption('nearby-test-network');assert.equal(await page.locator('#wifi-ssid').inputValue(),'nearby-test-network');
  await page.locator('#wifi-password').fill('test-pass');const writesBefore=wifiPosts;
  for(const [state,text] of [[7,'认证失败'],[8,'未找到网络'],[9,'连接超时'],[10,'认证通过，但网络校时失败'],[6,'连接成功']]){
    testState=state;await page.locator('#test-wifi').click();await page.waitForFunction(text=>document.getElementById('wifi-test-status').textContent.includes(text),text);
    await page.waitForFunction(()=>!document.getElementById('editor').disabled);
    assert.equal(wifiPosts,writesBefore);assert.equal(wifi.ssid,'');assert.equal(await page.locator('#wifi-password').inputValue(),'test-pass');
  }
  await page.locator('#wifi-password').fill('changed-pass');assert((await page.locator('#wifi-test-status').textContent()).includes('已修改'));
  // Recover an accepted test whose POST response did not reach the browser.
  lostTestResponse=true;await page.locator('#test-wifi').click();await page.waitForFunction(()=>document.getElementById('wifi-test-status').textContent.includes('连接成功'));
  assert.equal(testPosts,6);assert.equal(wifiPosts,writesBefore);
  // A failed poll is retried without losing the entered password.
  testState=6;const oldReadRoute='http://passport.test/api/wifi/job';
  await page.route(oldReadRoute,async route=>{await page.unroute(oldReadRoute);return route.fulfill({json:job});});
  pollFailures=1;await page.locator('#test-wifi').click();await page.waitForFunction(()=>document.getElementById('wifi-test-status').textContent.includes('正在重试'));
  await page.waitForFunction(()=>!document.getElementById('editor').disabled);assert.equal(await page.locator('#wifi-password').inputValue(),'changed-pass');
  assert.equal(wifiPosts,writesBefore);
  scanNetworks=[];await page.locator('#scan-wifi').click();await page.waitForFunction(()=>document.getElementById('wifi-scan-status').textContent.includes('未发现'));
  assert(await page.locator('#wifi-networks').isDisabled());assert.equal(scanPosts,2);assert.equal(await page.locator('#wifi-password').inputValue(),'changed-pass');
  await page.locator('#wifi-ssid').fill('hidden-test-network');await page.locator('#test-wifi').click();await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  assert.equal(testBody.ssid,'hidden-test-network');assert.equal(testBody.password,'changed-pass');assert.equal(wifiPosts,writesBefore);
  // Retest the saved password without exposing it in metadata.
  await page.locator('#save-wifi').click();await page.waitForFunction(()=>document.getElementById('wifi-status').textContent.includes('网络已保存'));
  await page.locator('#wifi-password').fill('');await page.locator('#test-wifi').click();await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  assert.equal(testBody.keep_password,true);assert.equal(testBody.password,'');
  await page.locator('#clear-wifi').click();await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  assert.equal(await page.locator('#home-name').inputValue(),'USER');
  assert.equal(await page.locator('#home-avatar option').count(),4);
  await page.locator('#home-name').fill('星翼');await page.locator('#home-badge').fill('DEV / 01');
  for(const avatar of ['1','2','3','0'])await page.locator('#home-avatar').selectOption(avatar);
  await page.locator('#home-timezone').selectOption('-345');await page.locator('#home-hour12').selectOption('1');
  failHome=true;await page.locator('#save-home').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('工牌保存失败'));
  assert.equal(home.name,'USER');assert.equal(posts,0);assert.equal(await page.locator('#home-name').inputValue(),'星翼');
  await page.locator('#save-home').click();await page.waitForFunction(()=>document.getElementById('home-saved').textContent==='工牌已保存');
  assert.equal(home.name,'星翼');assert.equal(home.timezone_minutes,-345);assert.equal(home.hour12,1);assert.equal(homePosts,2);assert.equal(posts,0);
  await page.locator('#sync-time').click();await page.waitForFunction(()=>document.getElementById('clock-status').textContent.includes('已同步'));assert.equal(timePosts,1);
  await page.reload();await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  assert.equal(await page.locator('#home-name').inputValue(),'星翼');assert.equal(await page.locator('#home-badge').inputValue(),'DEV / 01');
  await page.locator('#home-name').fill('😀');await page.locator('#save-home').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('不支持'));assert.equal(homePosts,2);
  await page.locator('#home-name').fill('星翼');await page.locator('#home-badge').fill('');await page.locator('#save-home').click();await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  assert.equal(home.badge,'');
  assert.equal(await page.locator('#actions .action').count(),4);
  assert.equal(await page.locator('.triple h2').textContent(),'三击');
  await page.locator('.tab[data-key="2"]').click();assert.equal(await page.locator('.fixed h2').textContent(),'长按');
  await page.locator('#profile').selectOption('1');
  const downloadPromise=page.waitForEvent('download');await page.locator('#export-profile').click();
  const download=await downloadPromise;const exported=fs.readFileSync(await download.path(),'utf8');
  assert.equal(K.parseImport(exported).config.profiles[0].name,'浏览');assert(!exported.includes('test-session'));
  const upload=async text=>{
    await page.locator('#import-input').setInputFiles({name:'scenario.json',mimeType:'application/json',buffer:Buffer.from(text)});
    await page.locator('#import-review').waitFor({state:'visible'});
    await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  };
  await upload(exported);assert.equal(await page.locator('#import-name').inputValue(),'浏览1');assert.equal(posts,0);
  await page.locator('#import-cancel').click();assert.equal(await page.locator('#profile option').count(),2);
  await upload(exported);await page.locator('#import-apply').click();
  assert.equal(await page.locator('#profile option').count(),3);assert.equal(posts,0);assert.deepEqual(device,original);
  assert.equal(await page.locator('#profile-name').inputValue(),'浏览1');assert.equal(await page.locator('#current-profile').textContent(),'当前：默认');
  const replacement=JSON.parse(exported);replacement.profile.bindings[10]={title:'测试三击',steps:[{type:1,modifiers:8,keys:[6],consumer:0,delay_ms:0}]};
  await upload(JSON.stringify(replacement));await page.locator('#import-mode').selectOption('replace');
  assert(await page.locator('#import-name').isDisabled());await page.locator('#import-apply').click();
  assert.equal(await page.locator('#profile option').count(),3);assert.equal(await page.locator('.triple input[aria-label="三击动作标题"]').inputValue(),'测试三击');assert.equal(posts,0);
  // Invalid file leaves all staged mappings intact; no request is sent.
  await page.locator('#import-input').setInputFiles({name:'bad.json',mimeType:'application/json',buffer:Buffer.from('{"version":99}')});
  await page.waitForFunction(()=>document.getElementById('status').textContent.includes('版本'));
  assert.equal(await page.locator('.triple input[aria-label="三击动作标题"]').inputValue(),'测试三击');assert.equal(posts,0);
  failSave=true;await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('保存失败'));
  assert.equal(posts,1);assert.deepEqual(device,original);assert.equal(await page.locator('.triple input[aria-label="三击动作标题"]').inputValue(),'测试三击');
  await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('dirty').textContent==='已保存到设备');
  assert.equal(posts,2);assert.equal(device.profiles.length,3);assert.equal(device.profiles[1].bindings[10].title,'测试三击');
  // Full import has an explicit replacement review, and remains staged until Save.
  const beforeFull=clone(device);await upload(K.exportFile(K.defaults(),'configuration'));
  assert((await page.locator('#import-warning').textContent()).includes('替换'));await page.locator('#import-apply').click();
  assert.equal(await page.locator('#profile option').count(),1);assert.equal(posts,2);assert.deepEqual(device,beforeFull);
  if(process.env.KP_BROWSER_OUTPUT){fs.mkdirSync(process.env.KP_BROWSER_OUTPUT,{recursive:true});await page.screenshot({path:path.join(process.env.KP_BROWSER_OUTPUT,'web-desktop.png'),fullPage:true});}
  await page.setViewportSize({width:390,height:844});
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
  assert.equal(await page.locator('.brand').count(),1);
  if(process.env.KP_BROWSER_OUTPUT)await page.screenshot({path:path.join(process.env.KP_BROWSER_OUTPUT,'web-mobile.png')});
  assert.deepEqual(errors,[]);await browser.close();
  console.log('Browser Wi-Fi scan/test/status/retry/no implicit save, password visibility/retention, identity save/reload/failure, four avatars, time sync, four gesture cards, import/export and mobile layout: PASS');
}
main().catch(e=>{console.error(e);process.exit(1);});
