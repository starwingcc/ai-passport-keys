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
  let device=K.defaults(),posts=0,failSave=false,connection='hotspot',deviceSystem=0;
  let job={id:0,kind:0,state:0,busy:false,connected:false,ssid:'',networks:[]},scanPosts=0,testPosts=0,testBody,testState=6,pollFailures=0,lostTestResponse=false,testLinkFailures=0,testRestoreFailure=false,restorePolls=0,testRestorePolls=0;
  let scanNetworks=[{ssid:'nearby-test-network',rssi:-35,secured:true},{ssid:'open-test-network',rssi:-55,secured:false},{ssid:'<b>literal-name</b>',rssi:-65,secured:true}];
  let wifi={ssid:"",has_password:false,state:5,clock_valid:false},wifiPassword="",wifiPosts=0,wifiBody,failWifi=false,settingsPosts=0;
  let home=K.homeDefaults(),homePosts=0,failHome=false;
  const savePaths=[],separateWrites=[];let exitPosts=0;
  K.addProfile(device,'浏览',0);const original=clone(device);
  await page.route('http://passport.test/**',async route=>{
    const request=route.request(),url=new URL(request.url());
    if(url.pathname==='/api/exit'){++exitPosts;return route.fulfill({json:{ok:true}});}
    if(url.pathname==='/api/presence'){
      assert.equal(request.headers()['x-session'],'test-session');
      return route.fulfill({json:{id:request.postDataJSON().id||1}});
    }
    if(request.method()==='POST'&&url.pathname==='/api/settings')savePaths.push(url.pathname);
    if(request.method()==='POST'&&['/api/config','/api/home','/api/wifi','/api/time'].includes(url.pathname)){
      separateWrites.push(url.pathname);
      return route.fulfill({status:405,body:'The web editor must save the complete settings record.'});
    }
    if(url.pathname==='/api/settings'){
      assert.equal(request.headers()['x-session'],'test-session');
      if(request.method()==='GET')return route.fulfill({json:{version:1,home,wifi:{version:1,ssid:wifi.ssid,password:wifiPassword},config:device}});
      const settings=request.postDataJSON();K.validateSettings(settings);++settingsPosts;++wifiPosts;wifiBody=settings.wifi;
      if(JSON.stringify(home)!==JSON.stringify(settings.home))++homePosts;
      if(JSON.stringify(device)!==JSON.stringify(settings.config))++posts;
      if(failHome){failHome=false;return route.fulfill({status:500,body:'工牌保存失败'});}
      if(failWifi){failWifi=false;return route.fulfill({status:500,body:'Wi-Fi 保存失败'});}
      if(failSave){failSave=false;return route.fulfill({status:500,body:'保存失败，仍使用原配置'});}
      home=settings.home;device=settings.config;wifiPassword=settings.wifi.password;
      wifi={...wifi,ssid:settings.wifi.ssid,has_password:!!wifiPassword};
      return route.fulfill({json:{ok:true}});
    }
    if(url.pathname==='/api/wifi/job'){
      assert.equal(request.headers()['x-session'],'test-session');
      if(pollFailures){--pollFailures;return route.fulfill({status:503,body:'temporary link interruption'});}
      if(restorePolls){--restorePolls;return route.fulfill({json:{...job,state:13,busy:true}});}
      return route.fulfill({json:job});
    }
    if(url.pathname==='/api/wifi/scan'){
      assert.equal(request.headers()['x-session'],'test-session');++scanPosts;
      job={id:job.id+1,kind:1,state:5,busy:false,connected:false,ssid:'',networks:scanNetworks};return route.fulfill({json:{id:job.id}});
    }
    if(url.pathname==='/api/wifi/test'){
      assert.equal(request.headers()['x-session'],'test-session');++testPosts;testBody=request.postDataJSON();
      job={id:job.id+1,kind:2,state:testState,busy:false,connected:testState===6,ssid:testBody.ssid,networks:[],restore_failed:testRestoreFailure};
      if(testLinkFailures)pollFailures=testLinkFailures;
      restorePolls=testRestorePolls;
      if(lostTestResponse){lostTestResponse=false;return route.fulfill({status:503,body:'lost response'});}
      return route.fulfill({json:{id:job.id}});
    }
    if(url.pathname==='/api/wifi'){
      assert.equal(request.headers()['x-session'],'test-session');
      return route.fulfill({json:wifi});
    }
    if(url.pathname==='/api/config')return route.fulfill({json:{config:device,session:'test-session',connection,system:deviceSystem}});
    const files={'/':'index.html','/app.js':'app.js','/charset.js':'charset.js','/app.css':'app.css'};
    const file=files[url.pathname];if(!file)return route.fulfill({status:404});
    return route.fulfill({body:fs.readFileSync(path.join(__dirname,'../main/web',file)),contentType:file.endsWith('.js')?'text/javascript':file.endsWith('.css')?'text/css':'text/html',headers:{'Content-Security-Policy':"default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'"}});
  });
  await page.goto('http://passport.test/');await page.locator('#editor').waitFor({state:'visible'});
  await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  await page.waitForFunction(()=>!document.getElementById('wifi-fields').disabled);
  assert.equal(await page.locator('#save-home, #save-wifi').count(),0);
  assert((await page.locator('#save').textContent()).includes('保存全部设置'));
  assert.equal(await page.locator('#wifi-password').getAttribute('type'),'text');
  assert(await page.locator('#wifi-show-password').isChecked());
  await page.locator('#wifi-ssid').fill('test-network');await page.locator('#wifi-password').fill('test-pass');
  await page.locator('#toggle-password').click();assert.equal(await page.locator('#wifi-password').getAttribute('type'),'password');
  assert.equal(await page.locator('#wifi-password').inputValue(),'test-pass');
  await page.locator('#toggle-password').click();assert.equal(await page.locator('#wifi-password').getAttribute('type'),'text');
  failWifi=true;await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('Wi-Fi 保存失败'));
  assert.equal(wifi.ssid,'');assert.equal(await page.locator('#wifi-password').inputValue(),'test-pass');
  await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('wifi-status').textContent.includes('网络已保存'));
  assert.equal(wifi.ssid,'test-network');assert.equal(wifi.has_password,true);assert.equal(await page.locator('#wifi-password').inputValue(),'test-pass');
  assert.equal(await page.locator('#wifi-password').getAttribute('type'),'text');
  await page.locator('#toggle-password').click();await page.locator('#save').click();
  await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  assert.equal(await page.locator('#wifi-password').inputValue(),'test-pass');assert.equal(await page.locator('#wifi-password').getAttribute('type'),'password');
  assert.equal(wifiBody.password,'test-pass');assert.equal(wifiBody.version,1);
  await page.locator('#toggle-password').click();await page.locator('#wifi-password').fill('');
  await page.locator('#save').click();await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  assert.equal(wifiBody.password,'test-pass');
  await page.locator('#wifi-ssid').fill('中'.repeat(11));await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('32 字节'));
  assert.equal(wifiPosts,3);await page.locator('#wifi-ssid').fill('test-network');
  await page.reload();await page.waitForFunction(()=>!document.getElementById('wifi-fields').disabled);
  assert.equal(await page.locator('#wifi-ssid').inputValue(),'test-network');assert.equal(await page.locator('#wifi-password').inputValue(),'test-pass');
  await page.locator('#clear-wifi').click();
  assert.equal(wifi.ssid,'test-network');assert.equal(await page.locator('#wifi-ssid').inputValue(),'');
  await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('wifi-status').textContent.includes('网络已清除'));
  assert.equal(wifi.ssid,'');assert.equal(wifi.has_password,false);assert.equal(posts,0);assert.equal(await page.locator('#wifi-password').inputValue(),'');
  // Scan selection, malicious-looking SSIDs, connection results, no implicit
  // save, repeated tests, input retention, and polling after link interruption.
  await page.locator('#scan-wifi').click();await page.waitForFunction(()=>document.getElementById('wifi-scan-status').textContent.includes('找到 3'));
  assert.equal(await page.locator('#wifi-networks option').count(),4);assert.equal(await page.locator('#wifi-networks b').count(),0);
  assert((await page.locator('#wifi-networks').textContent()).includes('<b>literal-name</b>'));
  await page.locator('#scan-results button').filter({hasText:'nearby-test-network'}).click();assert.equal(await page.locator('#wifi-ssid').inputValue(),'nearby-test-network');
  await page.locator('#wifi-password').fill('test-pass');const writesBefore=wifiPosts;
  for(const [state,text] of [[7,'认证失败'],[8,'未找到网络'],[9,'连接超时'],[6,'连接成功']]){
    testState=state;await page.locator('#test-wifi').click();await page.waitForFunction(text=>document.getElementById('wifi-test-status').textContent.includes(text),text);
    await page.waitForFunction(()=>!document.getElementById('editor').disabled);
    assert.equal(wifiPosts,writesBefore);assert.equal(wifi.ssid,'');assert.equal(await page.locator('#wifi-password').inputValue(),'test-pass');
  }
  await page.locator('#wifi-password').fill('changed-pass');assert((await page.locator('#wifi-test-status').textContent()).includes('已修改'));
  // Recover an accepted test whose POST response did not reach the browser.
  lostTestResponse=true;await page.locator('#test-wifi').click();await page.waitForFunction(()=>document.getElementById('wifi-test-status').textContent.includes('连接成功'));
  assert.equal(testPosts,5);assert.equal(wifiPosts,writesBefore);
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
  await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('wifi-status').textContent.includes('网络已保存'));
  await page.locator('#wifi-password').fill('');await page.locator('#test-wifi').click();await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  assert.equal(testBody.keep_password,true);assert.equal(testBody.password,'');
  await page.locator('#clear-wifi').click();await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  assert.equal(await page.locator('#home-name').inputValue(),'USER');
  assert.equal(await page.locator('#home-avatar option').count(),4);
  await page.locator('#home-name').fill('星翼');await page.locator('#home-badge').fill('DEV / 01');
  for(const avatar of ['1','2','3','0'])await page.locator('#home-avatar').selectOption(avatar);
  await page.locator('#home-timezone').selectOption('-345');await page.locator('#home-hour12').selectOption('1');
  failHome=true;await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('工牌保存失败'));
  assert.equal(home.name,'USER');assert.equal(posts,0);assert.equal(await page.locator('#home-name').inputValue(),'星翼');
  await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('home-saved').textContent==='工牌已保存');
  assert.equal(home.name,'星翼');assert.equal(home.timezone_minutes,-345);assert.equal(home.hour12,1);assert.equal(homePosts,2);assert.equal(posts,0);
  assert.equal(await page.locator('#sync-time').count(),0);assert.deepEqual(separateWrites,[]);
  await page.reload();await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  assert.equal(await page.locator('#home-name').inputValue(),'星翼');assert.equal(await page.locator('#home-badge').inputValue(),'DEV / 01');
  await page.locator('#home-name').fill('😀');await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('不支持'));assert.equal(homePosts,2);
  await page.locator('#home-name').fill('星翼');await page.locator('#home-badge').fill('');await page.locator('#save').click();await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  assert.equal(home.badge,'');
  assert.equal(await page.locator('.event').count(),4);assert.equal(await page.locator('#actions').count(),1);
  await page.locator('.tab[data-key="2"]').click();await page.locator('.event[data-event="3"]').click();
  assert(await page.locator('#system-action').isVisible());assert(await page.locator('#actions').isHidden());
  assert((await page.locator('#system-action').textContent()).includes('500 ms'));
  await page.locator('.event[data-event="2"]').click();
  await page.locator('#profile').selectOption('1');
  const downloadPromise=page.waitForEvent('download');await page.locator('#export-toggle').click();await page.locator('#export-profile').click();
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
  assert.equal(await page.locator('#profile-name').inputValue(),'浏览1');assert.equal(await page.locator('#current-profile').textContent(),'当前：coding模式');
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
  await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('dirty').textContent==='全部设置已保存');
  assert.equal(posts,2);assert.equal(device.profiles.length,3);assert.equal(device.profiles[1].bindings[10].title,'测试三击');
  // Full import has an explicit replacement review, and remains staged until Save.
  const beforeFull=clone(device);await upload(K.exportFile({...K.settingsDefaults(),home:clone(home),wifi:{version:1,ssid:wifi.ssid,password:wifiPassword}},'configuration'));
  assert((await page.locator('#import-warning').textContent()).includes('替换'));await page.locator('#import-apply').click();
  assert.equal(await page.locator('#profile option').count(),1);assert.equal(posts,2);assert.deepEqual(device,beforeFull);
  // Manual selection retains every left/right modifier and all ordinary usages.
  await page.locator('.tab[data-key="0"]').click();await page.locator('.event[data-event="0"]').click();
  assert.equal(await page.locator('#steps .step').count(),0);await page.locator('#add-step').click();
  await page.locator('.manual').first().click();const manual=page.locator('#key-editor');
  assert.deepEqual(await manual.locator('.modifier-column').nth(0).locator('label').allTextContents(),['左 Ctrl','左 Shift','左 Option/Alt','左 Command']);
  assert.deepEqual(await manual.locator('.modifier-column').nth(1).locator('label').allTextContents(),['右 Ctrl','右 Shift','右 Option/Alt','右 Command']);
  assert.equal(await manual.locator('select option').count(),K.keys.length);
  await manual.locator('select').selectOption('41');
  await manual.getByLabel('左 Command',{exact:true}).check();await manual.getByLabel('右 Command',{exact:true}).check();
  await manual.getByLabel('左 Shift',{exact:true}).check();
  assert.equal(await manual.locator('.chord-preview').textContent(),'Shift+Command+RCommand+Esc');
  await page.locator('#manual-apply').click();
  assert.equal(await page.locator('.step-chord').first().textContent(),'Shift+Command+RCommand+Esc');
  await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('dirty').textContent==='全部设置已保存');
  assert(!('logical_modifiers' in device.profiles[0].bindings[0].steps[0]));assert.equal(device.profiles[0].bindings[0].steps[0].modifiers,0x8a);
  await page.reload();await page.waitForFunction(()=>!document.getElementById('editor').disabled);
  await page.locator('.manual').first().click();
  assert(await manual.getByLabel('左 Command',{exact:true}).isChecked());assert(await manual.getByLabel('右 Command',{exact:true}).isChecked());
  await manual.getByLabel('左 Command',{exact:true}).uncheck();await page.locator('#manual-cancel').click();
  assert.equal(await page.locator('.step-chord').first().textContent(),'Shift+Command+RCommand+Esc');
  await page.waitForFunction(()=>!document.getElementById('wifi-fields').disabled);
  // One click saves edits in all three modules. Invalid drafts prevent every write.
  await page.locator('#home-badge').fill('ALL');await page.locator('#wifi-ssid').fill('unified-network');
  await page.locator('#wifi-password').fill('test-pass');await page.locator('#profile-name').fill('统一保存');
  const pendingStatus=await page.locator('#dirty').textContent();
  for(const name of ['工牌','Wi-Fi','按键情景'])assert(pendingStatus.includes(name));
  const beforeInvalid=savePaths.length;
  await page.locator('#profile-name').fill('😀');await page.locator('#save').click();
  await page.waitForFunction(()=>document.getElementById('status').textContent.includes('不支持'));
  assert.equal(savePaths.length,beforeInvalid);assert.equal(home.badge,'');assert.equal(wifi.ssid,'');
  await page.locator('#profile-name').fill('统一保存');await page.locator('#save').click();
  await page.waitForFunction(()=>document.getElementById('status').textContent.includes('完整配置已保存'));
  const savedStatus=await page.locator('#status').textContent();
  assert(savedStatus.includes('工牌和按键情景已更新')&&savedStatus.includes('Wi-Fi 网络将在退出配置后切换'));
  assert.equal(exitPosts,0);assert.equal(page.url(),'http://passport.test/');
  assert.deepEqual(savePaths.slice(beforeInvalid),['/api/settings']);
  assert.equal(home.badge,'ALL');assert.equal(wifi.ssid,'unified-network');assert.equal(device.profiles[0].name,'统一保存');
  const beforeNoop=savePaths.length;await page.locator('#save').click();
  await page.waitForFunction(()=>document.getElementById('status').textContent==='没有未保存的修改。');
  assert.equal(savePaths.length,beforeNoop);
  // A complete export backs up badge, credentials and scenarios. Imports only stage them.
  const fullDownloadPromise=page.waitForEvent('download');await page.locator('#export-toggle').click();await page.locator('#export-all').click();
  const fullDownload=await fullDownloadPromise,fullText=fs.readFileSync(await fullDownload.path(),'utf8');
  const completeBackup=JSON.parse(fullText);
  assert.equal(completeBackup.version,7);assert.deepEqual(completeBackup.settings.config,device);
  assert.deepEqual(completeBackup.settings.home,home);assert.equal(completeBackup.settings.wifi.password,'test-pass');
  assert(!fullText.includes('test-session'));
  await page.locator('#home-badge').fill('DRAFT');await page.locator('#wifi-ssid').fill('draft-network');
  await page.locator('#wifi-password').fill('draft-pass');await page.locator('#profile-name').fill('草稿');
  const beforeImport=settingsPosts;await upload(fullText);
  assert((await page.locator('#import-warning').textContent()).includes('Wi-Fi 密码'));
  await page.locator('#import-apply').click();assert.equal(settingsPosts,beforeImport);
  assert.equal(await page.locator('#home-badge').inputValue(),'ALL');assert.equal(await page.locator('#wifi-ssid').inputValue(),'unified-network');
  assert.equal(await page.locator('#wifi-password').inputValue(),'test-pass');assert.equal(await page.locator('#profile-name').inputValue(),'统一保存');
  await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('完整配置已保存'));
  assert.equal(settingsPosts,beforeImport+1);
  // Importing an explicitly empty password clears it even when the SSID is unchanged.
  const openBackup=clone(completeBackup);openBackup.settings.wifi.password='';await upload(JSON.stringify(openBackup));
  await page.locator('#import-apply').click();await page.locator('#save').click();
  await page.waitForFunction(()=>document.getElementById('status').textContent.includes('完整配置已保存'));
  assert.equal(wifiPassword,'');assert.equal(wifi.has_password,false);
  await upload(fullText);await page.locator('#import-apply').click();await page.locator('#save').click();
  await page.waitForFunction(()=>document.getElementById('status').textContent.includes('完整配置已保存'));
  assert.equal(wifiPassword,'test-pass');
  // A failed complete save keeps every module unchanged and retries one complete request.
  await page.locator('#home-badge').fill('RETRY');await page.locator('#wifi-ssid').fill('retry-network');
  await page.locator('#profile-name').fill('重试保存');const beforePartial=savePaths.length;
  failWifi=true;await page.locator('#save').click();
  await page.waitForFunction(()=>document.getElementById('status').textContent.includes('Wi-Fi 保存失败'));
  assert.deepEqual(savePaths.slice(beforePartial),['/api/settings']);
  assert.equal(home.badge,'ALL');assert.equal(wifi.ssid,'unified-network');assert.equal(device.profiles[0].name,'统一保存');
  assert((await page.locator('#status').textContent()).includes('完整配置尚未确认保存'));
  assert((await page.locator('#dirty').textContent()).includes('工牌、Wi-Fi、按键情景'));
  assert.equal(await page.locator('#wifi-password').inputValue(),'test-pass');
  const beforeRetry=savePaths.length;await page.locator('#save').click();
  await page.waitForFunction(()=>document.getElementById('status').textContent.includes('完整配置已保存'));
  assert.deepEqual(savePaths.slice(beforeRetry),['/api/settings']);
  assert.equal(wifi.ssid,'retry-network');assert.equal(device.profiles[0].name,'重试保存');
  // Clearing credentials is a draft and survives a cancelled attempt to leave.
  const beforeClear=savePaths.length;await page.locator('#clear-wifi').click();
  assert.equal(savePaths.length,beforeClear);assert.equal(wifi.ssid,'retry-network');
  page.once('dialog',dialog=>dialog.dismiss());await page.locator('#exit').click();
  assert((await page.locator('#dirty').textContent()).includes('Wi-Fi'));
  await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('wifi-status').textContent.includes('网络已清除'));
  assert.deepEqual(savePaths.slice(beforeClear),['/api/settings']);assert.equal(wifi.ssid,'');assert.equal(wifi.has_password,false);
  await page.reload();await page.waitForFunction(()=>!document.getElementById('wifi-fields').disabled);
  assert.equal(await page.locator('#home-badge').inputValue(),'RETRY');assert.equal(await page.locator('#profile-name').inputValue(),'重试保存');
  assert.equal(await page.locator('#wifi-ssid').inputValue(),'');
  const beforeDefaults=settingsPosts;page.once('dialog',dialog=>dialog.accept());await page.locator('#defaults').click();
  assert.equal(settingsPosts,beforeDefaults);assert.equal(await page.locator('#home-name').inputValue(),'USER');
  assert.equal(await page.locator('#home-timezone').inputValue(),'480');assert.equal(await page.locator('#wifi-ssid').inputValue(),'');
  await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('完整配置已保存'));
  assert.deepEqual(home,K.homeDefaults());assert.deepEqual(device,K.defaults());
  // Editing, recording, media and step ordering still operate on the real schema.
  await page.locator('#new-profile').click();assert.equal(await page.locator('#steps .step').count(),0);
  await page.locator('#add-step').click();await page.locator('#add-step').click();
  assert.equal(await page.locator('#steps .step').count(),2);
  assert.equal(await page.getByLabel('第1步共用版本后等待毫秒',{exact:true}).inputValue(),'80');
  assert(await page.getByLabel('第2步共用版本后等待毫秒',{exact:true}).isDisabled());
  await page.getByLabel('第1步共用版本后等待毫秒',{exact:true}).fill('325');
  await page.locator('#steps .step').first().getByRole('button',{name:'录制',exact:true}).click();
  await page.keyboard.down('ControlRight');await page.keyboard.down('KeyC');await page.keyboard.up('KeyC');await page.keyboard.up('ControlRight');
  assert.equal(await page.locator('.step-chord').first().textContent(),'RControl+C');
  await page.locator('#steps .step').first().getByRole('button',{name:'录制',exact:true}).click();
  await page.locator('#steps .step').first().getByRole('button',{name:'取消录制',exact:true}).click();
  assert.equal(await page.locator('.step-chord').first().textContent(),'RControl+C');
  await page.getByLabel('第2步共用版本类型',{exact:true}).selectOption('2');
  assert.equal(await page.getByLabel('媒体动作',{exact:true}).locator('option').count(),6);
  await page.getByLabel('媒体动作',{exact:true}).selectOption('233');
  await page.getByLabel('上移第2步',{exact:true}).click();
  assert.equal(await page.getByLabel('第1步共用版本类型',{exact:true}).inputValue(),'2');
  assert.equal(await page.getByLabel('第1步共用版本后等待毫秒',{exact:true}).inputValue(),'80');
  await page.getByLabel('下移第1步',{exact:true}).click();
  await page.getByLabel('删除整个第2步',{exact:true}).click();
  assert.equal(await page.getByLabel('第1步共用版本后等待毫秒',{exact:true}).inputValue(),'0');
  assert(await page.getByLabel('第1步共用版本后等待毫秒',{exact:true}).isDisabled());
  await page.locator('.manual').first().click();
  for(let i=1;i<6;i++)await manual.getByRole('button',{name:'+ 添加普通按键',exact:true}).click();
  assert(await manual.getByRole('button',{name:'+ 添加普通按键',exact:true}).isDisabled());
  const duplicate=await manual.getByLabel('普通按键 1',{exact:true}).inputValue();
  await manual.getByLabel('普通按键 2',{exact:true}).selectOption(duplicate);
  assert((await page.locator('#manual-error').textContent()).includes('重复'));
  await page.locator('#manual-cancel').click();
  await page.locator('#action-title').fill('录制测试');await page.locator('#preview').click();
  assert.equal(await page.locator('.preview-title').textContent(),'录制测试');
  assert.equal(await page.locator('.preview-keys').textContent(),'RControl+C');await page.locator('#preview-close').click();
  await page.locator('#activate-profile').click();await page.locator('#copy-profile').click();
  assert.equal(await page.locator('#steps .step').count(),1);
  await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('dirty').textContent==='全部设置已保存');
  assert.equal(device.active_profile,1);assert.equal(device.profiles.length,3);
  assert.equal(device.profiles[1].bindings[0].steps[0].modifiers,16);assert.equal(device.profiles[1].bindings[0].title,'录制测试');
  assert.deepEqual(device.profiles[2].bindings,device.profiles[1].bindings);
  page.once('dialog',dialog=>dialog.accept());await page.locator('#delete-profile').click();
  await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('dirty').textContent==='全部设置已保存');
  assert.equal(device.profiles.length,2);
  // Reset only the test fixture before the visual checks.
  page.once('dialog',dialog=>dialog.accept());await page.locator('#defaults').click();await page.locator('#save').click();
  await page.waitForFunction(()=>document.getElementById('dirty').textContent==='全部设置已保存');
  assert(await page.locator('.more-settings').evaluate(el=>el.open));
  assert.equal(await page.locator('#home-timezone option').count(),105);
  for(const width of [1440,1024,850,768,621,390,360]){
    await page.setViewportSize({width,height:844});await page.mouse.move(0,0);await page.waitForTimeout(200);
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'overflow at '+width);
    const buttons=await page.locator('#scan-wifi,#test-wifi,#preview').evaluateAll(els=>els.map(el=>{const r=el.getBoundingClientRect(),s=getComputedStyle(el);return [r.width,r.height,s.backgroundColor,s.borderColor,s.color,s.fontSize];}));
    assert.deepEqual(buttons[0],buttons[1],'network button styles at '+width);assert.deepEqual(buttons[0],buttons[2],'preview button styles at '+width);
    const title=await page.locator('#action-title').boundingBox(),preview=await page.locator('#preview').boundingBox();
    assert(preview.x>=title.x+title.width-1,'preview next to title at '+width);
    await page.evaluate(()=>scrollTo({top:document.documentElement.scrollHeight,behavior:'instant'}));
    for(const id of ['save','exit','defaults']){const r=await page.locator('#'+id).boundingBox();assert(r.y>=0&&r.y+r.height<=844,'fixed '+id+' at '+width);}
    const label=await page.locator('.brand .title').boundingBox(),restore=await page.locator('#defaults').boundingBox();assert(restore.x>label.x+label.width);
    await page.evaluate(()=>scrollTo({top:0,behavior:'instant'}));
  }
  await page.setViewportSize({width:1440,height:1100});
  if(process.env.KP_BROWSER_OUTPUT){fs.mkdirSync(process.env.KP_BROWSER_OUTPUT,{recursive:true});await page.screenshot({path:path.join(process.env.KP_BROWSER_OUTPUT,'web-desktop.png'),fullPage:true});}
  await page.setViewportSize({width:390,height:844});await page.evaluate(()=>scrollTo({top:0,behavior:'instant'}));await page.waitForTimeout(200);
  assert.equal(await page.locator('.brand').count(),1);
  if(process.env.KP_BROWSER_OUTPUT)await page.screenshot({path:path.join(process.env.KP_BROWSER_OUTPUT,'web-mobile.png')});
  // LAN scanning/testing survives lost POST/GET responses and restoration,
  // preserves unsaved edits, and never submits the candidate twice.
  connection='wifi';await page.reload();await page.waitForFunction(()=>!document.getElementById('wifi-fields').disabled);
  assert(await page.locator('#scan-wifi').isEnabled());assert(await page.locator('#test-wifi').isEnabled());
  scanNetworks=[{ssid:'nearby-test-network',rssi:-35,secured:true}];
  const scansBefore=scanPosts,testsBefore=testPosts,savesBefore=settingsPosts;
  await page.locator('#scan-wifi').click();await page.waitForFunction(()=>!document.getElementById('wifi-fields').disabled);
  assert.equal(scanPosts,scansBefore+1);
  await page.locator('#scan-results button').filter({hasText:'nearby-test-network'}).click();
  await page.locator('#wifi-password').fill('candidate-pass');
  const homeBefore=await page.locator('#home-name').inputValue();
  testState=6;lostTestResponse=true;testLinkFailures=3;testRestorePolls=2;
  await page.locator('#test-wifi').click();
  await page.waitForFunction(()=>document.getElementById('wifi-test-status').textContent.includes('正在等待恢复原网络'));
  await page.waitForFunction(()=>document.getElementById('wifi-test-status').textContent.includes('正在恢复配置前'));
  await page.waitForFunction(()=>!document.getElementById('wifi-fields').disabled);
  assert.equal(testPosts,testsBefore+1);assert.equal(settingsPosts,savesBefore);
  assert.equal(await page.locator('#wifi-ssid').inputValue(),'nearby-test-network');
  assert.equal(await page.locator('#wifi-password').inputValue(),'candidate-pass');
  assert.equal(await page.locator('#home-name').inputValue(),homeBefore);
  assert((await page.locator('#wifi-test-status').textContent()).includes('连接成功'));
  assert(!(await page.locator('#wifi-test-status').textContent()).includes('校时'));
  testLinkFailures=0;testRestorePolls=0;testState=7;await page.locator('#test-wifi').click();
  await page.waitForFunction(()=>!document.getElementById('wifi-fields').disabled);
  assert((await page.locator('#wifi-test-status').textContent()).includes('认证失败'));
  testState=6;testRestoreFailure=true;await page.locator('#test-wifi').click();
  await page.waitForFunction(()=>!document.getElementById('wifi-fields').disabled);
  assert((await page.locator('#wifi-test-status').textContent()).includes('原网络恢复失败'));testRestoreFailure=false;
  assert((await page.locator('#wifi-status').textContent()).includes('当前通过 WiFi 配置'));
  await page.locator('#wifi-ssid').fill('updated-network');await page.locator('#wifi-password').fill('updated-pass');
  await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('dirty').textContent==='全部设置已保存');
  assert.equal(wifi.ssid,'updated-network');assert(await page.locator('#scan-wifi').isEnabled());assert(await page.locator('#test-wifi').isEnabled());
  connection='hotspot';await page.reload();await page.waitForFunction(()=>!document.getElementById('wifi-fields').disabled);
  assert(!(await page.locator('#scan-wifi').isDisabled()));assert(!(await page.locator('#test-wifi').isDisabled()));
  device=K.defaults();device.profiles[0].bindings[10]={title:'重做',steps:[
    {type:1,modifiers:10,keys:[29],consumer:0,system:2,delay_ms:80},
    {type:1,modifiers:1,keys:[28],consumer:0,system:1,delay_ms:0}
  ]};
  for(const width of [1120,320])for(const system of [0,1]){
    deviceSystem=system;await page.setViewportSize({width,height:900});await page.reload();
    await page.waitForFunction(()=>!document.getElementById('wifi-fields').disabled);
    await page.getByRole('tab',{name:'OK 确定键',exact:false}).click();await page.getByRole('tab',{name:'三击',exact:true}).click();
    await page.locator('#preview').click();
    assert.equal(await page.locator('#preview-heading').textContent(),'设备显示预览 · '+(system?'Windows':'Mac'));
    assert.equal(await page.locator('.preview-keys').textContent(),system?'Control+Y':'Shift+Command+Z');
    assert.equal(await page.locator('.preview-progress').textContent(),'');
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
    await page.locator('#preview-close').click();
  }
  assert.deepEqual(separateWrites,[]);assert.deepEqual(errors,[]);await browser.close();
  console.log('Browser unified save, failed-save retry, staged defaults/clearing, Wi-Fi scan/test, saved-password display, identity, gestures, manual/recorded keys, steps, import/export and fixed mobile layout: PASS');
}
main().catch(e=>{console.error(e);process.exit(1);});
