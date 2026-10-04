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
  const empty=()=>({type:0,modifiers:0,keys:[],consumer:0});
  function defaults(){const bindings=Array.from({length:8},empty);[82,81,40].forEach((k,i)=>{bindings[i*3]={type:1,modifiers:0,keys:[k],consumer:0};if(i<2)bindings[i*3+2]=clone(bindings[i*3]);});return{version:1,bindings};}
  function capture(events){
    let modifiers=0;const pressed=new Set();
    for(const e of events){
      if(e.repeat)continue;
      if(e.isComposing||e.key==='Process'||e.key==='Dead'||e.key==='Fn'||e.key==='FnLock')throw Error('该按键无法录制，请手动选择标准按键。');
      let m=modCodes.indexOf(e.code);
      if(m>=0)modifiers|=1<<m;
      else {const k=keys.find(k=>k.code===e.code);if(!k)throw Error('无法识别该按键，请手动配置。');pressed.add(k.usage);}
      if(e.ctrlKey&&!(modifiers&17))modifiers|=1;
      if(e.shiftKey&&!(modifiers&34))modifiers|=2;
      if(e.altKey&&!(modifiers&68))modifiers|=4;
      if(e.metaKey&&!(modifiers&136))modifiers|=8;
    }
    if(pressed.size>6)throw Error('最多支持六个普通按键。');
    if(!pressed.size&&!modifiers)throw Error('没有收到按键，请手动选择。');
    return{type:1,modifiers,keys:[...pressed],consumer:0};
  }
  root.Keypad={keys,modCodes,media,defaults,capture};
  if(typeof document==='undefined')return;
  let config, session='', selected=0, dirty=false, busy=false, recording=null;
  const $=id=>document.getElementById(id);
  function status(text,error=false){$('status').textContent=text;$('status').className=error?'error':'';}
  function changed(){dirty=true;$('dirty').textContent='有未保存的修改';}
  function element(tag,text,cls){const el=document.createElement(tag);if(text!==undefined)el.textContent=text;if(cls)el.className=cls;return el;}
  function button(text,cls,fn){const b=element('button',text,cls);b.type='button';b.addEventListener('click',fn);return b;}
  function summary(a){if(!a.type)return['未启用'];if(a.type===2)return[media.find(m=>m[0]===a.consumer)?.[1]||'媒体键'];return[...modLabels.filter((_,i)=>a.modifiers&(1<<i)),...a.keys.map(v=>keys.find(k=>k.usage===v)?.label||'未知按键')];}
  function select(options,value,fn){const s=element('select');options.forEach(([v,label])=>{const o=element('option',label);o.value=v;s.append(o);});s.value=value;s.addEventListener('change',()=>fn(Number(s.value)));return s;}
  function render(){
    if(!config)return;
    $('actions').replaceChildren();
    ['单击','双击','长按'].forEach((name,gesture)=>{
      const slot=selected*3+gesture, fixed=slot===8;
      const card=element('article',undefined,'action'+(fixed?' fixed':''));
      const head=element('div',undefined,'action-head');head.append(element('h2',name),element('span',`0${gesture+1} / ${fixed?'SYSTEM':'ACTION'}`,'index'));card.append(head);
      const binding=element('div',undefined,'binding');card.append(binding);
      if(fixed){binding.textContent='返回上一层';card.append(element('p','OK 长按 500ms 为全局返回操作，始终保留，不会向电脑发送按键。'));$('actions').append(card);return;}
      const a=config.bindings[slot];summary(a).forEach(label=>binding.append(element('kbd',label)));
      const type=select([[0,'关闭此动作'],[1,'按键 / 组合键'],[2,'媒体控制']],a.type,v=>{cancelRecording(false);config.bindings[slot]=v===1?{type:1,modifiers:0,keys:[40],consumer:0}:v===2?{type:2,modifiers:0,keys:[],consumer:205}:empty();changed();render();});type.className='type';type.setAttribute('aria-label',name+'动作类型');card.append(type);
      if(a.type===2){const ms=select(media,a.consumer,v=>{a.consumer=v;changed();render();});ms.className='type';ms.setAttribute('aria-label',name+'媒体动作');card.append(ms);}
      if(a.type===1){
        const rec=button('点击录制组合键','record',()=>{if(recording?.slot===slot){cancelRecording();return;}cancelRecording(false);recording={slot,events:[],down:new Set(),button:rec,timer:null};rec.textContent='请按组合键… 再次点击取消';rec.classList.add('recording');status('录制中：按下组合键后松开。若系统拦截，请取消并手动配置。');});card.append(rec);
        const manual=element('details',undefined,'manual');manual.append(element('summary','手动选键与修饰键'));
        const mods=element('div',undefined,'modifiers');modLabels.forEach((name,i)=>{const label=element('label');const check=element('input');check.type='checkbox';check.checked=!!(a.modifiers&(1<<i));check.addEventListener('change',()=>{a.modifiers^=1<<i;changed();binding.replaceChildren(...summary(a).map(x=>element('kbd',x)));});label.append(check,document.createTextNode(name));mods.append(label);});manual.append(mods);
        a.keys.forEach((v,i)=>{const row=element('div',undefined,'key-choice');const choice=select(keys.map(k=>[k.usage,k.label]),v,value=>{if(a.keys.includes(value)){status('组合键不能包含重复按键。',true);choice.value=v;return;}a.keys[i]=value;changed();render();});choice.setAttribute('aria-label',name+'第'+(i+1)+'个按键');row.append(choice,button('移除','',()=>{a.keys.splice(i,1);changed();render();}));manual.append(row);});
        if(a.keys.length<6)manual.append(button('+ 添加普通按键','add',()=>{a.keys.push(keys.find(k=>!a.keys.includes(k.usage)).usage);changed();render();}));
        card.append(manual);
      }
      $('actions').append(card);
    });
  }
  function cancelRecording(message=true){if(!recording)return;clearTimeout(recording.timer);recording.button.textContent='点击录制组合键';recording.button.classList.remove('recording');recording=null;if(message)status('录制已取消，原映射未修改。');}
  function finishRecording(){if(!recording)return;const saved=recording;try{config.bindings[saved.slot]=capture(saved.events);cancelRecording(false);changed();render();status('已录制，请保存到 Passport。');}catch(e){cancelRecording(false);status(e.message,true);}}
  document.addEventListener('keydown',e=>{if(!recording)return;e.preventDefault();e.stopPropagation();if(e.repeat)return;clearTimeout(recording.timer);recording.events.push({code:e.code,key:e.key,ctrlKey:e.ctrlKey,altKey:e.altKey,shiftKey:e.shiftKey,metaKey:e.metaKey,isComposing:e.isComposing});recording.down.add(e.code);});
  document.addEventListener('keyup',e=>{if(!recording)return;e.preventDefault();e.stopPropagation();recording.down.delete(e.code);if(!recording.down.size)finishRecording();else if(modCodes.includes(e.code))recording.timer=setTimeout(finishRecording,120);});
  window.addEventListener('blur',()=>cancelRecording());
  document.addEventListener('visibilitychange',()=>{if(document.hidden)cancelRecording();});
  document.querySelectorAll('.tab').forEach(tab=>tab.addEventListener('click',()=>{cancelRecording(false);selected=Number(tab.dataset.key);document.querySelectorAll('.tab').forEach(t=>{t.classList.toggle('active',t===tab);t.setAttribute('aria-selected',String(t===tab));});render();}));
  async function request(path,body){const response=await fetch(path,{method:'POST',headers:{'X-Session':session,'Content-Type':'application/json'},body:body===undefined?undefined:JSON.stringify(body),signal:AbortSignal.timeout(8000)});if(!response.ok)throw Error(await response.text());return response.json();}
  async function operation(fn){if(busy||!config)return;cancelRecording(false);busy=true;document.querySelectorAll('button,select,input').forEach(b=>b.disabled=true);try{await fn();}catch(e){status('操作未完成：'+e.message,true);}finally{busy=false;document.querySelectorAll('button,select,input').forEach(b=>b.disabled=false);}}
  $('save').addEventListener('click',()=>operation(async()=>{if(config.bindings.some(a=>a.type===1&&!a.keys.length&&!a.modifiers))throw Error('键盘动作至少需要一个按键或修饰键。');await request('/api/config',config);dirty=false;$('dirty').textContent='已保存到设备';status('保存成功。退出热点，再进入指令模式即可使用。');}));
  $('defaults').addEventListener('click',()=>{if(!config||!confirm('恢复默认映射？保存后才会应用到设备。'))return;cancelRecording(false);config=defaults();changed();render();});
  $('exit').addEventListener('click',()=>{if(dirty&&!confirm('尚有未保存的修改，仍要退出热点？'))return;operation(async()=>{await request('/api/exit');status('热点正在关闭。设备返回设置页；长按 OK 返回主菜单，再进入指令模式。');config=null;$('dirty').textContent='会话已结束';});});
  $('forget').addEventListener('click',()=>{if(!confirm('清除配对记录？设备将关闭热点并显示确认页，请在设备上确认。'))return;operation(async()=>{await request('/api/forget');status('请在 Passport 确认清除配对。热点正在关闭。');config=null;$('dirty').textContent='请在设备确认';});});
  window.addEventListener('beforeunload',e=>{if(dirty){e.preventDefault();e.returnValue='';}});
  fetch('/api/config',{signal:AbortSignal.timeout(8000)}).then(async r=>{if(!r.ok)throw Error('读取配置失败');const data=await r.json();config=data.config;session=data.session;$('dirty').textContent='已读取设备配置';render();}).catch(e=>{status('无法连接设备：'+e.message+'。请确认已连接 Passport 热点，然后刷新。',true);$('dirty').textContent='连接失败';});
})(globalThis);
