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
    if(kind==='configuration'){
      validateSettings(c,true);
      return JSON.stringify({format:fileFormat,version:6,kind,settings:clone(c)});
    }
    if(kind!=='scenario')throw Error('导出类型无效。');
    validate(c,true);
    if(!Number.isInteger(index)||index<0||index>=c.profiles.length)throw Error('请选择要导出的情景。');
    return JSON.stringify({format:fileFormat,version:5,kind,profile:clone(c.profiles[index])});
  }
  function parseImport(text){
    if(typeof text!=='string'||utf8(text)>fileLimit)throw Error('文件过大，最多 64 KiB。');
    let file;try{file=JSON.parse(text);}catch{throw Error('文件不是有效的 JSON。');}
    if(file?.version===6){
      fields(file,['format','version','kind','settings']);
      if(file.format!==fileFormat||file.kind!=='configuration')throw Error('完整配置文件格式不匹配。');
      validateSettings(file.settings);
      return {kind:'settings',config:clone(file.settings.config),settings:clone(file.settings)};
    }
    if(file?.version!==5)throw Error('不支持该配置文件版本。');
    fields(file,['format','version','kind','profile']);
    if(file.format!==fileFormat||file.kind!=='scenario')throw Error('单情景文件格式不匹配。');
    const config={version:5,active_profile:0,profiles:[clone(file.profile)]};
    validate(config);return {kind:'scenario',config};
  }
  function applyImport(current,payload,options={}){
    if(payload.kind==='settings'){
      fields(payload,['kind','config','settings']);validateSettings(payload.settings);
      return {config:clone(payload.settings.config),editing:payload.settings.config.active_profile,settings:clone(payload.settings)};
    }
    fields(payload,['kind','config']);validate(payload.config);
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
    const messages=['尚未测试','正在准备','正在扫描附近 Wi-Fi','正在连接测试网络，请稍候','Wi-Fi 已连接','扫描完成','连接成功，已取得 IP 地址。请点击“保存全部设置”。','认证失败，请检查密码或路由器设置。','未找到网络，请检查名称、距离和 2.4 GHz 设置。','连接超时，请检查路由器和信号。','Wi-Fi 连接测试完成。','连接或扫描失败，请重试。','网络安全方式不兼容，请检查路由器设置。','正在恢复配置前的 Wi-Fi，请稍候。'];
    return messages[job.state]||'设备返回了未知结果，请重试。';
  }
  const settingsDefaults=()=>({version:1,home:homeDefaults(),wifi:{version:1,ssid:'',password:''},config:defaults()});
  function validateSettings(s,stored=false){
    fields(s,['version','home','wifi','config']);
    if(s.version!==1)throw Error('完整配置版本无效。');
    validateHome(s.home,stored);validate(s.config,stored);
    fields(s.wifi,['version','ssid','password']);
    if(s.wifi.version!==1)throw Error('Wi-Fi 配置版本无效。');
    wifiPayload(s.wifi.ssid,s.wifi.password);
    if(utf8(JSON.stringify(s))>33792)throw Error('完整配置超过网页传输容量，请减少步骤或文字。');
    return true;
  }
  async function saveSettings(settings,write,onSaved=()=>{}){
    validateSettings(settings);
    const snapshot=clone(settings);
    try{await write('/api/settings',snapshot);}catch(e){throw Error('完整配置尚未确认保存，请重试。'+e.message);}
    onSaved('/api/settings',snapshot);
    return true;
  }
  root.Keypad={settingsDefaults,validateSettings,saveSettings,wifiJobMessage,wifiPayload,homeDefaults,validateHome,timePayload,exportFile,parseImport,applyImport,fileLimit,keys,modCodes,media,defaults,capture,actionLabel,sequenceLabel,titleLabel,capacity,validate,validateText,addStep,removeStep,moveStep,addProfile,removeProfile};
  if(typeof document==='undefined')return;
  let pendingImport=null;
  let config,session='',selected=0,gesture=0,editing=0,dirty=false,busy=false,recording=null,lanWifi=false;
  let home,homeDirty=false,wifi,wifiDirty=false,wifiNetworks=[],wifiRevision=0,wifiSavedPassword='',wifiKeepAllowed=true;
  const $=id=>document.getElementById(id);
  function status(text,error=false){$('status').textContent=text;$('status').className=error?'error':'';}
  function metrics(){if(!config)return;const used=capacity(config);$('capacity').textContent=used+' / 6144 B · '+config.profiles.length+' / 8 情景';$('capacity').classList.toggle('error',used>6144);$('current-profile').textContent='当前：'+config.profiles[config.active_profile].name;}
  function dirtyStatus(){
    const names=[];if(homeDirty)names.push('工牌');if(wifiDirty)names.push('Wi-Fi');if(dirty)names.push('按键情景');
    $('dirty').textContent=names.length?'未保存：'+names.join('、'):'全部设置已保存';
    $('save-state').classList.toggle('dirty',!!names.length);
  }
  function changed(){dirty=true;dirtyStatus();metrics();}
  function homeChanged(){homeDirty=true;$('home-saved').textContent='工牌有未保存的修改';dirtyStatus();}
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
  const gestureNames=['单击','双击','三击','长按'];
  const binding=()=>config.profiles[editing].bindings[selected*4+gesture];
  function icon(name){const svg=document.createElementNS('http://www.w3.org/2000/svg','svg'),use=document.createElementNS(svg.namespaceURI,'use');svg.setAttribute('aria-hidden','true');use.setAttribute('href','#i-'+name);svg.append(use);return svg;}
  function tool(label,name,fn){const b=button('', 'icon-btn step-tool',fn);b.setAttribute('aria-label',label);b.title=label;b.append(icon(name));return b;}
  function wrappedSelect(options,value,fn,cls,label){const wrap=element('div',undefined,'select-wrap '+cls),choice=select(options,value,fn);choice.className='control';choice.setAttribute('aria-label',label);wrap.append(choice);return wrap;}
  let manualEdit=null;
  function manualRender(){
    const a=manualEdit.draft,body=$('manual-body');body.replaceChildren();
    body.append(element('div',actionLabel(a)||'请选择按键','chord-preview'));
    const mods=element('div',undefined,'modifier-grid');
    for(let side=0;side<2;side++){
      const column=element('div',undefined,'modifier-column');column.append(element('h4',side?'右侧修饰键':'左侧修饰键'));
      const choices=[[side?'右 Cmd/Ctrl':'左 Cmd/Ctrl','logical_modifiers',1<<side],...modLabels.slice(side*4,side*4+4).map((label,i)=>[label,'modifiers',1<<(side*4+i)])];
      const options=element('div',undefined,'modifier-options');
      for(const [label,field,bit] of choices){
        const wrap=element('label'),input=element('input');input.type='checkbox';input.checked=!!(a[field]&bit);input.setAttribute('aria-label',label);
        input.addEventListener('change',()=>{a[field]=input.checked?a[field]|bit:a[field]&~bit;if(field==='logical_modifiers'&&!a[field])delete a[field];manualRender();});
        wrap.append(input,document.createTextNode(label));options.append(wrap);
      }
      column.append(options);mods.append(column);
    }
    body.append(mods);
    const heading=element('div',undefined,'ordinary-heading');heading.append(element('span','普通按键'),element('small','最多 6 个'));body.append(heading);
    a.keys.forEach((value,i)=>{
      const row=element('div',undefined,'ordinary-row'),wrap=wrappedSelect(keys.map(k=>[k.usage,k.label]),value,next=>{
        if(a.keys.some((k,j)=>j!==i&&k===next)){$('manual-error').textContent='组合键不能包含重复按键。';wrap.firstChild.value=value;return;}
        a.keys[i]=next;manualRender();
      },'key-choice','普通按键 '+(i+1));
      row.append(element('span',String(i+1).padStart(2,'0')),wrap,tool('移除普通按键 '+(i+1),'trash',()=>{a.keys.splice(i,1);manualRender();}));body.append(row);
    });
    const add=button('+ 添加普通按键','add-ordinary',()=>{a.keys.push(keys.find(k=>!a.keys.includes(k.usage)).usage);manualRender();});add.disabled=a.keys.length>=6;body.append(add);
    const empty=!a.keys.length&&!a.modifiers&&!a.logical_modifiers;
    $('manual-apply').disabled=empty;$('manual-error').textContent=empty?'至少选择一个普通按键或修饰键。':'';
  }
  function openManual(index){
    cancelRecording(false);manualEdit={binding:binding(),index,draft:clone(binding().steps[index])};manualRender();$('key-editor').showModal();
  }
  $('manual-cancel').addEventListener('click',()=>$('key-editor').close());
  $('key-editor').addEventListener('close',()=>{manualEdit=null;});
  $('manual-apply').addEventListener('click',()=>{
    if(!manualEdit||busy)return;
    const {binding:b,index,draft}=manualEdit;b.steps[index]=draft;changed();$('key-editor').close();render();
  });
  $('action-title').addEventListener('input',()=>{binding().title=$('action-title').value;changed();});
  $('preview').addEventListener('click',()=>{$('preview-body').replaceChildren(preview(binding(),gestureNames[gesture]));$('preview-dialog').showModal();});
  $('preview-close').addEventListener('click',()=>$('preview-dialog').close());
  $('add-step').addEventListener('click',()=>{try{cancelRecording(false);addStep(binding());changed();render();}catch(e){status(e.message,true);}});
  function render(){
    $('editor').disabled=busy||!config;$('forget').disabled=busy||!config;$('wifi-fields').disabled=busy||!wifi;
    $('save').disabled=busy||!config||!home||!wifi;
    for(const id of ['defaults','exit','import-file','export-toggle','export-all','export-profile'])$(id).disabled=busy||!config;
    if(!config)return;
    metrics();$('profile').replaceChildren(...config.profiles.map((p,i)=>{const o=element('option',p.name+(i===config.active_profile?' · 当前':''));o.value=i;return o;}));$('profile').value=editing;$('profile-name').value=config.profiles[editing].name;
    $('activate-profile').disabled=editing===config.active_profile;$('delete-profile').disabled=config.profiles.length===1;$('new-profile').disabled=$('copy-profile').disabled=config.profiles.length>=8;
    document.querySelectorAll('.tab').forEach(tab=>{
      const key=Number(tab.dataset.key);tab.classList.toggle('active',key===selected);tab.setAttribute('aria-selected',String(key===selected));
      const count=config.profiles[editing].bindings.slice(key*4,key*4+4).filter(b=>b.steps.length).length;tab.querySelector('.key-count').textContent=count+' 个动作';
    });
    document.querySelectorAll('.event').forEach(tab=>{
      const g=Number(tab.dataset.event),slot=selected*4+g;tab.classList.toggle('active',g===gesture);tab.setAttribute('aria-selected',String(g===gesture));tab.classList.toggle('has-action',slot===11||!!config.profiles[editing].bindings[slot].steps.length);
    });
    $('binding-summary').textContent=['上键','下键','确定键'][selected]+' · '+gestureNames[gesture];
    const fixed=selected===2&&gesture===3;$('system-action').hidden=!fixed;$('actions').hidden=fixed;if(fixed)return;
    const b=binding(),name=gestureNames[gesture],slot=selected*4+gesture;
    $('action-title').value=b.title;$('action-title').setAttribute('aria-label',name+'动作标题');$('step-count').textContent=b.steps.length+' / 64 步';
    $('actions').classList.toggle('triple',gesture===2);
    $('steps').replaceChildren();document.querySelector('.table-head').hidden=!b.steps.length;
    let note=$('gesture-note');if(!note){note=element('p',undefined,'gesture-note');note.id='gesture-note';$('actions').insertBefore(note,$('steps'));}
    note.hidden=gesture!==3;note.textContent='上下键长按 200ms 触发；只有长按有步骤时，按下立即执行。单步保持到松开，多步完整执行一次。';
    if(!b.steps.length)$('steps').append(element('div','无。添加步骤后开始配置。','empty'));
    b.steps.forEach((a,index)=>{
      const panel=element('section',undefined,'step');panel.append(element('span',String(index+1).padStart(2,'0'),'step-index mono'));
      panel.append(wrappedSelect([[1,'键盘组合键'],[2,'媒体控制']],a.type,v=>{cancelRecording(false);b.steps[index]=v===1?{...newStep(),delay_ms:a.delay_ms}:{type:2,modifiers:0,keys:[],consumer:205,delay_ms:a.delay_ms};changed();render();},'step-kind',name+'第'+(index+1)+'步类型'));
      if(a.type===2)panel.append(wrappedSelect(media,a.consumer,v=>{a.consumer=v;changed();render();},'media-choice','媒体动作'));
      else{const chord=element('div',undefined,'keys'),label=element('span',undefined,'step-chord');actionLabel(a).split('+').forEach((key,i)=>{if(i)label.append(element('span','+','plus'));label.append(element('kbd',key));});chord.append(label);panel.append(chord);}
      const delay=element('label',undefined,'step-delay'),input=element('input');input.type='number';input.min=0;input.max=5000;input.step=1;input.value=a.delay_ms;input.disabled=index===b.steps.length-1;input.setAttribute('aria-label','第'+(index+1)+'步后等待毫秒');input.addEventListener('input',()=>{a.delay_ms=Number(input.value);changed();});
      delay.append(element('span',index===b.steps.length-1?'最后一步':'释放后等待','delay-label'),input,document.createTextNode('ms'));delay.title=index===b.steps.length-1?'最后一步，无额外等待':'释放后等待';panel.append(delay);
      const tools=element('div',undefined,'step-actions');
      if(a.type===1){
        tools.append(button('手动选键','record chord-button manual',()=>openManual(index)));
        const rec=button('录制','record chord-button',()=>{if(recording?.slot===slot&&recording.index===index){cancelRecording();return;}cancelRecording(false);recording={slot,index,events:[],down:new Set(),button:rec,timer:null};rec.textContent='取消录制';rec.classList.add('recording');status('按下一组组合键后松开。多组操作请分别添加步骤；系统拦截时可手动配置。');});tools.append(rec);
      }
      const up=tool('上移第'+(index+1)+'步','up',()=>{cancelRecording(false);moveStep(b,index,-1);changed();render();});up.disabled=index===0;
      const down=tool('下移第'+(index+1)+'步','down',()=>{cancelRecording(false);moveStep(b,index,1);changed();render();});down.disabled=index===b.steps.length-1;
      const del=tool('删除第'+(index+1)+'步','trash',()=>{cancelRecording(false);removeStep(b,index);changed();render();});del.classList.add('delete');tools.append(up,down,del);panel.append(tools);$('steps').append(panel);
    });
    $('add-step').disabled=b.steps.length>=64;
  }
  function cancelRecording(message=true){if(!recording)return;clearTimeout(recording.timer);recording.button.textContent='录制';recording.button.classList.remove('recording');recording=null;if(message)status('录制已取消，原步骤未修改。');}
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
  function wifiRender(password=''){
    $('wifi-ssid').value=wifi.ssid;$('wifi-password').value=password;
    $('wifi-password').placeholder=wifi.has_password?'密码已保存，留空保持不变':'无密码网络可留空';
    $('wifi-saved').textContent=wifi.ssid?'已保存自动校时网络':'尚未配置自动校时';
    $('wifi-status').textContent=(wifi.clock_valid?'设备已有有效时间。':'设备尚未校时。')+(lanWifi?'当前通过 WiFi 配置；支持扫描。连接测试会暂时断开页面，完成后恢复原网络；请勿刷新。若地址变化，请按设备显示的新 IP 重新打开。网络修改在退出配置后生效。':'本机热点开启时，自动校时暂停；退出配置后尝试连接。');
    $('scan-wifi').disabled=$('test-wifi').disabled=false;
  }
  async function wifiLoad(password=''){
    const r=await fetch('/api/wifi',{headers:{'X-Session':session},signal:AbortSignal.timeout(8000)});
    if(!r.ok)throw Error('读取 Wi-Fi 设置失败');wifi=await r.json();wifiDirty=false;wifiRender(password);render();
  }
  $('wifi-show-password').addEventListener('change',()=>{
    const visible=$('wifi-show-password').checked;$('wifi-password').type=visible?'text':'password';
    $('toggle-password').setAttribute('aria-pressed',String(visible));$('toggle-password').setAttribute('aria-label',visible?'隐藏密码':'显示密码');
  });
  $('toggle-password').addEventListener('click',()=>{$('wifi-show-password').checked=!$('wifi-show-password').checked;$('wifi-show-password').dispatchEvent(new Event('change'));});
  function wifiChanged(){
    wifiDirty=true;++wifiRevision;$('wifi-saved').textContent='Wi-Fi 有未保存的修改';dirtyStatus();
    $('wifi-test-status').textContent='网络或密码已修改，请重新测试。';$('wifi-test-status').classList.remove('error');
  }
  for(const id of ['wifi-ssid','wifi-password'])$(id).addEventListener('input',wifiChanged);
  $('wifi-networks').addEventListener('change',()=>{
    if(!$('wifi-networks').value)return;$('wifi-ssid').value=$('wifi-networks').value;wifiChanged();$('scan-results').hidden=true;$('scan-wifi').setAttribute('aria-expanded','false');
  });
  function wifiCandidate(){
    const ssid=$('wifi-ssid').value,password=$('wifi-password').value;
    const network=wifiNetworks.find(n=>n.ssid===ssid);
    const keep=wifiKeepAllowed&&ssid===wifi.ssid&&wifi.has_password&&!password&&network?.secured!==false;
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
    $('scan-results').replaceChildren(...wifiNetworks.map(n=>{
      const row=button('', '',()=>{$('wifi-networks').value=n.ssid;$('wifi-networks').dispatchEvent(new Event('change'));});
      row.append(element('span',n.ssid),element('small',n.rssi+' dBm'+(n.secured?'':' · 无密码')));return row;
    }));
    $('scan-results').hidden=!wifiNetworks.length;$('scan-wifi').setAttribute('aria-expanded',String(!!wifiNetworks.length));
    $('wifi-scan-status').textContent=wifiNetworks.length?'找到 '+wifiNetworks.length+' 个网络；选择后填写密码。':'未发现可显示的网络，可重试扫描或手动填写名称。';
  }
  function wifiTestRender(job){
    $('wifi-test-status').textContent=job.ssid+'：'+wifiJobMessage(job)+(job.restore_failed?' 原网络恢复失败，请查看设备显示的地址，或返回本机热点配置。':'');
    $('wifi-test-status').classList.toggle('error',!job.busy&&(!job.connected||job.restore_failed));
  }
  async function wifiJobWait(id,revision){
    const deadline=performance.now()+135000;
    while(performance.now()<deadline){
      let job;
      try{job=await wifiJobRead();}catch(e){
        $('wifi-test-status').textContent=lanWifi?'测试期间页面暂时断开，正在等待恢复原网络… 若地址变化，请按设备显示的新 IP 重新打开。':'暂时无法读取结果，请保持连接 Passport 热点，正在重试…';
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
    throw Error(lanWifi?'等待结果超时，请检查设备显示的 IP；地址变化时使用新地址打开，或返回本机热点配置。':'等待结果超时，请重新连接 Passport 热点后重试。');
  }
  async function wifiJobStart(path,body){
    const previous=await wifiJobRead();let id;
    try{id=(await request(path,body)).id;}catch(e){
      // A LAN test can interrupt the POST response and the next GET. Recover
      // the accepted job after reconnection; never submit the password twice.
      const deadline=performance.now()+135000;
      while(id===undefined&&performance.now()<deadline){
        let current;
        try{current=await wifiJobRead();}catch(readError){
          $('wifi-test-status').textContent=lanWifi?'正在等待恢复原网络并读取测试结果… 若地址变化，请按设备显示的新 IP 重新打开。':'暂时无法读取结果，请保持连接 Passport 热点，正在重试…';
          await new Promise(resolve=>setTimeout(resolve,750));continue;
        }
        if(current.id===previous.id||current.kind!==(body?2:1)||(body&&current.ssid!==body.ssid))throw e;
        id=current.id;
      }
      if(id===undefined)throw Error(lanWifi?'无法恢复测试结果，请检查设备显示的 IP 或通过本机热点配置。':e.message);
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
  $('clear-wifi').addEventListener('click',()=>operation(()=>{
    $('wifi-ssid').value='';$('wifi-password').value='';wifiChanged();
    $('wifi-test-status').textContent='网络将在保存后清除，也可填写新网络。';
    $('wifi-status').textContent='清除网络待保存。';status('已清空网络填写内容，请点击“保存全部设置”确认清除。');
  }));
  function closeExport(){$('export-menu').hidden=true;$('export-toggle').setAttribute('aria-expanded','false');}
  $('export-toggle').addEventListener('click',()=>{const open=$('export-menu').hidden;$('export-menu').hidden=!open;$('export-toggle').setAttribute('aria-expanded',String(open));});
  document.addEventListener('click',e=>{
    if(!e.target.closest('.file-menu'))closeExport();
    if(!e.target.closest('.network-input')&&!e.target.closest('#scan-wifi')){$('scan-results').hidden=true;$('scan-wifi').setAttribute('aria-expanded','false');}
  });
  document.addEventListener('keydown',e=>{if(e.key==='Escape'){closeExport();$('scan-results').hidden=true;$('scan-wifi').setAttribute('aria-expanded','false');}});
  document.querySelectorAll('.event').forEach(tab=>tab.addEventListener('click',()=>{cancelRecording(false);gesture=Number(tab.dataset.event);render();}));
  const navLinks=[...document.querySelectorAll('.nav a')],navSections=navLinks.map(a=>document.querySelector(a.hash));
  function navUpdate(){const marker=innerWidth<=850?document.querySelector('.sidebar').getBoundingClientRect().bottom+20:150,current=navSections.find(s=>{const r=s.getBoundingClientRect();return r.top<=marker&&r.bottom>marker;})||navSections.find(s=>s.getBoundingClientRect().bottom>marker)||navSections.at(-1);navLinks.forEach(a=>a.classList.toggle('active',a.hash==='#'+current.id));}
  window.addEventListener('scroll',navUpdate,{passive:true});window.addEventListener('resize',navUpdate);navUpdate();
  function download(kind){
    closeExport();
    const text=exportFile(kind==='configuration'?currentSettings():config,kind,editing),url=URL.createObjectURL(new Blob([text],{type:'application/json'}));
    const link=element('a');link.href=url;link.download='passport-keys-'+(kind==='scenario'?'scenario':'all')+'.json';
    document.body.append(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);
    status(kind==='scenario'?'已导出当前编辑的情景。':'已导出工牌、Wi-Fi 与全部按键情景；文件包含 Wi-Fi 密码，请妥善保管。');
  }
  function importMode(){
    const replace=!$('import-collision').hidden&&$('import-mode').value==='replace';
    $('import-name').disabled=replace;
    $('import-warning').textContent=pendingImport?.kind==='settings'?
      '完整配置会替换编辑区的工牌、Wi-Fi 与全部按键情景，包括未保存的修改。文件包含 Wi-Fi 密码。检查后点击“保存全部设置”才写入设备。':
      replace?'将替换编辑区的同名情景。保存后生效。':'将追加为一个情景，设备当前选择保持原样。保存后生效。';
  }
  function reviewImport(payload){
    pendingImport=payload;$('import-error').textContent='';
    const profiles=payload.config.profiles,steps=profiles.reduce((n,p)=>n+p.bindings.reduce((n,b)=>n+b.steps.length,0),0);
    const single=payload.kind==='scenario',name=profiles[0].name;
    $('import-summary').textContent=single?'情景“'+name+'” · 11 个动作 · '+steps+' 步':
      (payload.kind==='settings'?'工牌 + Wi-Fi + ':'')+profiles.length+' 个情景 · '+steps+' 步 · 当前：'+profiles[payload.config.active_profile].name;
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
      cancelRecording(false);config=next.config;editing=next.editing;
      if(next.settings){
        home=next.settings.home;homeRender();homeChanged();
        $('wifi-ssid').value=next.settings.wifi.ssid;$('wifi-password').value=next.settings.wifi.password;wifiKeepAllowed=false;wifiChanged();
      }
      changed();render();$('import-review').close();
      status('已加载到编辑区。请检查映射和当前情景，再点击“保存全部设置”。');
    }catch(e){$('import-error').textContent=e.message;}
  });
  function currentSettings(){
    if(!home||!wifi)throw Error('完整配置尚未读取，请稍后重试。');
    const candidate=wifiCandidate();
    return {version:1,home:clone(home),wifi:{version:1,ssid:candidate.ssid,password:candidate.keep_password?wifiSavedPassword:candidate.password},config:clone(config)};
  }
  $('save').addEventListener('click',()=>operation(async()=>{
    if(!dirty&&!homeDirty&&!wifiDirty){status('没有未保存的修改。');return;}
    const settings=currentSettings(),password=$('wifi-password').value;
    status('正在保存完整配置…');
    await saveSettings(settings,request);
    wifiSavedPassword=settings.wifi.password;wifiKeepAllowed=true;
    wifi={...wifi,ssid:settings.wifi.ssid,has_password:!!settings.wifi.password};
    dirty=homeDirty=wifiDirty=false;$('home-saved').textContent='工牌已保存';wifiRender(password);dirtyStatus();
    $('wifi-status').textContent=settings.wifi.ssid?'网络已保存，退出配置后开始自动校时。':'自动校时网络已清除。';
    status('完整配置已保存到 Passport。退出配置后生效。');
  }));
  $('defaults').addEventListener('click',()=>{
    if(!config||busy||!confirm('恢复默认会重置编辑区的工牌、Wi-Fi 与全部按键情景。保存后生效，是否继续？'))return;
    cancelRecording(false);const settings=settingsDefaults();config=settings.config;home=settings.home;editing=0;
    homeRender();homeChanged();$('wifi-ssid').value='';$('wifi-password').value='';wifiKeepAllowed=false;wifiChanged();changed();render();
  });
  $('exit').addEventListener('click',()=>{if((dirty||homeDirty||wifiDirty)&&!confirm('尚有未保存的修改，仍要退出配置？'))return;operation(async()=>{await request('/api/exit');status('配置正在关闭。设备返回Passport配置；长按 OK 返回设置，再长按进入首页，再选择“开始控制”。');config=null;dirty=homeDirty=wifiDirty=false;$('dirty').textContent='会话已结束';});});
  $('forget').addEventListener('click',()=>{if((dirty||homeDirty||wifiDirty)&&!confirm('尚有未保存修改，清除配对将结束网页会话，仍要继续？'))return;if(!confirm('设备将退出配置并显示确认页，请在设备上确认清除配对。'))return;operation(async()=>{await request('/api/forget');status('请在 Passport 确认清除配对。配置正在关闭。');config=null;dirty=homeDirty=wifiDirty=false;$('dirty').textContent='请在设备确认';});});
  window.addEventListener('beforeunload',e=>{if(dirty||homeDirty||wifiDirty){e.preventDefault();e.returnValue='';}});
  fetch('/api/config',{signal:AbortSignal.timeout(8000)}).then(async r=>{
    if(!r.ok)throw Error('读取设置会话失败');const info=await r.json();session=info.session;lanWifi=info.connection==='wifi';
    const response=await fetch('/api/settings',{headers:{'X-Session':session},signal:AbortSignal.timeout(8000)});
    if(!response.ok)throw Error('读取完整配置失败');const settings=await response.json();validateSettings(settings,true);
    config=settings.config;home=settings.home;wifiSavedPassword=settings.wifi.password;editing=config.active_profile;
    $('dirty').textContent='已读取完整配置';$('home-saved').textContent='已读取工牌';homeRender();
    await wifiLoad(wifiSavedPassword);dirtyStatus();const job=await wifiJobRead();
    if(job.kind===1&&job.state===5)wifiScanRender(job);
    if(job.busy)await operation(()=>wifiJobWait(job.id,wifiRevision));else if(job.kind===2&&job.state!==0)wifiTestRender(job);
  }).catch(e=>{status('无法读取配置：'+e.message+'。请确认仍连接设备所用的网络，然后刷新。',true);$('dirty').textContent='连接失败';});
})(globalThis);
