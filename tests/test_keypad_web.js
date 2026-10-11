const assert=require('node:assert/strict');
require('../main/web/charset.js');require('../main/web/app.js');
const K=globalThis.Keypad;
const {capture,keys,validate}=K;
// The listener embeds its bootstrap script and pins its CSP hash. Changing
// either side must fail the ordinary host gate, before device/browser testing.
{
  const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
  const page=fs.readFileSync(path.join(__dirname,'../main/web/config-wait.html'),'utf8');
  const script=page.split('<script>')[1].split('</script>')[0];
  const hash=crypto.createHash('sha256').update(script).digest('base64');
  assert(fs.readFileSync(path.join(__dirname,'../main/keyboard/keypad_transfer_web.c'),'utf8').includes('sha256-'+hash));
}
// Editor/import tests use a stable fixture independent of first-use defaults.
function editorFixture(){
  const bindings=Array.from({length:11},()=>({title:'',steps:[]}));
  for(const [slot,modifiers,key] of [[0,0,41],[3,4,0],[4,0,42],[5,8,4],[7,0,81],[8,0,40]])
    bindings[slot].steps=[{type:1,modifiers,keys:key?[key]:[],consumer:0,system:0,delay_ms:0}];
  bindings[8].title='回车';return{version:6,active_profile:0,profiles:[{name:'Editor',bindings}]};
}
const initial=K.defaults();assert(validate(initial));
assert.equal(initial.profiles[0].name,'coding模式');
for(const system of [1,2]){
  const expected=['无',system===2?'Command+Z':'Control+Z',system===2?'Shift+Command+Z':'Control+Y','Option/Alt','Backspace','无','无',system===2?'Command+A → Backspace':'Control+A → Backspace','Enter','无','无'];
  initial.profiles[0].bindings.forEach((b,i)=>assert.equal(K.devicePreview(b,system).sequence,expected[i]));
}
assert.equal(initial.profiles[0].bindings[3].title,'可配置为语音输入');
assert.equal(initial.profiles[0].bindings[7].title,'全选并删除');
const c=editorFixture();assert.equal(c.profiles[0].bindings.length,11);assert.equal(c.version,6);assert(validate(c));
assert.equal(K.capacity(c),480);assert(!('description' in c.profiles[0].bindings[0]));
assert.deepEqual(c.profiles[0].bindings[0].steps[0].keys,[41]);
assert.equal(c.profiles[0].bindings[3].steps[0].modifiers,4);
assert.deepEqual(c.profiles[0].bindings[4].steps[0].keys,[42]);
assert.equal(K.sequenceLabel(c.profiles[0].bindings[5]),'Command+A');
assert.equal(K.titleLabel(c.profiles[0].bindings[8]),'回车');
assert.deepEqual(capture([{code:'KeyC',key:'c',metaKey:true}]),{type:1,modifiers:8,keys:[6],consumer:0});
assert.equal(capture([{code:'MetaRight',key:'Meta',metaKey:true}]).modifiers,128);
assert.deepEqual(capture([{code:'ShiftLeft',key:'Shift',shiftKey:true},{code:'Digit1',key:'!',shiftKey:true}]).keys,[30]);
assert.deepEqual(capture([{code:'NumpadEnter',key:'Enter'}]).keys,[88]);
assert.deepEqual(capture([{code:'KeyA',key:'a'},{code:'KeyA',key:'a',repeat:true}]).keys,[4]);
for(const events of [[{code:'Fn',key:'Fn'}],[{code:'KeyA',key:'Process',isComposing:true}],Array.from('ABCDEFG').map(ch=>({code:'Key'+ch,key:ch})),[]])assert.throws(()=>capture(events));
assert.equal(new Set(keys.map(x=>x.usage)).size,keys.length);assert.equal(new Set(keys.map(x=>x.code)).size,keys.length);
const b=c.profiles[0].bindings[5];b.title='全选后复制';K.addStep(b,capture([{code:'KeyC',key:'c',metaKey:true}]));
assert.equal(K.sequenceLabel(b),'Command+A → Command+C');assert.equal(b.steps[0].delay_ms,80);assert.equal(b.steps[1].delay_ms,0);
K.moveStep(b,1,-1);assert.equal(K.sequenceLabel(b),'Command+C → Command+A');assert.equal(b.steps[0].delay_ms,80);assert.equal(b.steps[1].delay_ms,0);
K.removeStep(b,0);assert.equal(b.steps[0].delay_ms,0);assert(validate(c));
const newIndex=K.addProfile(c,'Coding',0);assert.equal(newIndex,1);assert.equal(c.active_profile,0);
c.profiles[1].bindings[5].title='复制';assert.equal(b.title,'全选后复制'); // deep copy
c.active_profile=1;K.removeProfile(c,0);assert.equal(c.active_profile,0);assert.equal(c.profiles[0].name,'Coding');assert(validate(c));
assert.throws(()=>K.removeProfile(c,0));assert.throws(()=>K.addProfile(c,'Coding'));
for(let i=1;i<8;i++)K.addProfile(c,'情景'+i);assert.throws(()=>K.addProfile(c,'第九套'));assert(validate(c));
assert.throws(()=>K.validateText('😀',16));assert.throws(()=>K.validateText('\ud800',16));assert.throws(()=>K.validateText('12345678901234567',16));
assert.throws(()=>K.validateText('a\nb',16));K.validateText('编程浏览，ABC',16);assert.throws(()=>K.validateText('a\nb',64));
assert.throws(()=>K.validateText('编程龘',16),/U\+9F98/);assert.throws(()=>K.validateText('靐',16));
const restored=editorFixture();restored.profiles[0].name='龘字';restored.profiles[0].bindings[0].title='靐字';
assert(validate(restored,true));assert.throws(()=>validate(restored),/U\+9F98/);
restored.profiles[0].name='Coding';assert.throws(()=>validate(restored),/U\+9750/);
restored.profiles[0].bindings[0].title='复制';assert(validate(restored));
for(const text of ['\ud800','a\nb','\u0085','12345678901234567']){restored.profiles[0].name=text;assert.throws(()=>validate(restored,true));}
const huge=editorFixture(),macro=huge.profiles[0].bindings[1];for(let i=0;i<64;i++)K.addStep(macro);assert.throws(()=>K.addStep(macro));assert(validate(huge));
const oldSchema=editorFixture();oldSchema.version=2;assert.throws(()=>validate(oldSchema));
const removed=editorFixture();removed.profiles[0].bindings[0].description='old';assert.throws(()=>validate(removed));
const invalid=editorFixture();invalid.profiles[0].bindings[0].steps[0].keys=[4,4];assert.throws(()=>validate(invalid));
invalid.profiles[0].bindings[0].steps[0].keys=[4];invalid.profiles[0].bindings[0].steps[0].delay_ms=1;assert.throws(()=>validate(invalid));
invalid.profiles[0].bindings[0].steps[0].delay_ms=0;invalid.profiles[0].bindings[0].title='😀';assert.throws(()=>validate(invalid));
while(K.capacity(huge)<=6144)K.addProfile(huge,'复制'+huge.profiles.length,0);assert.throws(()=>validate(huge));
if(process.env.KP_DEFAULTS_JSON){const fixture=JSON.parse(require('node:fs').readFileSync(process.env.KP_DEFAULTS_JSON,'utf8'));assert.deepEqual(K.defaults(),fixture.config);assert.equal(K.capacity(K.defaults()),fixture.bytes);}
console.log('Browser recording, steps, profiles, metadata and capacity: PASS');
// Import/export is a pure transaction over the editor; failed imports cannot mutate it.
const copy=x=>JSON.parse(JSON.stringify(x));
const shared=editorFixture();K.addProfile(shared,'浏览',0);shared.active_profile=1;
K.addStep(shared.profiles[0].bindings[2],{type:1,modifiers:8,keys:[6],consumer:0});
K.addStep(shared.profiles[1].bindings[6],{type:2,modifiers:0,keys:[],consumer:233});
for(let i=0;i<64;i++)K.addStep(shared.profiles[1].bindings[10]);
shared.profiles[1].bindings[10].title='组合测试';assert(validate(shared));
const sharedSettings={...K.settingsDefaults(),config:shared};
const allFile=K.exportFile(sharedSettings,'configuration'),singleFile=K.exportFile(shared,'scenario',1);
const all=K.parseImport(allFile),single=K.parseImport(singleFile);
assert.deepEqual(all.config,shared);assert.equal(single.kind,'scenario');assert.deepEqual(single.config.profiles,[shared.profiles[1]]);
assert.deepEqual(K.applyImport(editorFixture(),all),{config:shared,editing:1,settings:sharedSettings});
const target=editorFixture(),snapshot=JSON.stringify(target),added=K.applyImport(target,single);
assert.equal(added.editing,1);assert.equal(added.config.active_profile,0);assert.equal(added.config.profiles.length,2);
assert.equal(JSON.stringify(target),snapshot);assert.throws(()=>K.applyImport(added.config,single));
const renamed=K.applyImport(added.config,single,{name:'新浏览'});assert.equal(renamed.config.profiles.length,3);assert.equal(renamed.config.profiles[2].name,'新浏览');
const replacing=copy(single);replacing.config.profiles[0].bindings[2].title='替换后的动作';
const replaced=K.applyImport(added.config,replacing,{mode:'replace'});assert.equal(replaced.config.profiles.length,2);assert.equal(replaced.config.profiles[1].bindings[2].title,'替换后的动作');
assert.throws(()=>K.applyImport(target,single,{mode:'replace'}));assert.throws(()=>K.applyImport(target,single,{name:'😀'}));
assert.throws(()=>K.applyImport(c,single));assert.equal(c.profiles.length,8);
// Current and compatible legacy envelopes are accepted; unsupported versions are rejected.
for(const version of [1,2,3,4,5,8]){
  assert.throws(()=>K.parseImport(JSON.stringify({...JSON.parse(allFile),version})));
}
for(const version of [1,2,3,4,7,8]){
  assert.throws(()=>K.parseImport(JSON.stringify({...JSON.parse(singleFile),version})));
}
assert.throws(()=>K.parseImport(JSON.stringify(shared)));
assert.throws(()=>K.parseImport(JSON.stringify({format:'passport-keys',version:5,kind:'configuration',active_profile:shared.active_profile,profiles:shared.profiles})));
assert.throws(()=>K.exportFile(shared,'configuration'));
assert.throws(()=>K.applyImport(target,{kind:'configuration',config:shared}));
// Scenario exports contain no credentials; neither export includes session or pairing state.
assert(!/session|password|pairing|token/.test(singleFile));
assert(!/session|pairing|token/.test(allFile));
const invalidFiles=['null','[]','{}','{',JSON.stringify({...JSON.parse(allFile),version:99}),JSON.stringify({...JSON.parse(allFile),format:'other'}),JSON.stringify({...JSON.parse(singleFile),session:'private'}),' '.repeat(K.fileLimit+1)];
for(const alter of [
 f=>f.active_profile=8,f=>f.active_profile='0',f=>f.profiles=null,f=>f.profiles=[],f=>f.profiles[1].name=f.profiles[0].name,
 f=>f.profiles[0].name='龘',f=>f.profiles[0].bindings[0].title='😀',f=>f.profiles[0].bindings.pop(),f=>f.profiles[0].bindings.push({title:'',steps:[]}),
 f=>f.profiles[0].extra=true,f=>f.profiles[0].bindings[0].extra=true,f=>f.profiles[0].bindings[0].steps[0].extra=true,
 f=>f.profiles[0].bindings[0].steps[0].keys=[4,4],f=>f.profiles[0].bindings[0].steps[0].keys=[102],
 f=>f.profiles[0].bindings[0].steps[0].modifiers=256,f=>f.profiles[0].bindings[0].steps[0].delay_ms=5001,
 f=>f.profiles[0].bindings[0].steps[0].consumer=233,f=>f.profiles[0].bindings[0].steps[0].type=0,
 f=>f.profiles[1].bindings[10].steps.push(copy(f.profiles[1].bindings[10].steps[0])),
 f=>{while(f.profiles.length<8)f.profiles.push({...copy(f.profiles[1]),name:'复制'+f.profiles.length});},
 f=>f.profiles[0].bindings[2].steps[0].delay_ms=1
]){const f=JSON.parse(allFile);alter(f.settings.config);invalidFiles.push(JSON.stringify(f));}
for(const file of invalidFiles){assert.throws(()=>K.parseImport(file));assert.equal(JSON.stringify(target),snapshot);}
// The exact 6144-byte limit survives all-file export/import, including 64-step arrays.
const boundary={version:6,active_profile:0,profiles:[{name:'A',bindings:Array.from({length:11},()=>({title:'',steps:[]}))}]};
for(let b=0;b<7;b++)for(let n=0;n<64;n++)K.addStep(boundary.profiles[0].bindings[b]);
for(let n=0;n<30;n++)K.addStep(boundary.profiles[0].bindings[7]);boundary.profiles[0].bindings[0].title='12345678901';
assert.equal(K.capacity(boundary),6144);assert(validate(boundary));assert.deepEqual(K.parseImport(K.exportFile({...K.settingsDefaults(),config:boundary},'configuration')).config,boundary);
const over=copy(boundary);over.profiles[0].bindings[10].title='A';assert.throws(()=>validate(over));
const overImport={kind:'scenario',config:{version:6,active_profile:0,profiles:[{name:'追加',bindings:editorFixture().profiles[0].bindings}]}};
const before=JSON.stringify(boundary);assert.throws(()=>K.applyImport(boundary,overImport));assert.equal(JSON.stringify(boundary),before);
assert.equal(K.sequenceLabel({title:'',steps:[]}),'无');
// Legacy flags migrate to fixed Command and missing system defaults to All.
const adaptive=editorFixture();adaptive.version=5;const adaptiveStep=adaptive.profiles[0].bindings[5].steps[0];
adaptiveStep.modifiers=2;adaptiveStep.logical_modifiers=3;delete adaptiveStep.system;
const legacy=copy(adaptive);assert(validate(adaptive));assert.equal(adaptive.version,6);
assert.equal(adaptiveStep.modifiers,0x8a);assert.equal(adaptiveStep.system,0);assert(!('logical_modifiers' in adaptiveStep));
assert.equal(K.actionLabel(adaptiveStep),'Shift+Command+RCommand+A');
assert.deepEqual(K.parseImport(K.exportFile({...K.settingsDefaults(),config:adaptive},'configuration')).config,adaptive);
assert.deepEqual(K.parseImport(JSON.stringify({format:'passport-keys',version:6,kind:'configuration',settings:{...K.settingsDefaults(),config:legacy}})).config,adaptive);
assert.deepEqual(K.parseImport(JSON.stringify({format:'passport-keys',version:5,kind:'scenario',profile:legacy.profiles[0]})).config,adaptive);
for(const flags of [-1,4,256,1.5,'1',null]){const bad=copy(legacy);bad.profiles[0].bindings[5].steps[0].logical_modifiers=flags;assert.throws(()=>validate(bad));}
const logicalOnly=copy(legacy);Object.assign(logicalOnly.profiles[0].bindings[5].steps[0],{keys:[],modifiers:0,logical_modifiers:2});assert(validate(logicalOnly));
assert.equal(logicalOnly.profiles[0].bindings[5].steps[0].modifiers,128);
const logicalMedia=copy(legacy);Object.assign(logicalMedia.profiles[0].bindings[5].steps[0],{type:2,keys:[],modifiers:0,consumer:233});assert.throws(()=>validate(logicalMedia));
for(const side of [1,2,3])for(let mods=0;mods<256;mods++){
  const old=editorFixture(),a=old.profiles[0].bindings[5].steps[0];old.version=5;a.modifiers=mods;a.logical_modifiers=side;
  assert(validate(old));assert.equal(a.modifiers,mods|(side&1?8:0)|(side&2?128:0));assert(!('logical_modifiers' in a));
}
assert.equal(K.actionLabel({type:1,modifiers:68,keys:[]}),'Option/Alt+ROption/Alt');
for(const system of [-1,3,256,1.5,'1',null]){const bad=editorFixture();bad.profiles[0].bindings[0].steps[0].system=system;assert.throws(()=>validate(bad));}
const branch=(system,key=4)=>({type:1,modifiers:0,keys:[key],consumer:0,system,delay_ms:80});
const redo={title:'重做',steps:[{...branch(2,29),modifiers:10},{...branch(1,28),modifiers:1,delay_ms:0}]};
assert.deepEqual(K.devicePreview(redo,1),{title:'重做',sequence:'Control+Y',total:1,progress:''});
assert.deepEqual(K.devicePreview(redo,2),{title:'重做',sequence:'Shift+Command+Z',total:1,progress:''});
assert.equal(K.devicePreview({...redo,title:''},1).title,'Control+Y');
assert.equal(K.devicePreview({...redo,title:''},2).title,'Shift+Command+Z');
const previewMacro={title:'',steps:[...redo.steps,{type:2,consumer:205,modifiers:0,keys:[],delay_ms:0}]};
for(const system of [1,2]){
  const view=K.devicePreview(previewMacro,system);assert.equal(view.title,'组合指令');assert.equal(view.total,2);assert.equal(view.progress,'2 / 2');
  assert.equal(view.sequence,K.devicePreview(redo,system).sequence+' → 播放暂停');
}
assert.deepEqual(K.devicePreview({title:'',steps:[redo.steps[0]]},1),{title:'无',sequence:'无',total:0,progress:''});
assert.deepEqual(K.devicePreview({title:'',steps:[]},2),{title:'无',sequence:'无',total:0,progress:''});
const grouped={title:'',steps:[branch(0),branch(1,5),branch(2,6),branch(2,7),branch(1,8),branch(0,9)]};grouped.steps.at(-1).delay_ms=0;
assert.deepEqual(K.stepGroups(grouped).map(g=>g.map(x=>x.index)),[[0],[1,2],[3,4],[5]]);
assert.deepEqual(K.executionSteps(grouped,1).map(x=>[x.number,x.a.keys[0]]),[[1,4],[2,5],[3,8],[4,9]]);
assert.deepEqual(K.executionSteps(grouped,2).map(x=>[x.number,x.a.keys[0]]),[[1,4],[2,6],[3,7],[4,9]]);
assert.deepEqual(K.stepGroups({steps:[branch(1),branch(1),branch(2),branch(2)]}).map(g=>g.length),[1,2,1]);
K.moveGroup(grouped,1,1);assert.deepEqual(grouped.steps.map(a=>a.keys[0]),[4,7,8,5,6,9]);
K.moveGroup(grouped,3,-1);assert.deepEqual(grouped.steps.map(a=>a.keys[0]),[4,7,8,9,5,6]);assert.equal(grouped.steps.at(-1).delay_ms,0);assert.equal(grouped.steps[3].delay_ms,80);
K.removeGroup(grouped,1);assert.deepEqual(grouped.steps.map(a=>a.keys[0]),[4,9,5,6]);
K.removeStep(grouped,2);assert.deepEqual(K.stepGroups(grouped).map(g=>g.length),[1,1,1]);assert.equal(grouped.steps.at(-1).system,2);
for(const system of [0,1,2]){
  const original=branch(system),media=K.changeType(original,2),keyboard=K.changeType(media,1),recorded=K.recordedStep(keyboard,[{code:'KeyC',key:'c',metaKey:true}]);
  for(const a of [media,keyboard,recorded]){assert.equal(a.system,system);assert.equal(a.delay_ms,80);}
}
const systems=editorFixture();systems.profiles[0].bindings[0].steps=[branch(1),branch(2),{type:2,modifiers:0,keys:[],consumer:233,system:2,delay_ms:0}];
assert(validate(systems));K.addProfile(systems,'Copy',0);assert.deepEqual(systems.profiles[0].bindings,systems.profiles[1].bindings);
assert.deepEqual(K.parseImport(K.exportFile({...K.settingsDefaults(),config:systems},'configuration')).config,systems);
assert.deepEqual(K.parseImport(K.exportFile(systems,'scenario')).config.profiles[0],systems.profiles[0]);
const pairedLimit={title:'',steps:[]};for(let i=0;i<64;i++)K.addStep(pairedLimit,branch(i%2+1));
assert.equal(K.stepGroups(pairedLimit).length,32);assert.throws(()=>K.addStep(pairedLimit));
console.log('Legacy Command migration, system persistence, filtering, shared step numbering, group moves/deletion and edit retention: PASS');
if(process.env.KP_CONFIG_TEST){
  const {spawnSync}=require('node:child_process');
  for(const config of [editorFixture(),all.config,added.config,renamed.config,replaced.config,boundary,adaptive,logicalOnly,systems]){
    const run=spawnSync(process.env.KP_CONFIG_TEST,['--stdin'],{input:JSON.stringify(config),encoding:'utf8'});
    assert.equal(run.status,0,run.stderr);const decoded=JSON.parse(run.stdout);
    assert.deepEqual(decoded.config,config);assert.equal(decoded.bytes,K.capacity(config));
  }
  assert.notEqual(spawnSync(process.env.KP_CONFIG_TEST,['--stdin'],{input:JSON.stringify(over),encoding:'utf8'}).status,0);
}
console.log('Current JSON import/export, unsupported-format rejection, atomic failures, 6 KiB boundary and C/browser round trips: PASS');

const h=K.homeDefaults();assert(K.validateHome(h));assert.equal(h.timezone_minutes,480);
for(const avatar of [0,1,2,3])assert(K.validateHome({...h,avatar,name:'星翼',badge:''}));
for(const patch of [{name:''},{name:'龘'},{name:'😀'},{badge:'a\nb'},{avatar:4},{avatar:'0'},{timezone_minutes:-735},{timezone_minutes:841},{timezone_minutes:1},{hour12:2},{hour12:true}])assert.throws(()=>K.validateHome({...h,...patch}));
assert(K.validateHome({...h,name:'龘'},true));
assert.deepEqual(K.timePayload(1791167040999),{epoch:1791167040});
for(const v of [NaN,Infinity,-1,0,4102444800000])assert.throws(()=>K.timePayload(v));
console.log('Browser identity validation, avatar presets, fixed UTC offsets and sync payload: PASS');

assert.deepEqual(K.wifiPayload('家庭 WiFi','test-pass'),{ssid:'家庭 WiFi',password:'test-pass',keep_password:false});
assert(K.wifiPayload('A'.repeat(32),'p'.repeat(63)));assert(K.wifiPayload('test-network','',true));
assert.deepEqual(K.wifiPayload('',''),{ssid:'',password:'',keep_password:false});
for(const args of [['中'.repeat(11),'test-pass'],['A'.repeat(33),'test-pass'],['test','short'],['test','密碼密碼'],['test','p'.repeat(64)],['','test-pass'],['','',true],['test','test-pass',true],['a\n',''],['\ud800','']])assert.throws(()=>K.wifiPayload(...args));
console.log('Browser Wi-Fi UTF-8 byte bounds, password rules, retention and clearing: PASS');

for(const state of [7,8,9,12])assert(!K.wifiJobMessage({state}).includes('密码错误'));
assert(K.wifiJobMessage({state:6}).includes('连接成功'));
assert(K.wifiJobMessage({state:6}).includes('IP 地址'));
assert(!K.wifiJobMessage({state:6}).includes('校时'));
console.log('Wi-Fi result messages distinguish authentication, discovery and timeout; obtaining an IP passes the test: PASS');

async function testSaveSettings(){
  const drafts=()=>({...K.settingsDefaults(),wifi:{version:1,ssid:'test-network',password:'test-pass'}});
  for(const [key,invalid] of [
    ['home',{...K.homeDefaults(),name:'😀'}],
    ['wifi',{version:1,ssid:'test-network',password:'short'}],
    ['config',{...editorFixture(),active_profile:8}]
  ]){
    const settings=drafts();settings[key]=invalid;let writes=0;
    await assert.rejects(K.saveSettings(settings,async()=>{++writes;}));assert.equal(writes,0);
  }
  const pending=drafts(),snapshot=copy(pending),bodies=[],paths=[];
  assert(await K.saveSettings(pending,async(path,body)=>{
    paths.push(path);bodies.push(body);pending.config.profiles[0].name='later edit';
  }));
  assert.deepEqual(paths,['/api/settings']);assert.deepEqual(bodies,[snapshot]);
  let confirmed=false;
  await assert.rejects(K.saveSettings(drafts(),async()=>{throw Error('injected failure');},()=>{confirmed=true;}),/尚未确认保存/);
  assert.equal(confirmed,false);
  const all=drafts();all.home.name='星翼';all.config.profiles[0].name='完整备份';
  const exported=K.exportFile(all,'configuration'),parsed=K.parseImport(exported);
  assert.equal(JSON.parse(exported).version,7);assert(exported.includes('test-pass'));assert(!exported.includes('session'));
  assert.deepEqual(parsed.settings,all);assert.deepEqual(K.applyImport(editorFixture(),parsed).settings,all);
  assert.deepEqual(K.parseImport(K.exportFile(K.settingsDefaults(),'configuration')).settings,K.settingsDefaults());
  for(const alter of [s=>s.home.name='',s=>s.wifi.password='short',s=>delete s.home,s=>s.config.active_profile=99,s=>s.version=9,s=>s.extra=true]){
    const file=JSON.parse(exported);alter(file.settings);assert.throws(()=>K.parseImport(JSON.stringify(file)));
  }
  // The complete transport includes metadata beyond the 38 KiB shortcut JSON limit.
  const nearLimit=copy(boundary),bytes=c=>new TextEncoder().encode(JSON.stringify(c)).length;
  for(const binding of nearLimit.profiles[0].bindings)for(const step of binding.steps){
    if(bytes(nearLimit)>=38900)break;
    const old=step.keys;step.keys=[4,5,6,7,8,9];if(bytes(nearLimit)>38912)step.keys=old;
  }
  assert(K.validate(nearLimit));
  const expanded={...drafts(),config:nearLimit,home:{...K.homeDefaults(),name:'星'.repeat(16),badge:'翼'.repeat(16)},wifi:{version:1,ssid:'A'.repeat(32),password:'"'.repeat(63)}};
  assert(bytes(expanded)>38912&&bytes(expanded)<=39936&&K.validateSettings(expanded));
  assert.deepEqual(K.parseImport(K.exportFile(expanded,'configuration')).settings,expanded);
  if(process.env.KP_CONFIG_TEST){
    const {spawnSync}=require('node:child_process');
    for(const settings of [K.settingsDefaults(),all,{...all,config:boundary},expanded]){
      const run=spawnSync(process.env.KP_CONFIG_TEST,['--settings-stdin'],{input:JSON.stringify(settings),encoding:'utf8'});
      assert.equal(run.status,0,run.stderr);assert.deepEqual(JSON.parse(run.stdout).settings,settings);
    }
  }
  console.log('Complete configuration exports/imports all three modules, validates before one save, snapshots requests and confirms only success: PASS');
}
testSaveSettings().catch(e=>{console.error(e);process.exitCode=1;});
