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
  const empty=()=>({title:'',description:'',steps:[]});
  const newStep=()=>({type:1,modifiers:0,keys:[40],consumer:0,delay_ms:0});
  const modNames=['Control','Shift','Option','Command','RControl','RShift','ROption','RCommand'];
  const usageNames={40:'Enter',41:'Esc',42:'Backspace',43:'Tab',44:'Space',76:'Delete',79:'Right',80:'Left',81:'Down',82:'Up'};
  function actionLabel(a){
    if(a.type===2)return media.find(m=>m[0]===a.consumer)?.[1].replaceAll(' / ','')||'媒体键';
    const names=modNames.filter((_,i)=>a.modifiers&(1<<i));
    for(const v of a.keys){let name;if(v>=4&&v<=29)name=String.fromCharCode(65+v-4);else if(v>=30&&v<=39)name=String((v-29)%10);else if(v>=58&&v<=69)name='F'+(v-57);else if(v>=104&&v<=115)name='F'+(v-104+13);else name=usageNames[v]||'Key'+v.toString(16).toUpperCase().padStart(2,'0');names.push(name);}
    return names.join('+');
  }
  const sequenceLabel=b=>b.steps.length?b.steps.map(actionLabel).join(' → '):'关闭';
  const titleLabel=b=>b.title||(b.steps.length>1?'组合指令':sequenceLabel(b));
  function defaults(){const bindings=Array.from({length:8},empty);[[0,0,41],[2,4,0],[3,0,42],[4,8,4],[5,0,81],[6,0,40]].forEach(([slot,mod,key])=>{bindings[slot].steps=[{type:1,modifiers:mod,keys:key?[key]:[],consumer:0,delay_ms:0}];});bindings[6].title='回车';return{version:2,active_profile:0,profiles:[{name:'默认',bindings}]};}
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
  function validateText(s,max,multiline=false){if(typeof s!=='string'||Array.from(s).length>max)throw Error('名称和标题最多 16 字，描述最多 64 字。');for(const ch of s){const cp=ch.codePointAt(0);if(!supported(cp)&&!(multiline&&cp===10))throw Error('设备不支持字符“'+ch+'”（U+'+cp.toString(16).toUpperCase()+'），请替换。');}}
  function capacity(c){let used=1;const put=s=>{if(s)used+=utf8(s)+1;};for(const p of c.profiles){put(p.name);for(const b of p.bindings){put(b.title);put(b.description);if(b.steps.length){used=(used+1)&~1;used+=b.steps.length*12;}}}return 536+used;}
  function validate(c){
    if(c.version!==2||!c.profiles.length||c.profiles.length>8||!Number.isInteger(c.active_profile)||c.active_profile<0||c.active_profile>=c.profiles.length)throw Error('情景配置无效。');
    const names=new Set();for(const p of c.profiles){validateText(p.name,16);if(!p.name||names.has(p.name))throw Error('情景名称不能为空或重复。');names.add(p.name);if(p.bindings.length!==8)throw Error('每个情景必须有八个动作。');for(const b of p.bindings){validateText(b.title,16);validateText(b.description,64,true);if(b.steps.length>64)throw Error('每个动作最多 64 步。');for(const [i,a] of b.steps.entries()){if(!Number.isInteger(a.delay_ms)||a.delay_ms<0||a.delay_ms>5000||(i===b.steps.length-1&&a.delay_ms!==0))throw Error('步骤间隔必须为 0–5000ms，最后一步为 0。');if(!Number.isInteger(a.modifiers)||a.modifiers<0||a.modifiers>255||!Array.isArray(a.keys)||a.keys.length>6||new Set(a.keys).size!==a.keys.length||a.keys.some(k=>!Number.isInteger(k)||!keys.some(v=>v.usage===k)))throw Error('组合键无效或包含重复按键。');if(a.type===1){if(a.consumer!==0||(!a.modifiers&&!a.keys.length))throw Error('键盘步骤至少需要一个按键或修饰键。');}else if(a.type===2){if(a.modifiers||a.keys.length||!media.some(m=>m[0]===a.consumer))throw Error('媒体步骤无效。');}else throw Error('步骤类型无效。');}}}
    if(capacity(c)>6144)throw Error('配置超过 6 KiB，请减少步骤或文字。');if(utf8(JSON.stringify(c))>32768)throw Error('配置超过网页传输容量，请减少步骤。');return true;
  }
  function addStep(b,a=newStep()){if(b.steps.length>=64)throw Error('每个动作最多 64 步。');if(b.steps.length)b.steps.at(-1).delay_ms=80;b.steps.push({...clone(a),delay_ms:0});}
  function removeStep(b,index){b.steps.splice(index,1);if(b.steps.length)b.steps.at(-1).delay_ms=0;}
  function moveStep(b,index,delta){const to=index+delta;if(to<0||to>=b.steps.length)return;const last=b.steps.at(-1);[b.steps[index],b.steps[to]]=[b.steps[to],b.steps[index]];if(b.steps.at(-1)!==last)last.delay_ms=80;b.steps.at(-1).delay_ms=0;}
  function addProfile(c,name,copy=-1){if(c.profiles.length>=8)throw Error('最多 8 个情景。');validateText(name,16);if(!name||c.profiles.some(p=>p.name===name))throw Error('情景名称不能为空或重复。');c.profiles.push(copy<0?{name,bindings:Array.from({length:8},empty)}:{...clone(c.profiles[copy]),name});return c.profiles.length-1;}
  function removeProfile(c,index){if(c.profiles.length===1)throw Error('至少保留一个情景。');c.profiles.splice(index,1);if(c.active_profile===index)c.active_profile=0;else if(c.active_profile>index)--c.active_profile;}
  root.Keypad={keys,modCodes,media,defaults,capture,actionLabel,sequenceLabel,titleLabel,capacity,validate,validateText,addStep,removeStep,moveStep,addProfile,removeProfile};
  if(typeof document==='undefined')return;
  let config,session='',selected=0,editing=0,dirty=false,busy=false,recording=null;
  const $=id=>document.getElementById(id);
  function status(text,error=false){$('status').textContent=text;$('status').className=error?'error':'';}
  function metrics(){if(!config)return;const used=capacity(config);$('capacity').textContent=used+' / 6144 B · '+config.profiles.length+' / 8 情景';$('capacity').classList.toggle('error',used>6144);$('current-profile').textContent='当前：'+config.profiles[config.active_profile].name;}
  function changed(){dirty=true;$('dirty').textContent='有未保存的修改';metrics();}
  function element(tag,text,cls){const el=document.createElement(tag);if(text!==undefined)el.textContent=text;if(cls)el.className=cls;return el;}
  function button(text,cls,fn){const b=element('button',text,cls);b.type='button';b.addEventListener('click',()=>{try{fn();}catch(e){status(e.message,true);}});return b;}
  function select(options,value,fn){const s=element('select');for(const [v,label] of options){const o=element('option',label);o.value=v;s.append(o);}s.value=value;s.addEventListener('change',()=>fn(Number(s.value)));return s;}
  function preview(b){const frame=element('div',undefined,'device-preview');frame.setAttribute('aria-label','设备详情预览');frame.append(element('div',config.profiles[editing].name,'preview-profile'),element('div','--%','preview-battery'),element('div','蓝牙已连接','preview-state'),element('div',titleLabel(b),'preview-title'),element('div',sequenceLabel(b),'preview-keys'),element('div',b.description,'preview-description'),element('div','已完成','preview-progress'));return frame;}
  function metadata(card,b,name){let frame;for(const [field,label,max] of [['title','动作标题',16],['description','描述',64]]){const wrap=element('label',label,'text-field');const input=element(field==='description'?'textarea':'input');input.value=b[field];input.maxLength=max;input.setAttribute('aria-label',name+label);if(field==='title')input.placeholder='留空则自动命名';input.addEventListener('input',()=>{b[field]=input.value;changed();if(frame){frame.querySelector('.preview-title').textContent=titleLabel(b);frame.querySelector('.preview-description').textContent=b.description;}});wrap.append(input);card.append(wrap);}const details=element('details',undefined,'preview-details');details.append(element('summary','查看设备全屏预览'));frame=preview(b);details.append(frame);card.append(details);}
  function render(){
    $('editor').disabled=busy||!config;$('forget').disabled=busy||!config;if(!config)return;
    metrics();$('profile').replaceChildren(...config.profiles.map((p,i)=>{const o=element('option',p.name+(i===config.active_profile?' · 当前':''));o.value=i;return o;}));$('profile').value=editing;$('profile-name').value=config.profiles[editing].name;
    $('activate-profile').disabled=editing===config.active_profile;$('delete-profile').disabled=config.profiles.length===1;$('new-profile').disabled=$('copy-profile').disabled=config.profiles.length>=8;
    $('actions').replaceChildren();
    ['单击','双击','长按'].forEach((name,gesture)=>{
      const slot=selected*3+gesture,fixed=slot===8;const card=element('article',undefined,'action'+(fixed?' fixed':''));
      const head=element('div',undefined,'action-head');head.append(element('h2',name),element('span',`0${gesture+1} / ${fixed?'SYSTEM':'ACTION'}`,'index'));card.append(head);
      if(fixed){card.append(element('div','返回上一层','binding'),element('p','OK 长按 500ms 保留为全局返回。组合执行期间，可随时取消并进入主菜单。'));$('actions').append(card);return;}
      const b=config.profiles[editing].bindings[slot];metadata(card,b,name);
      const heading=element('div',undefined,'steps-heading');heading.append(element('h3','操作步骤'),element('span',b.steps.length+' / 64'));card.append(heading);
      if(!b.steps.length)card.append(element('p','此动作未启用。添加步骤后开始配置。','empty-state'));
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
          const manual=element('details',undefined,'manual');manual.append(element('summary','手动选择按键'));const mods=element('div',undefined,'modifiers');modLabels.forEach((label,i)=>{const wrap=element('label');const input=element('input');input.type='checkbox';input.checked=!!(a.modifiers&(1<<i));input.addEventListener('change',()=>{a.modifiers^=1<<i;changed();chord.textContent=actionLabel(a);card.querySelector('.preview-keys').textContent=sequenceLabel(b);card.querySelector('.preview-title').textContent=titleLabel(b);});wrap.append(input,document.createTextNode(label));mods.append(wrap);});manual.append(mods);
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
  $('save').addEventListener('click',()=>operation(async()=>{validate(config);await request('/api/config',config);dirty=false;$('dirty').textContent='已保存到设备';status('保存成功。退出热点后，在设备进入“开始控制”即可使用。');}));
  $('defaults').addEventListener('click',()=>{if(!config||!confirm('恢复默认配置会移除当前编辑的全部情景。保存后生效，是否继续？'))return;cancelRecording(false);config=defaults();editing=0;changed();render();});
  $('exit').addEventListener('click',()=>{if(dirty&&!confirm('尚有未保存的修改，仍要退出热点？'))return;operation(async()=>{await request('/api/exit');status('热点正在关闭。设备返回设置；长按 OK 进入主菜单，再选择“开始控制”。');config=null;dirty=false;$('dirty').textContent='会话已结束';});});
  $('forget').addEventListener('click',()=>{if(dirty&&!confirm('尚有未保存修改，清除配对将结束网页会话，仍要继续？'))return;if(!confirm('设备将退出热点并显示确认页，请在设备上确认清除配对。'))return;operation(async()=>{await request('/api/forget');status('请在 Passport 确认清除配对。热点正在关闭。');config=null;dirty=false;$('dirty').textContent='请在设备确认';});});
  window.addEventListener('beforeunload',e=>{if(dirty){e.preventDefault();e.returnValue='';}});
  fetch('/api/config',{signal:AbortSignal.timeout(8000)}).then(async r=>{if(!r.ok)throw Error('读取配置失败');const data=await r.json();validate(data.config);config=data.config;session=data.session;editing=config.active_profile;$('dirty').textContent='已读取设备配置';render();}).catch(e=>{status('无法读取配置：'+e.message+'。请确认已连接 Passport 热点，然后刷新。',true);$('dirty').textContent='连接失败';});
})(globalThis);
