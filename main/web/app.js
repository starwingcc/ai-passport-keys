/* The pure capture helpers are also exercised by the host test suite. */
(function (root) {
  'use strict';
  const keys = [];
  function add(code, usage, label) { keys.push({code, usage, label}); }
  for (let i = 0; i < 26; i++) add('Key' + String.fromCharCode(65+i), 4+i, String.fromCharCode(65+i));
  for (let i = 1; i <= 9; i++) add('Digit'+i, 29+i, String(i));
  add('Digit0',39,'0');
  [['Enter',40,'Enter / 回车'],['Escape',41,'Esc'],['Backspace',42,'Backspace'],['Tab',43,'Tab'],['Space',44,'Space'],['Minus',45,'-'],['Equal',46,'='],['BracketLeft',47,'['],['BracketRight',48,']'],['Backslash',49,'\\'],['IntlHash',50,'Intl #'],['Semicolon',51,';'],['Quote',52,"'"],['Backquote',53,'`'],['Comma',54,','],['Period',55,'.'],['Slash',56,'/'],['CapsLock',57,'Caps Lock']].forEach(a=>add(...a));
  for(let i=1;i<=12;i++)add('F'+i,57+i,'F'+i);
  [['PrintScreen',70,'Print Screen'],['ScrollLock',71,'Scroll Lock'],['Pause',72,'Pause'],['Insert',73,'Insert'],['Home',74,'Home'],['PageUp',75,'Page Up'],['Delete',76,'Delete / 向前删除'],['End',77,'End'],['PageDown',78,'Page Down'],['ArrowRight',79,'→'],['ArrowLeft',80,'←'],['ArrowDown',81,'↓'],['ArrowUp',82,'↑'],['NumLock',83,'Num Lock'],['NumpadDivide',84,'Num /'],['NumpadMultiply',85,'Num *'],['NumpadSubtract',86,'Num -'],['NumpadAdd',87,'Num +'],['NumpadEnter',88,'Num Enter']].forEach(a=>add(...a));
  for(let i=1;i<=9;i++)add('Numpad'+i,88+i,'Num '+i);
  [['Numpad0',98,'Num 0'],['NumpadDecimal',99,'Num .'],['IntlBackslash',100,'Intl \\'],['ContextMenu',101,'Menu'],['NumpadEqual',103,'Num =']].forEach(a=>add(...a));
  for(let i=13;i<=24;i++)add('F'+i,104+i-13,'F'+i);
  const modCodes=['ControlLeft','ShiftLeft','AltLeft','MetaLeft','ControlRight','ShiftRight','AltRight','MetaRight'];
  const modLabels=['左 Ctrl','左 Shift','左 Option','左 Command','右 Ctrl','右 Shift','右 Option','右 Command'];
  const media=[[233,'音量加'],[234,'音量减'],[226,'静音'],[205,'播放 / 暂停'],[182,'上一首'],[181,'下一首']];
  const clone=x=>JSON.parse(JSON.stringify(x));
  const empty=()=>({title:'',steps:[]});
  const newStep=()=>({type:1,modifiers:0,keys:[40],consumer:0,delay_ms:0});
  const modNames=['Control','Shift','Option','Command','RControl','RShift','ROption','RCommand'];
  const usageNames={40:'Enter',41:'Esc',42:'Backspace',43:'Tab',44:'Space',76:'Delete',79:'Right',80:'Left',81:'Down',82:'Up'};
  function actionLabel(a){
    if(a.type===2)return media.find(m=>m[0]===a.consumer)?.[1].replaceAll(' / ','')||'媒体键';
    const names=[];if(a.logical_modifiers&1)names.push('Cmd/Ctrl');if(a.logical_modifiers&2)names.push('RCmd/Ctrl');names.push(...modNames.filter((_,i)=>a.modifiers&(1<<i)));
    for(const v of a.keys){let name;if(v>=4&&v<=29)name=String.fromCharCode(65+v-4);else if(v>=30&&v<=39)name=String((v-29)%10);else if(v>=58&&v<=69)name='F'+(v-57);else if(v>=104&&v<=115)name='F'+(v-104+13);else name=usageNames[v]||'Key'+v.toString(16).toUpperCase().padStart(2,'0');names.push(name);}
    return names.join('+');
  }
  const sequenceLabel=b=>b.steps.length?b.steps.map(actionLabel).join(' → '):'无';
  const titleLabel=b=>b.title||(b.steps.length>1?'组合指令':sequenceLabel(b));
  function defaults(){const bindings=Array.from({length:11},empty);[[0,0,41],[3,4,0],[4,0,42],[5,8,4],[7,0,81],[8,0,40]].forEach(([slot,mod,key])=>{bindings[slot].steps=[{type:1,modifiers:mod,keys:key?[key]:[],consumer:0,delay_ms:0}];});bindings[8].title='回车';return{version:5,active_profile:0,profiles:[{name:'默认',bindings}]};}
  function capture(events){
    let modifiers=0;const pressed=new Set();
    for(const e of events){
      if(e.repeat)continue;
      if(e.isComposing||e.key==='Process'||e.key==='Dead'||e.key==='Fn'||e.key==='FnLock')throw Error('该按键无法录制，请手动选择标准按键。');
      const m=modCodes.indexOf(e.code);
      if(m>=0)modifiers|=1<<m;
      else{const k=keys.find(k=>k.code===e.code);if(!k)throw Error('无法识别该按键，请手动配置。');pressed.add(k.usage);}
      if(e.ctrlKey&&!(modifiers&17))modifiers|=1;
      if(e.shiftKey&&!(modifiers&34))modifiers|=2;
      if(e.altKey&&!(modifiers&68))modifiers|=4;
      if(e.metaKey&&!(modifiers&136))modifiers|=8;
    }
    if(pressed.size>6)throw Error('最多支持六个普通按键。');
    if(!pressed.size&&!modifiers)throw Error('没有收到按键，请手动选择。');
    return{type:1,modifiers,keys:[...pressed],consumer:0};
  }
  const utf8=s=>new TextEncoder().encode(s).length;
  function supported(cp){const ranges=root.KeypadCharset;let lo=0,hi=ranges.length;while(lo<hi){const mid=(lo+hi)>>1;if(cp<ranges[mid][0])hi=mid;else if(cp>ranges[mid][1])lo=mid+1;else return true;}return false;}
  function validateText(s,max,displayCoverage=true){if(typeof s!=='string'||Array.from(s).length>max)throw Error('名称和标题最多 16 字。');for(const ch of s){const cp=ch.codePointAt(0);if(cp<32||(cp>=127&&cp<=159)||(cp>=0xD800&&cp<=0xDFFF)||(displayCoverage&&!supported(cp)))throw Error('设备不支持字符“'+ch+'”（U+'+cp.toString(16).toUpperCase()+'），请替换。');}}
  const homeDefaults=()=>({version:1,name:'USER',badge:'',avatar:0,timezone_minutes:480,hour12:0});
  function validateHome(h,stored=false){
    fields(h,['version','name','badge','avatar','timezone_minutes','hour12']);
    validateText(h.name,16,!stored);validateText(h.badge,16,!stored);
    if(h.version!==1||!h.name||!Number.isInteger(h.avatar)||h.avatar<0||h.avatar>3||
      !Number.isInteger(h.timezone_minutes)||h.timezone_minutes< -720||h.timezone_minutes>840||h.timezone_minutes%15||
      ![0,1].includes(h.hour12))throw Error('工牌名称、头像或时间设置无效。');return true;
  }
  function timePayload(milliseconds=Date.now()){
    const epoch=Math.floor(milliseconds/1000);
    if(!Number.isSafeInteger(epoch)||epoch<1577836800||epoch>4102444799)throw Error('请检查手机或电脑的系统时间（支持 2020–2099 年）。');
    return {epoch};
  }
  function capacity(c){let used=1;const put=s=>{if(s)used+=utf8(s)+1;};for(const p of c.profiles){put(p.name);for(const b of p.bindings){put(b.title);if(b.steps.length){used=(used+1)&~1;used+=b.steps.length*12;}}}return 392+used;}
  function fields(value,names){
    if(!value||typeof value!=='object'||Array.isArray(value)||Object.keys(value).length!==names.length||names.some(n=>!Object.hasOwn(value,n)))throw Error('文件结构或字段无效。');
  }
  function validate(c,stored=false){
    fields(c,['version','active_profile','profiles']);
    if(c.version!==5||!Array.isArray(c.profiles)||!c.profiles.length||c.profiles.length>8||!Number.isInteger(c.active_profile)||c.active_profile<0||c.active_profile>=c.profiles.length)throw Error('情景配置无效。');
    const names=new Set();for(const p of c.profiles){
      fields(p,['name','bindings']);validateText(p.name,16,!stored);
      if(!p.name||names.has(p.name))throw Error('情景名称不能为空或重复。');names.add(p.name);
      if(!Array.isArray(p.bindings)||p.bindings.length!==11)throw Error('每个情景必须有十一个动作。');
      for(const b of p.bindings){
        fields(b,['title','steps']);validateText(b.title,16,!stored);
        if(!Array.isArray(b.steps)||b.steps.length>64)throw Error('每个动作最多 64 步。');
        for(const [i,a] of b.steps.entries()){
          fields(a,Object.hasOwn(a,'logical_modifiers')?['type','modifiers','keys','consumer','delay_ms','logical_modifiers']:['type','modifiers','keys','consumer','delay_ms']);
          if(Object.hasOwn(a,'logical_modifiers')&&(!Number.isInteger(a.logical_modifiers)||a.logical_modifiers<0||a.logical_modifiers>3))throw Error('Cmd/Ctrl 修饰键无效。');
          if(!Number.isInteger(a.delay_ms)||a.delay_ms<0||a.delay_ms>5000||(i===b.steps.length-1&&a.delay_ms!==0))throw Error('步骤间隔必须为 0–5000ms，最后一步为 0。');
          if(!Number.isInteger(a.modifiers)||a.modifiers<0||a.modifiers>255||!Array.isArray(a.keys)||a.keys.length>6||new Set(a.keys).size!==a.keys.length||a.keys.some(k=>!Number.isInteger(k)||!keys.some(v=>v.usage===k)))throw Error('组合键无效或包含重复按键。');
          if(a.type===1){if(a.consumer!==0||(!a.modifiers&&!a.logical_modifiers&&!a.keys.length))throw Error('键盘步骤至少需要一个按键或修饰键。');}
          else if(a.type===2){if(a.modifiers||a.logical_modifiers||a.keys.length||!media.some(m=>m[0]===a.consumer))throw Error('媒体步骤无效。');}
          else throw Error('步骤类型无效。');
        }
      }
    }
    if(capacity(c)>6144)throw Error('配置超过 6 KiB，请减少步骤或文字。');
    if(utf8(JSON.stringify(c))>32768)throw Error('配置超过网页传输容量，请减少步骤。');return true;
  }
  // Files are configuration-only envelopes; the HTTP session never enters them.
  const fileFormat='passport-keys',fileLimit=65536;
  function exportFile(c,kind,index=c.active_profile){
    validate(c,true);
    if(kind==='scenario'){
      if(!Number.isInteger(index)||index<0||index>=c.profiles.length)throw Error('请选择要导出的情景。');
      return JSON.stringify({format:fileFormat,version:5,kind,profile:clone(c.profiles[index])});
    }
    if(kind!=='configuration')throw Error('导出类型无效。');
    return JSON.stringify({format:fileFormat,version:5,kind,active_profile:c.active_profile,profiles:clone(c.profiles)});
  }
  function upgradeProfiles(profiles,version){
    if(!Array.isArray(profiles)||!profiles.length||profiles.length>8)throw Error('文件情景数量无效。');
    return profiles.map(p=>{
      fields(p,['name','bindings']);
      if(!Array.isArray(p.bindings)||p.bindings.length!==(version>=4?11:8))throw Error('文件动作槽位数量无效。');
      const bindings=p.bindings.map(b=>{
        const steps=()=>{if(!Array.isArray(b.steps))throw Error('文件步骤无效。');return b.steps.map(a=>{fields(a,version===5&&Object.hasOwn(a,'logical_modifiers')?['type','modifiers','keys','consumer','delay_ms','logical_modifiers']:['type','modifiers','keys','consumer','delay_ms']);return clone(a);});};
        if(version===2){
          fields(b,['title','description','steps']);
          if(typeof b.description!=='string'||Array.from(b.description).length>64)throw Error('旧版动作说明无效。');
          for(const line of b.description.split('\n'))validateText(line,64);
          return {title:b.title,steps:steps()};
        }
        fields(b,['title','steps']);return {title:b.title,steps:steps()};
      });
      if(version>=4)return {name:p.name,bindings};
      // v2/v3: UP single/double/hold, DOWN single/double/hold, OK single/double.
      return {name:p.name,bindings:[bindings[0],bindings[1],empty(),bindings[2],bindings[3],bindings[4],empty(),bindings[5],bindings[6],bindings[7],empty()]};
    });
  }
  function parseImport(text){
    if(typeof text!=='string'||utf8(text)>fileLimit)throw Error('文件过大，最多 64 KiB。');
    let file;try{file=JSON.parse(text);}catch{throw Error('文件不是有效的 JSON。');}
    if(!file||![2,3,4,5].includes(file.version))throw Error('不支持该配置文件版本。');
    let kind,profiles,active;
    if(Object.hasOwn(file,'format')){
      if(file.format!==fileFormat)throw Error('文件格式不匹配。');kind=file.kind;
      if(kind==='scenario'){fields(file,['format','version','kind','profile']);profiles=[file.profile];active=0;}
      else if(kind==='configuration'){fields(file,['format','version','kind','active_profile','profiles']);profiles=file.profiles;active=file.active_profile;}
      else throw Error('文件导入类型无效。');
    }else{
      fields(file,['version','active_profile','profiles']);kind='configuration';profiles=file.profiles;active=file.active_profile;
    }
    const config={version:5,active_profile:active,profiles:upgradeProfiles(profiles,file.version)};
    validate(config);return {kind,config};
  }
  function applyImport(current,payload,options={}){
    fields(payload,['kind','config']);validate(payload.config);
    if(payload.kind==='configuration')return {config:clone(payload.config),editing:payload.config.active_profile};
    if(payload.kind!=='scenario'||payload.config.profiles.length!==1)throw Error('单情景文件无效。');
    const next=clone(current),profile=clone(payload.config.profiles[0]);
    const existing=next.profiles.findIndex(p=>p.name===profile.name);let editing;
    if(options.mode==='replace'){
      if(existing<0)throw Error('没有可替换的同名情景。');editing=existing;next.profiles[editing]=profile;
    }else{
      if(options.name!==undefined)profile.name=options.name;
      if(next.profiles.some(p=>p.name===profile.name))throw Error('同名情景已存在，请选择替换或重命名。');
      editing=next.profiles.length;next.profiles.push(profile);
    }
    validate(next);return {config:next,editing};
  }
  function addStep(b,a=newStep()){if(b.steps.length>=64)throw Error('每个动作最多 64 步。');if(b.steps.length)b.steps.at(-1).delay_ms=80;b.steps.push({...clone(a),delay_ms:0});}
  function removeStep(b,index){b.steps.splice(index,1);if(b.steps.length)b.steps.at(-1).delay_ms=0;}
  function moveStep(b,index,delta){const to=index+delta;if(to<0||to>=b.steps.length)return;const last=b.steps.at(-1);[b.steps[index],b.steps[to]]=[b.steps[to],b.steps[index]];if(b.steps.at(-1)!==last)last.delay_ms=80;b.steps.at(-1).delay_ms=0;}
  function addProfile(c,name,copy=-1){if(c.profiles.length>=8)throw Error('最多 8 个情景。');validateText(name,16);if(!name||c.profiles.some(p=>p.name===name))throw Error('情景名称不能为空或重复。');c.profiles.push(copy<0?{name,bindings:Array.from({length:11},empty)}:{...clone(c.profiles[copy]),name});return c.profiles.length-1;}
  function removeProfile(c,index){if(c.profiles.length===1)throw Error('至少保留一个情景。');c.profiles.splice(index,1);if(c.active_profile===index)c.active_profile=0;else if(c.active_profile>index)--c.active_profile;}
  function wifiPayload(ssid,password,keep_password=false){
    if(typeof ssid!=='string'||typeof password!=='string'||typeof keep_password!=='boolean'||new TextEncoder().encode(ssid).length>32||
      [...ssid].some(c=>{const n=c.codePointAt(0);return n<32||(n>=127&&n<=159)||(n>=0xd800&&n<=0xdfff);})||
      !/^[\x20-\x7e]{0,63}$/.test(password)||(password.length>0&&password.length<8)||(!ssid&&(password||keep_password))||(keep_password&&password))
      throw Error('网络名称最多 32 字节；密码应为 8–63 位英文、数字或符号。无密码网络可留空。');
    return {ssid,password,keep_password};
  }
  function wifiJobMessage(job){
    const messages=['尚未测试','正在准备','正在扫描附近 Wi-Fi','正在连接，请保持连接 Passport 热点','Wi-Fi 已连接，正在校时','扫描完成','连接成功，认证通过且已校时。请保存网络设置。','认证失败，请检查密码或路由器设置。','未找到网络，请检查名称、距离和 2.4 GHz 设置。','连接超时，请检查路由器和信号。','Wi-Fi 认证通过，但网络校时失败。请检查外网连接，密码可正常使用。','连接或扫描失败，请重试。','网络安全方式不兼容，请检查路由器设置。'];
    return messages[job.state]||'设备返回了未知结果，请重试。';
  }
  root.Keypad={wifiJobMessage,wifiPayload,homeDefaults,validateHome,timePayload,exportFile,parseImport,applyImport,fileLimit,keys,modCodes,media,defaults,capture,actionLabel,sequenceLabel,titleLabel,capacity,validate,validateText,addStep,removeStep,moveStep,addProfile,removeProfile};
  if(typeof document==='undefined')return;
  let pendingImport=null;
  let config,session='',selected=0,editing=0,dirty=false,busy=false,recording=null;
  let home,homeDirty=false,wifi,wifiDirty=false,wifiNetworks=[],wifiRevision=0;
  const $=id=>document.getElementById(id);
  function status(text,error=false){$('status').textContent=text;$('status').className=error?'error':'';}
  function metrics(){if(!config)return;const used=capacity(config);$('capacity').textContent=used+' / 6144 B · '+config.profiles.length+' / 8 情景';$('capacity').classList.toggle('error',used>6144);$('current-profile').textContent='当前：'+config.profiles[config.active_profile].name;}
  function changed(){dirty=true;$('dirty').textContent='有未保存的修改';metrics();}
  function homeChanged(){homeDirty=true;$('home-saved').textContent='工牌有未保存的修改';}
  function homeRender(){
    $('home-name').value=home.name;$('home-badge').value=home.badge;
    $('home-avatar').value=home.avatar;$('home-timezone').value=home.timezone_minutes;$('home-hour12').value=home.hour12;
  }
  for(let offset=-720;offset<=840;offset+=15){
    const absolute=Math.abs(offset),text='UTC'+(offset<0?'-':'+')+String(Math.floor(absolute/60)).padStart(2,'0')+':'+String(absolute%60).padStart(2,'0');
    const option=document.createElement('option');option.value=offset;option.textContent=text;$('home-timezone').append(option);
  }
  for(const [id,field] of [['home-name','name'],['home-badge','badge'],['home-avatar','avatar'],['home-timezone','timezone_minutes'],['home-hour12','hour12']]){
    $(id).addEventListener(field==='name'||field==='badge'?'input':'change',()=>{home[field]=field==='name'||field==='badge'?$(id).value:Number($(id).value);homeChanged();});
  }
  function element(tag,text,cls){const el=document.createElement(tag);if(text!==undefined)el.textContent=text;if(cls)el.className=cls;return el;}
  function button(text,cls,fn){const b=element('button',text,cls);b.type='button';b.addEventListener('click',()=>{try{fn();}catch(e){status(e.message,true);}});return b;}
  function select(options,value,fn){const s=element('select');for(const [v,label] of options){const o=element('option',label);o.value=v;s.append(o);}s.value=value;s.addEventListener('change',()=>fn(Number(s.value)));return s;}
  function preview(b,gesture){const frame=element('div',undefined,'device-preview');frame.setAttribute('aria-label','设备详情预览');const band=element('div',undefined,'preview-band');band.append(element('div',titleLabel(b),'preview-title'));frame.append(element('div',config.profiles[editing].name,'preview-profile'),element('div','--%','preview-battery'),element('div',['上','下','OK'][selected]+' · '+gesture,'preview-state'),band,element('div',sequenceLabel(b),'preview-keys'),element('div','已完成','preview-status'),element('div',b.steps.length>1?b.steps.length+' / '+b.steps.length:'','preview-progress'));return frame;}
  function metadata(card,b,name){const wrap=element('label','动作标题','text-field'),input=element('input');input.value=b.title;input.maxLength=16;input.setAttribute('aria-label',name+'动作标题');input.placeholder='留空则自动命名';let frame;input.addEventListener('input',()=>{b.title=input.value;changed();frame.querySelector('.preview-title').textContent=titleLabel(b);});wrap.append(input);card.append(wrap);const details=element('details',undefined,'preview-details');details.append(element('summary','查看设备全屏预览'));frame=preview(b,name);details.append(frame);card.append(details);}
  function render(){
    $('editor').disabled=busy||!config;$('forget').disabled=busy||!config;$('wifi-fields').disabled=busy||!wifi;if(!config)return;
    metrics();$('profile').replaceChildren(...config.profiles.map((p,i)=>{const o=element('option',p.name+(i===config.active_profile?' · 当前':''));o.value=i;return o;}));$('profile').value=editing;$('profile-name').value=config.profiles[editing].name;
    $('activate-profile').disabled=editing===config.active_profile;$('delete-profile').disabled=config.profiles.length===1;$('new-profile').disabled=$('copy-profile').disabled=config.profiles.length>=8;
    $('actions').replaceChildren();
    ['单击','双击','三击','长按'].forEach((name,gesture)=>{
      const slot=selected*4+gesture,fixed=slot===11;const card=element('article',undefined,'action'+(fixed?' fixed':'')+(gesture===2?' triple':''));
      const head=element('div',undefined,'action-head');head.append(element('h2',name),element('span',`0${gesture+1} / ${fixed?'SYSTEM':'ACTION'}`,'index'));card.append(head);
      if(fixed){card.append(element('div','返回上一层','binding'),element('p','OK 长按 500ms 保留为全局返回。组合执行期间，可随时取消并进入主菜单。'));$('actions').append(card);return;}
      const b=config.profiles[editing].bindings[slot];metadata(card,b,name);
      if(gesture===3)card.append(element('p','上下键长按 200ms 触发；只有长按有步骤时，按下立即执行。单步保持到松开，多步完整执行一次。','empty-state'));
      const heading=element('div',undefined,'steps-heading');heading.append(element('h3','操作步骤'),element('span',b.steps.length+' / 64'));card.append(heading);
      if(!b.steps.length)card.append(element('p','无。添加步骤后开始配置。','empty-state'));
      b.steps.forEach((a,index)=>{
        const panel=element('section',undefined,'step');const top=element('div',undefined,'step-head');top.append(element('span',String(index+1).padStart(2,'0'),'step-index'));
        const kind=select([[1,'键盘组合键'],[2,'媒体控制']],a.type,v=>{cancelRecording(false);b.steps[index]=v===1?{...newStep(),delay_ms:a.delay_ms}:{type:2,modifiers:0,keys:[],consumer:205,delay_ms:a.delay_ms};changed();render();});kind.setAttribute('aria-label',name+'第'+(index+1)+'步类型');top.append(kind);
        const up=button('↑','step-tool',()=>{cancelRecording(false);moveStep(b,index,-1);changed();render();});up.disabled=index===0;up.setAttribute('aria-label','上移第'+(index+1)+'步');
        const down=button('↓','step-tool',()=>{cancelRecording(false);moveStep(b,index,1);changed();render();});down.disabled=index===b.steps.length-1;down.setAttribute('aria-label','下移第'+(index+1)+'步');
        const del=button('×','step-tool',()=>{cancelRecording(false);removeStep(b,index);changed();render();});del.setAttribute('aria-label','删除第'+(index+1)+'步');top.append(up,down,del);panel.append(top);
        const chord=element('div',actionLabel(a),'step-chord');panel.append(chord);
        if(a.type===2){const ms=select(media,a.consumer,v=>{a.consumer=v;changed();render();});ms.className='type';ms.setAttribute('aria-label','媒体动作');panel.append(ms);}
        else{
          const rec=button('录制这一步','record',()=>{if(recording?.slot===slot&&recording.index===index){cancelRecording();return;}cancelRecording(false);recording={slot,index,events:[],down:new Set(),button:rec,timer:null};rec.textContent='请按一组组合键… 点击取消';rec.classList.add('recording');status('按下一组组合键后松开。多组操作请分别添加步骤；系统拦截时可手动配置。');});panel.append(rec);
          const manual=element('details',undefined,'manual');manual.append(element('summary','手动选择按键'));const mods=element('div',undefined,'modifiers');
          for(let side=0;side<2;side++){
            const column=element('div',undefined,'modifier-column');column.setAttribute('aria-label',side?'右修饰键':'左修饰键');
            const choices=[[side?'右 Cmd/Ctrl':'左 Cmd/Ctrl','logical_modifiers',1<<side],...modLabels.slice(side*4,side*4+4).map((label,i)=>[label,'modifiers',1<<(side*4+i)])];
            for(const [label,field,bit] of choices){const wrap=element('label'),input=element('input');input.type='checkbox';input.checked=!!(a[field]&bit);input.addEventListener('change',()=>{a[field]=input.checked?a[field]|bit:a[field]&~bit;if(field==='logical_modifiers'&&!a[field])delete a[field];changed();chord.textContent=actionLabel(a);card.querySelector('.preview-keys').textContent=sequenceLabel(b);card.querySelector('.preview-title').textContent=titleLabel(b);});wrap.append(input,document.createTextNode(label));column.append(wrap);}
            mods.append(column);
          }
          manual.append(mods);
          a.keys.forEach((v,i)=>{const row=element('div',undefined,'key-choice');const choice=select(keys.map(k=>[k.usage,k.label]),v,value=>{if(a.keys.includes(value)){status('组合键不能包含重复按键。',true);choice.value=v;return;}a.keys[i]=value;changed();render();});choice.setAttribute('aria-label','第'+(index+1)+'步第'+(i+1)+'个按键');row.append(choice,button('移除','',()=>{a.keys.splice(i,1);changed();render();}));manual.append(row);});if(a.keys.length<6)manual.append(button('+ 添加普通按键','add',()=>{a.keys.push(keys.find(k=>!a.keys.includes(k.usage)).usage);changed();render();}));panel.append(manual);
        }
        const delay=element('label',index===b.steps.length-1?'最后一步，无额外等待':'释放后等待','step-delay');const input=element('input');input.type='number';input.min=0;input.max=5000;input.step=1;input.value=a.delay_ms;input.disabled=index===b.steps.length-1;input.setAttribute('aria-label','第'+(index+1)+'步后等待毫秒');input.addEventListener('input',()=>{a.delay_ms=Number(input.value);changed();});delay.append(input,document.createTextNode('ms'));panel.append(delay);card.append(panel);
      });
      const add=button('+ 添加一步','add-step',()=>{cancelRecording(false);addStep(b);changed();render();});add.disabled=b.steps.length>=64;card.append(add);$('actions').append(card);
    });
  }
  function cancelRecording(message=true){if(!recording)return;clearTimeout(recording.timer);recording.button.textContent='录制这一步';recording.button.classList.remove('recording');recording=null;if(message)status('录制已取消，原步骤未修改。');}
  function finishRecording(){if(!recording)return;const saved=recording;try{const b=config.profiles[editing].bindings[saved.slot],old=b.steps[saved.index];b.steps[saved.index]={...capture(saved.events),delay_ms:old.delay_ms};cancelRecording(false);changed();render();status('已录制，请保存到 Passport。');}catch(e){cancelRecording(false);status(e.message,true);}}
  document.addEventListener('keydown',e=>{if(!recording)return;e.preventDefault();e.stopPropagation();if(e.repeat)return;clearTimeout(recording.timer);recording.events.push({code:e.code,key:e.key,ctrlKey:e.ctrlKey,altKey:e.altKey,shiftKey:e.shiftKey,metaKey:e.metaKey,isComposing:e.isComposing});recording.down.add(e.code);});
  document.addEventListener('keyup',e=>{if(!recording)return;e.preventDefault();e.stopPropagation();recording.down.delete(e.code);if(!recording.down.size)finishRecording();else if(modCodes.includes(e.code))recording.timer=setTimeout(finishRecording,120);});
  window.addEventListener('blur',()=>cancelRecording());document.addEventListener('visibilitychange',()=>{if(document.hidden)cancelRecording();});
  document.querySelectorAll('.tab').forEach(tab=>tab.addEventListener('click',()=>{cancelRecording(false);selected=Number(tab.dataset.key);document.querySelectorAll('.tab').forEach(t=>{t.classList.toggle('active',t===tab);t.setAttribute('aria-selected',String(t===tab));});render();}));
  $('profile').addEventListener('change',()=>{cancelRecording(false);editing=Number($('profile').value);render();});
  $('profile-name').addEventListener('input',()=>{config.profiles[editing].name=$('profile-name').value;changed();$('profile').options[editing].textContent=$('profile-name').value+(editing===config.active_profile?' · 当前':'');document.querySelectorAll('.preview-profile').forEach(el=>el.textContent=$('profile-name').value);});
  function uniqueName(){for(let n=1;;n++){const name='新情景'+n;if(!config.profiles.some(p=>p.name===name))return name;}}
  $('new-profile').addEventListener('click',()=>{cancelRecording(false);editing=addProfile(config,uniqueName());changed();render();});
  $('copy-profile').addEventListener('click',()=>{cancelRecording(false);editing=addProfile(config,uniqueName(),editing);changed();render();});
  $('delete-profile').addEventListener('click',()=>{if(!confirm('删除情景“'+config.profiles[editing].name+'”？保存后生效。'))return;cancelRecording(false);removeProfile(config,editing);editing=Math.min(editing,config.profiles.length-1);changed();render();});
  $('activate-profile').addEventListener('click',()=>{config.active_profile=editing;changed();render();status('已设为当前情景，保存后生效。');});
  async function request(path,body){const response=await fetch(path,{method:'POST',headers:{'X-Session':session,'Content-Type':'application/json'},body:body===undefined?undefined:JSON.stringify(body),signal:AbortSignal.timeout(8000)});if(!response.ok)throw Error(await response.text());return response.json();}
  async function operation(fn){if(busy||!config)return;cancelRecording(false);busy=true;render();try{await fn();}catch(e){status('操作未完成：'+e.message,true);}finally{busy=false;render();}}
  $('save-home').addEventListener('click',()=>operation(async()=>{
    validateHome(home);await request('/api/home',home);homeDirty=false;$('home-saved').textContent='工牌已保存';
    status('工牌设置已保存，退出热点后可在首页查看。');
  }));
  $('sync-time').addEventListener('click',()=>operation(async()=>{
    await request('/api/time',timePayload());$('clock-status').textContent='已同步当前手机／电脑时间。配置 Wi-Fi 后，下次开机可自动校时。';
    status(homeDirty?'校时成功。工牌和时区仍有修改，请保存工牌设置。':'校时成功，退出热点后可查看时间。');
  }));
  function wifiRender(password=''){
    $('wifi-ssid').value=wifi.ssid;$('wifi-password').value=password;
    $('wifi-password').placeholder=wifi.has_password?'密码已保存，留空保持不变':'无密码网络可留空';
    $('wifi-saved').textContent=wifi.ssid?'已保存自动校时网络':'尚未配置自动校时';
    $('wifi-status').textContent=(wifi.clock_valid?'设备已有有效时间。':'设备尚未校时。')+'配置热点开启时，自动校时暂停；退出热点后尝试连接。';
  }
  async function wifiLoad(password=''){
    const r=await fetch('/api/wifi',{headers:{'X-Session':session},signal:AbortSignal.timeout(8000)});
    if(!r.ok)throw Error('读取 Wi-Fi 设置失败');wifi=await r.json();wifiDirty=false;wifiRender(password);render();
  }
  $('wifi-show-password').addEventListener('change',()=>{
    $('wifi-password').type=$('wifi-show-password').checked?'text':'password';
  });
  function wifiChanged(){
    wifiDirty=true;++wifiRevision;$('wifi-saved').textContent='Wi-Fi 有未保存的修改';
    $('wifi-test-status').textContent='网络或密码已修改，请重新测试。';$('wifi-test-status').className='';
  }
  for(const id of ['wifi-ssid','wifi-password'])$(id).addEventListener('input',wifiChanged);
  $('wifi-networks').addEventListener('change',()=>{
    if(!$('wifi-networks').value)return;$('wifi-ssid').value=$('wifi-networks').value;wifiChanged();
  });
  function wifiCandidate(){
    const ssid=$('wifi-ssid').value,password=$('wifi-password').value;
    const network=wifiNetworks.find(n=>n.ssid===ssid);
    const keep=ssid===wifi.ssid&&wifi.has_password&&!password&&network?.secured!==false;
    if(network?.secured&&!password&&!keep)throw Error('此网络需要密码，请先填写。');
    return wifiPayload(ssid,password,keep);
  }
  async function wifiJobRead(){
    const r=await fetch('/api/wifi/job',{headers:{'X-Session':session},signal:AbortSignal.timeout(3000)});
    if(!r.ok)throw Error(await r.text());return r.json();
  }
  function wifiScanRender(job){
    wifiNetworks=job.networks||[];const first=document.createElement('option');first.value='';first.textContent='请选择网络，也可手动填写';
    $('wifi-networks').replaceChildren(first);
    for(const n of wifiNetworks){const o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+' · '+n.rssi+' dBm'+(n.secured?'':' · 无密码');$('wifi-networks').append(o);}
    $('wifi-networks').disabled=!wifiNetworks.length;
    $('wifi-scan-status').textContent=wifiNetworks.length?'找到 '+wifiNetworks.length+' 个网络；选择后填写密码。':'未发现可显示的网络，可重试扫描或手动填写名称。';
  }
  function wifiTestRender(job){
    $('wifi-test-status').textContent=job.ssid+'：'+wifiJobMessage(job);
    $('wifi-test-status').className=!job.busy&&!job.connected?'error':'';
  }
  async function wifiJobWait(id,revision){
    const deadline=performance.now()+75000;
    while(performance.now()<deadline){
      let job;
      try{job=await wifiJobRead();}catch(e){
        $('wifi-test-status').textContent='暂时无法读取结果，请保持连接 Passport 热点，正在重试…';
        await new Promise(resolve=>setTimeout(resolve,750));continue;
      }
      if(job.id!==id)throw Error('扫描或测试已被另一项操作替换，请重试。');
      if(job.kind===1)$('wifi-scan-status').textContent=wifiJobMessage(job);
      else if(revision===wifiRevision)wifiTestRender(job);
      if(!job.busy){
        if(job.kind===1){if(job.state!==5)throw Error(wifiJobMessage(job));wifiScanRender(job);}
        return job;
      }
      await new Promise(resolve=>setTimeout(resolve,500));
    }
    throw Error('等待结果超时，请重新连接 Passport 热点后重试。');
  }
  async function wifiJobStart(path,body){
    const previous=await wifiJobRead();let id;
    try{id=(await request(path,body)).id;}catch(e){
      // Connecting STA can briefly move the AP channel. Recover an accepted
      // job if its POST response was lost, without starting a second test.
      const current=await wifiJobRead();
      if(current.id===previous.id||current.kind!==(body?2:1)||(body&&current.ssid!==body.ssid))throw e;
      id=current.id;
    }
    return wifiJobWait(id,wifiRevision);
  }
  $('scan-wifi').addEventListener('click',()=>operation(async()=>{
    $('wifi-scan-status').textContent='正在扫描附近 Wi-Fi…';await wifiJobStart('/api/wifi/scan');
  }));
  $('test-wifi').addEventListener('click',()=>operation(async()=>{
    const candidate=wifiCandidate();if(!candidate.ssid)throw Error('请先选择或填写网络名称。');
    $('wifi-test-status').textContent='正在开始连接测试…';await wifiJobStart('/api/wifi/test',candidate);
  }));
  $('save-wifi').addEventListener('click',()=>operation(async()=>{
    const candidate=wifiCandidate(),password=$('wifi-password').value;
    await request('/api/wifi',candidate);await wifiLoad(password);
    const ssid=candidate.ssid;
    $('wifi-status').textContent=ssid?'网络已保存，退出热点后开始自动校时。':'自动校时网络已清除。';
    status('Wi-Fi 设置已保存。');
  }));
  $('clear-wifi').addEventListener('click',()=>operation(async()=>{
    await request('/api/wifi',wifiPayload('',''));await wifiLoad();++wifiRevision;$('wifi-test-status').textContent='网络设置已清除，可选择新网络测试。';status('已清除网络设置；仍可手动同步时间。');
  }));
  function download(kind){
    const text=exportFile(config,kind,editing),url=URL.createObjectURL(new Blob([text],{type:'application/json'}));
    const link=element('a');link.href=url;link.download='passport-keys-'+(kind==='scenario'?'scenario':'all')+'.json';
    document.body.append(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);
    status(kind==='scenario'?'已导出当前编辑的情景。':'已导出全部情景及当前选择。');
  }
  function importMode(){
    const replace=!$('import-collision').hidden&&$('import-mode').value==='replace';
    $('import-name').disabled=replace;
    $('import-warning').textContent=pendingImport?.kind==='configuration'?
      '全量导入会替换编辑区的全部情景及当前选择，包括未保存的修改。检查后点击“保存到 Passport”才写入设备。':
      replace?'将替换编辑区的同名情景。保存后生效。':'将追加为一个情景，设备当前选择保持原样。保存后生效。';
  }
  function reviewImport(payload){
    pendingImport=payload;$('import-error').textContent='';
    const profiles=payload.config.profiles,steps=profiles.reduce((n,p)=>n+p.bindings.reduce((n,b)=>n+b.steps.length,0),0);
    const single=payload.kind==='scenario',name=profiles[0].name;
    $('import-summary').textContent=single?'情景“'+name+'” · 11 个动作 · '+steps+' 步':
      profiles.length+' 个情景 · '+steps+' 步 · 当前：'+profiles[payload.config.active_profile].name;
    const collision=single&&config.profiles.some(p=>p.name===name);
    $('import-collision').hidden=!collision;$('import-name-label').hidden=!single;$('import-mode').value='rename';
    let candidate=name;
    if(collision)for(let n=1;;n++){
      const suffix=String(n);candidate=Array.from(name).slice(0,16-suffix.length).join('')+suffix;
      if(!config.profiles.some(p=>p.name===candidate))break;
    }
    $('import-name').value=candidate;importMode();$('import-review').showModal();
  }
  $('export-profile').addEventListener('click',()=>operation(()=>download('scenario')));
  $('export-all').addEventListener('click',()=>operation(()=>download('configuration')));
  $('import-file').addEventListener('click',()=>{if(!busy&&config)$('import-input').click();});
  $('import-input').addEventListener('change',()=>{
    const file=$('import-input').files[0];$('import-input').value='';if(!file)return;
    operation(async()=>{if(file.size>fileLimit)throw Error('文件过大，最多 64 KiB。');reviewImport(parseImport(await file.text()));});
  });
  $('import-mode').addEventListener('change',importMode);
  $('import-cancel').addEventListener('click',()=>$('import-review').close());
  $('import-review').addEventListener('close',()=>{pendingImport=null;});
  $('import-apply').addEventListener('click',()=>{
    if(!pendingImport||busy||!config)return;
    try{
      const mode=$('import-collision').hidden?'rename':$('import-mode').value;
      const next=applyImport(config,pendingImport,{mode,name:$('import-name').value});
      cancelRecording(false);config=next.config;editing=next.editing;changed();render();$('import-review').close();
      status('已加载到编辑区。请检查映射和当前情景，再点击“保存到 Passport”。');
    }catch(e){$('import-error').textContent=e.message;}
  });
  $('save').addEventListener('click',()=>operation(async()=>{validate(config);await request('/api/config',config);dirty=false;$('dirty').textContent='已保存到设备';status('保存成功。退出热点后，在设备进入“开始控制”即可使用。');}));
  $('defaults').addEventListener('click',()=>{if(!config||!confirm('恢复默认配置会移除当前编辑的全部情景。保存后生效，是否继续？'))return;cancelRecording(false);config=defaults();editing=0;changed();render();});
  $('exit').addEventListener('click',()=>{if((dirty||homeDirty||wifiDirty)&&!confirm('尚有未保存的修改，仍要退出热点？'))return;operation(async()=>{await request('/api/exit');status('热点正在关闭。设备返回设置；长按 OK 进入首页，再选择“开始控制”。');config=null;dirty=homeDirty=wifiDirty=false;$('dirty').textContent='会话已结束';});});
  $('forget').addEventListener('click',()=>{if((dirty||homeDirty||wifiDirty)&&!confirm('尚有未保存修改，清除配对将结束网页会话，仍要继续？'))return;if(!confirm('设备将退出热点并显示确认页，请在设备上确认清除配对。'))return;operation(async()=>{await request('/api/forget');status('请在 Passport 确认清除配对。热点正在关闭。');config=null;dirty=homeDirty=wifiDirty=false;$('dirty').textContent='请在设备确认';});});
  window.addEventListener('beforeunload',e=>{if(dirty||homeDirty||wifiDirty){e.preventDefault();e.returnValue='';}});
  Promise.all(['/api/config','/api/home'].map(async path=>{const r=await fetch(path,{signal:AbortSignal.timeout(8000)});if(!r.ok)throw Error('读取配置失败');return r.json();})).then(([data,identity])=>{validate(data.config,true);validateHome(identity,true);config=data.config;home=identity;session=data.session;editing=config.active_profile;$('dirty').textContent='已读取设备配置';$('home-saved').textContent='已读取工牌';homeRender();render();wifiLoad().then(async()=>{const job=await wifiJobRead();if(job.kind===1&&job.state===5)wifiScanRender(job);if(job.busy)await operation(()=>wifiJobWait(job.id,wifiRevision));else if(job.kind===2&&job.state!==0)wifiTestRender(job);}).catch(e=>{$('wifi-saved').textContent=e.message;status(e.message+'，请刷新重试。',true);});}).catch(e=>{status('无法读取配置：'+e.message+'。请确认已连接 Passport 热点，然后刷新。',true);$('dirty').textContent='连接失败';});
})(globalThis);
