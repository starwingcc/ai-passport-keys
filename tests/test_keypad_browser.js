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
  K.addProfile(device,'浏览',0);const original=clone(device);
  await page.route('http://passport.test/**',async route=>{
    const request=route.request(),url=new URL(request.url());
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
  if(process.env.KP_BROWSER_OUTPUT)await page.screenshot({path:path.join(process.env.KP_BROWSER_OUTPUT,'web-mobile.png'),fullPage:true});
  assert.deepEqual(errors,[]);await browser.close();
  console.log('Browser four gesture cards, downloads, import review, collisions, staged save/failure and mobile layout: PASS');
}
main().catch(e=>{console.error(e);process.exit(1);});
