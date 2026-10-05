const assert=require('node:assert/strict');
require('../main/web/charset.js');require('../main/web/app.js');
const K=globalThis.Keypad;
const {capture,defaults,keys,validate}=K;
const c=defaults();assert.equal(c.profiles[0].bindings.length,11);assert.equal(c.version,4);assert(validate(c));
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
const restored=defaults();restored.profiles[0].name='龘字';restored.profiles[0].bindings[0].title='靐字';
assert(validate(restored,true));assert.throws(()=>validate(restored),/U\+9F98/);
restored.profiles[0].name='Coding';assert.throws(()=>validate(restored),/U\+9750/);
restored.profiles[0].bindings[0].title='复制';assert(validate(restored));
for(const text of ['\ud800','a\nb','\u0085','12345678901234567']){restored.profiles[0].name=text;assert.throws(()=>validate(restored,true));}
const huge=defaults(),macro=huge.profiles[0].bindings[1];for(let i=0;i<64;i++)K.addStep(macro);assert.throws(()=>K.addStep(macro));assert(validate(huge));
const oldSchema=defaults();oldSchema.version=2;assert.throws(()=>validate(oldSchema));
const removed=defaults();removed.profiles[0].bindings[0].description='old';assert.throws(()=>validate(removed));
const invalid=defaults();invalid.profiles[0].bindings[0].steps[0].keys=[4,4];assert.throws(()=>validate(invalid));
invalid.profiles[0].bindings[0].steps[0].keys=[4];invalid.profiles[0].bindings[0].steps[0].delay_ms=1;assert.throws(()=>validate(invalid));
invalid.profiles[0].bindings[0].steps[0].delay_ms=0;invalid.profiles[0].bindings[0].title='😀';assert.throws(()=>validate(invalid));
while(K.capacity(huge)<=6144)K.addProfile(huge,'复制'+huge.profiles.length,0);assert.throws(()=>validate(huge));
if(process.env.KP_DEFAULTS_JSON){const fixture=JSON.parse(require('node:fs').readFileSync(process.env.KP_DEFAULTS_JSON,'utf8'));assert.deepEqual(defaults(),fixture.config);assert.equal(K.capacity(defaults()),fixture.bytes);}
console.log('Browser recording, steps, profiles, metadata and capacity: PASS');
// Import/export is a pure transaction over the editor; failed imports cannot mutate it.
const copy=x=>JSON.parse(JSON.stringify(x));
const shared=defaults();K.addProfile(shared,'浏览',0);shared.active_profile=1;
K.addStep(shared.profiles[0].bindings[2],{type:1,modifiers:8,keys:[6],consumer:0});
K.addStep(shared.profiles[1].bindings[6],{type:2,modifiers:0,keys:[],consumer:233});
for(let i=0;i<64;i++)K.addStep(shared.profiles[1].bindings[10]);
shared.profiles[1].bindings[10].title='组合测试';assert(validate(shared));
const allFile=K.exportFile(shared,'configuration'),singleFile=K.exportFile(shared,'scenario',1);
const all=K.parseImport(allFile),single=K.parseImport(singleFile);
assert.deepEqual(all.config,shared);assert.equal(single.kind,'scenario');assert.deepEqual(single.config.profiles,[shared.profiles[1]]);
assert.deepEqual(K.applyImport(defaults(),all),{config:shared,editing:1});
const target=defaults(),snapshot=JSON.stringify(target),added=K.applyImport(target,single);
assert.equal(added.editing,1);assert.equal(added.config.active_profile,0);assert.equal(added.config.profiles.length,2);
assert.equal(JSON.stringify(target),snapshot);assert.throws(()=>K.applyImport(added.config,single));
const renamed=K.applyImport(added.config,single,{name:'新浏览'});assert.equal(renamed.config.profiles.length,3);assert.equal(renamed.config.profiles[2].name,'新浏览');
const replacing=copy(single);replacing.config.profiles[0].bindings[2].title='替换后的动作';
const replaced=K.applyImport(added.config,replacing,{mode:'replace'});assert.equal(replaced.config.profiles.length,2);assert.equal(replaced.config.profiles[1].bindings[2].title,'替换后的动作');
assert.throws(()=>K.applyImport(target,single,{mode:'replace'}));assert.throws(()=>K.applyImport(target,single,{name:'😀'}));
assert.throws(()=>K.applyImport(c,single));assert.equal(c.profiles.length,8);
// Every former slot, including both holds and OK double, lands in its correct v4 slot.
const oldSlots=[0,1,3,4,5,7,8,9],legacy={version:3,active_profile:1,profiles:shared.profiles.map((p,i)=>({name:p.name,bindings:oldSlots.map((_,n)=>({title:'旧'+n,steps:[{type:1,modifiers:i,keys:[4+n],consumer:0,delay_ms:0}]}))}))};
const upgraded=K.parseImport(JSON.stringify(legacy));assert.equal(upgraded.config.version,4);assert.equal(upgraded.config.active_profile,1);
for(const [i,p] of upgraded.config.profiles.entries()){
  for(const [old,slot] of oldSlots.entries())assert.deepEqual(p.bindings[slot],legacy.profiles[i].bindings[old]);
  for(const slot of [2,6,10])assert.deepEqual(p.bindings[slot],{title:'',steps:[]});
}
assert.deepEqual(K.parseImport(K.exportFile(upgraded.config,'configuration')).config,upgraded.config);
const oldSingle={format:'passport-keys',version:3,kind:'scenario',profile:legacy.profiles[0]};
assert.deepEqual(K.parseImport(JSON.stringify(oldSingle)).config.profiles,[upgraded.config.profiles[0]]);
const v2=copy(legacy);v2.version=2;for(const p of v2.profiles)for(const b of p.bindings)b.description='旧说明\n第二行';
assert.deepEqual(K.parseImport(JSON.stringify(v2)).config,upgraded.config);
// Exports whitelist configuration fields; no session, password or pairing state.
for(const text of [allFile,singleFile])assert(!/session|password|pairing|token/.test(text));
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
]){const f=JSON.parse(allFile);alter(f);invalidFiles.push(JSON.stringify(f));}
for(const file of invalidFiles){assert.throws(()=>K.parseImport(file));assert.equal(JSON.stringify(target),snapshot);}
// The exact 6144-byte limit survives all-file export/import, including 64-step arrays.
const boundary={version:4,active_profile:0,profiles:[{name:'A',bindings:Array.from({length:11},()=>({title:'',steps:[]}))}]};
for(let b=0;b<7;b++)for(let n=0;n<64;n++)K.addStep(boundary.profiles[0].bindings[b]);
for(let n=0;n<30;n++)K.addStep(boundary.profiles[0].bindings[7]);boundary.profiles[0].bindings[0].title='12345678901';
assert.equal(K.capacity(boundary),6144);assert(validate(boundary));assert.deepEqual(K.parseImport(K.exportFile(boundary,'configuration')).config,boundary);
const over=copy(boundary);over.profiles[0].bindings[10].title='A';assert.throws(()=>validate(over));
const overImport={kind:'scenario',config:{version:4,active_profile:0,profiles:[{name:'追加',bindings:defaults().profiles[0].bindings}]}};
const before=JSON.stringify(boundary);assert.throws(()=>K.applyImport(boundary,overImport));assert.equal(JSON.stringify(boundary),before);
assert.equal(K.sequenceLabel({title:'',steps:[]}),'无');
if(process.env.KP_CONFIG_TEST){
  const {spawnSync}=require('node:child_process');
  for(const config of [defaults(),all.config,added.config,renamed.config,replaced.config,upgraded.config,boundary]){
    const run=spawnSync(process.env.KP_CONFIG_TEST,['--stdin'],{input:JSON.stringify(config),encoding:'utf8'});
    assert.equal(run.status,0,run.stderr);const decoded=JSON.parse(run.stdout);
    assert.deepEqual(decoded.config,config);assert.equal(decoded.bytes,K.capacity(config));
  }
  assert.notEqual(spawnSync(process.env.KP_CONFIG_TEST,['--stdin'],{input:JSON.stringify(over),encoding:'utf8'}).status,0);
}
console.log('Versioned JSON import/export, legacy slots, atomic failures, 6 KiB boundary and C/browser round trips: PASS');

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
assert(K.wifiJobMessage({state:6}).includes('连接成功'));assert(K.wifiJobMessage({state:10}).includes('认证通过'));
console.log('Wi-Fi result messages distinguish authentication, discovery, timeout and time-service failure: PASS');
