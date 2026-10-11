/* All content stays in this page or travels over RTCDataChannel. */
(function () {
  'use strict';
  const P=globalThis.PassportTransferProtocol, $=id=>document.getElementById(id);
  const mobile=/Android|iPhone|iPad|iPod/.test(navigator.userAgent)||(navigator.platform==='MacIntel'&&navigator.maxTouchPoints>1);
  // One page-wide budget includes pending receives and retained download blobs.
  const memoryLimit=1024*1024**2;
  const memoryLimitLabel=(memoryLimit/1024**2)+' MB';
  let ws, self=0, online=false, connecting=false, ended=false, heartbeat, retryTimer, connectDeadline, retries=0, rendering=false, sendingText=false;
  let noticeMessage='', noticeKind='action';
  let wakeRequest=0, wakeAbort;
  const entry=new URLSearchParams(location.search||'');
  let generation=Number(entry.get('generation')),lease=Number(entry.get('lease'));
  globalThis.history?.replaceState(null,'','/transfer');
  let pendingReceive=0;
  const rtcHint='当前浏览器未提供 WebRTC，无法直传。请检查 WebRTC 扩展是否放行当前地址，或用其他浏览器重试；无需开启摄像头或麦克风。';
  let rtcIssue=typeof globalThis.RTCPeerConnection==='function'?'':rtcHint;
  const rtcPeers=new Map();
  const peers=new Map(), links=new Map(), files=new Map(), catalogs=new Map(), history=[], texts=[];
  const encoder=new TextEncoder();
  const uuid=()=>{const a=new Uint8Array(12);crypto.getRandomValues(a);return Array.from(a,x=>x.toString(16).padStart(2,'0')).join('');};
  const bytes=n=>n>=1024**3?(n/1024**3).toFixed(1)+' GB':n>=1024**2?(n/1024**2).toFixed(1)+' MB':n>=1024?(n/1024).toFixed(1)+' KB':n+' B';
  function el(tag,text,cls){const e=document.createElement(tag);if(text!==undefined)e.textContent=text;if(cls)e.className=cls;return e;}
  function icon(name){
    const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');
    const use=document.createElementNS(svg.namespaceURI,'use');
    use.setAttribute('href','#i-'+name);
    svg.setAttribute('aria-hidden','true');
    svg.append(use);
    return svg;
  }
  function notice(text,kind='action'){noticeMessage=text;noticeKind=kind;renderConnection();}
  function renderConnection(){
    $('lamp').classList.toggle('on',online);
    $('connection').textContent=online?'局域网已连接':ended?'流转已关闭':connecting?'正在连接':'连接已中断';
    $('reconnect').hidden=online;
    $('reconnect').disabled=connecting;
    const text=noticeMessage||(online?'局域网已连接，可以发送文字与共享文件。':ended?'流转已结束，点击重新连接即可自动开启。':connecting?'正在连接 Passport，请保持网页打开。':'连接已中断，请重新连接。');
    const tone=noticeMessage?'error':online?'connected':'connecting';
    $('notice').dataset.tone=tone;
    $('notice').replaceChildren(icon(tone==='connected'?'check':'info'),el('span',text));
    $('notice').hidden=false;
  }
  async function copyText(text,button){
    try{
      let copied=false;
      if(navigator.clipboard?.writeText){try{await navigator.clipboard.writeText(text);copied=true;}catch{}}
      if(!copied){
        const focus=document.activeElement,field=el('textarea',text);
        field.value=text;
        field.setAttribute('readonly','');
        field.style.cssText='position:fixed;top:0;left:-9999px;opacity:0';
        document.body.append(field);
        field.select();
        try{copied=document.execCommand('copy');}
        finally{field.remove();focus?.focus({preventScroll:true});}
      }
      if(!copied)throw Error('复制失败，请允许浏览器访问剪贴板后重试。');
      button.replaceChildren(icon('check'),document.createTextNode('已复制'));
      setTimeout(()=>{
        if(button.isConnected)button.replaceChildren(icon('copy'),document.createTextNode('复制'));
      },2000);
    }catch(e){notice(e.message);}
  }
  function textLabel(t){
    if(!t.recipients)return '来自 '+t.name+' · 已接收';
    const groups=state=>t.recipients.filter(r=>r.state===state).map(r=>r.name).join('、');
    const received=groups('received'),pending=groups('pending'),failed=groups('failed');
    const status=[received&&received+'已接收',pending&&'等待'+pending+'接收',failed&&failed+'未接收'];
    return '发送给 '+t.recipients.map(r=>r.name).join('、')+'，'+status.filter(Boolean).join('；')+'。';
  }
  function textStatus(t){t.status=t.recipients.every(r=>r.state==='received')?'接收端已确认':t.recipients.some(r=>r.state==='pending')?'等待接收确认':'未收到确认，请重试';}
  function saveReceived(t){
    if(t.done)return;
    t.done=true;
    t.status='已校验，已交给浏览器保存';
    try{control(t.link,{type:'complete',id:t.id,saved:'download'});}catch{notice('文件已接收，但发送端已离线。');}
    if(t.link.rx===t)t.link.rx=null;
    // Let the browser claim the download URL before releasing its memory budget.
    setTimeout(()=>{
      if(t.url){URL.revokeObjectURL(t.url);t.url=null;}
      releaseMemory(t);
      update();
    },1000);
    update();
  }
  function appendProgress(row,t){
    const block=el('div',undefined,'file-progress'+(t.sha256&&!t.error?' complete':''));
    const track=el('div',undefined,'progress-track'),progress=el('progress');
    const caption=el('div',undefined,'progress-caption');
    block.dataset.transferId=t.id;
    const value=t.sha256&&!t.error?100:t.file.size?Math.min(99,t.offset/t.file.size*100):0;
    progress.max=100;
    progress.value=value;
    progress.setAttribute('aria-label',t.file.name+'传输进度');
    caption.append(el('span',`${t.direction==='发送'?'发送给 '+t.peerName+' · ':''}${bytes(t.offset)} / ${bytes(t.file.size)} · ${t.status}`),el('span',Math.floor(value)+'%'));
    track.append(progress,caption);
    block.append(track);
    if(t.direction==='发送'&&!t.done){
      const cancel=el('button','取消','cancel-file');
      cancel.onclick=()=>cancelTransfer(t,'已取消');
      block.append(cancel);
    }
    row.append(block);
  }
  function signal(message){if(!online||ws.readyState!==WebSocket.OPEN)throw Error('会话已结束，请重新连接');const text=JSON.stringify(message);if(encoder.encode(text).length>4096||ws.bufferedAmount>16384)throw Error('连接协商拥堵，请重试');ws.send(text);}
  function rename(){let name=$('name').value.trim()|| (mobile?'手机':'电脑');while(encoder.encode(name).length>48)name=name.slice(0,-1);$('name').value=name;try{localStorage.setItem('passport-transfer-name',name);}catch{}if(online)signal({type:'hello',generation,lease,name,rtc:!rtcIssue});}
  function targets(){return $('target').value==='all'?[...peers.keys()].filter(id=>id!==self&&rtcPeers.get(id)!==false):[Number($('target').value)].filter(id=>peers.has(id)&&id!==self);}
  function announce(){const list=[...files.values()].map(x=>({...x.meta,to:x.to}));if(encoder.encode(JSON.stringify(list)).length>2304)throw Error('文件列表过长，请减少文件或缩短文件名');if(online)signal({type:'files',files:list});}
  function update(){if(rendering)return;rendering=true;setTimeout(()=>{rendering=false;render();},100);}
  function render(){
    const textScroll=$('texts').scrollTop,catalogScroll=$('catalog').scrollTop;
    renderConnection();
    $('send-text').disabled=!online||!!rtcIssue||sendingText;
    $('files').disabled=!online||!!rtcIssue;
    $('target').disabled=!online;
    renderPeers();
    renderFiles();
    renderTexts();
    $('texts').scrollTop=textScroll;
    $('catalog').scrollTop=catalogScroll;
  }
  function renderPeers(){
    $('online').textContent=`在线 ${peers.size} 台`;
    const target=$('target').value;
    $('target').replaceChildren(el('option','所有在线设备'));
    $('target').firstChild.value='all';
    $('peers').replaceChildren();
    for(const [id,name] of peers){
      const item=el('span',name,'peer');
      item.append(el('small',rtcPeers.get(id)===false?'直传不可用':id===self?'本机':'#'+id));
      $('peers').append(item);
      if(id!==self){
        const option=el('option',name+' · #'+id+(rtcPeers.get(id)===false?' · 直传不可用':''));
        option.value=String(id);
        $('target').append(option);
      }
    }
    $('target').value=[...$('target').options].some(o=>o.value===target)?target:'all';
    if(!peers.size)$('peers').append(el('p','等待其他设备打开此页','empty'));
  }
  function renderFiles(){
    $('catalog').replaceChildren();
    const displayed=new Set();
    const addFile=(owner,f,mine,to,shared=true)=>{
      if(!mine&&to&&to!==self)return;
      const key=(mine?0:owner)+':'+f.id;
      if(displayed.has(key))return;
      displayed.add(key);
      const transfers=history.filter(t=>(t.direction==='发送')===mine&&(mine||t.owner===owner)&&t.file.id===f.id),latest=transfers[0];
      const row=el('div',undefined,'item'),head=el('div',undefined,'item-head');
      const left=el('div',undefined,'item-title'),info=el('div');
      const actions=el('div',undefined,'item-actions');
      row.dataset.fileId=f.id;
      row.dataset.owner=String(owner);
      const kind=f.mime.startsWith('image/')?'photo':/\.(zip|7z|rar)$/i.test(f.name)?'archive':'document';
      const fileIcon=el('span',undefined,'file-icon '+kind);
      fileIcon.append(icon(kind==='photo'?'photo':'file'));
      info.append(el('strong',f.name),el('p',`${bytes(f.size)} · ${mine?'本机共享':peers.get(owner)||latest?.peerName||'设备'}${to?' · 指定设备':''}`,'small'));
      left.append(fileIcon,info);
      if(!mine&&latest?.url){
        const save=el('a',undefined,'btn primary small save-file');
        save.append(icon('download'),document.createTextNode('保存文件'));
        save.href=latest.url;
        save.download=f.name;
        save.onclick=()=>saveReceived(latest);
        actions.append(save);
      }else if(!mine&&latest&&!latest.done){
        const cancel=el('button','取消','cancel-file');
        cancel.onclick=()=>cancelTransfer(latest,'已取消');
        actions.append(cancel);
      }else if(shared){
        const button=el('button',mine?'取消共享':latest?.error?'重新接收':'接收');
        button.disabled=!online||(!mine&&(!!rtcIssue||rtcPeers.get(owner)===false));
        button.onclick=()=>{
          if(mine){
            files.delete(f.id);
            try{announce();}catch(e){notice(e.message);}
            update();
          }else receive(owner,f).catch(e=>notice(e.message));
        };
        actions.append(button);
      }
      head.append(left,actions);
      row.append(head);
      if(mine){
        const seen=new Set();
        for(const t of transfers)if(!seen.has(t.link.id)){
          seen.add(t.link.id);
          appendProgress(row,t);
        }
      }else if(latest)appendProgress(row,latest);
      $('catalog').append(row);
    };
    for(const {meta,to} of files.values())addFile(self,meta,true,to);
    for(const [owner,list] of catalogs)for(const f of list)addFile(owner,f,false,f.to);
    // Keep an in-flight or finished file reachable if its shared catalog disappears.
    for(const t of history)addFile(t.owner,t.file,t.direction==='发送',0,false);
    if(!$('catalog').childElementCount)$('catalog').append(el('p','暂无共享文件','empty'));
  }
  function renderTexts(){
    $('texts').replaceChildren();
    for(const t of texts){
      const row=el('div',undefined,'received-text'),heading=el('div',undefined,'text-heading');
      const label=el('small',textLabel(t)),pre=el('pre',t.text),copy=el('button',undefined,'quiet');
      if(t.recipients?.every(r=>r.state==='received'))label.className='delivery-confirmed';
      copy.type='button';
      copy.append(icon('copy'),document.createTextNode('复制'));
      copy.onclick=()=>copyText(t.text,copy);
      heading.append(label,copy);
      row.append(heading,pre);
      $('texts').append(row);
    }
  }
  function record(link,id,file,direction){
    const t={link,id,file,direction,owner:direction==='发送'?self:link.id,peerName:peers.get(link.id)||'设备',offset:0,status:'正在连接',done:false,last:Date.now(),hash:new P.SHA256(),chunks:[]};
    history.unshift(t);
    while(history.length>20){
      const i=history.findLastIndex(x=>x.done);
      if(i<0)break;
      const [old]=history.splice(i,1);
      if(old.url)URL.revokeObjectURL(old.url);
    }
    update();
    return t;
  }
  function releaseMemory(t){t.chunks=[];t.reserved=0;}
  function cancelTransfer(t,reason,notify=true){
    if(t.done)return;
    t.done=true;
    t.error=true;
    t.status=reason;
    t.canceled=true;
    if(t.url){URL.revokeObjectURL(t.url);t.url=null;}
    if(notify&&t.link.dc?.readyState==='open'){
      try{control(t.link,{type:'cancel',id:t.id,reason});}catch{}
    }
    if(t.direction==='接收'){
      t.link.discardIncoming=true;
      const canceled=t.link.canceledReceives||(t.link.canceledReceives=new Set());
      canceled.add(t.id);
      if(canceled.size>20)canceled.delete(canceled.values().next().value);
    }
    if(t.sink)Promise.resolve(t.sink.abort()).catch(()=>{});
    releaseMemory(t);
    if(t.link.rx===t)t.link.rx=null;
    if(t.link.tx===t)t.link.tx=null;
    update();
  }
  function closeLink(link,reason){clearTimeout(link.timer);if(link.rx)cancelTransfer(link.rx,reason,false);if(link.tx)cancelTransfer(link.tx,reason,false);link.closed=true;link.dc?.close();link.pc.close();if(links.get(link.id)===link)links.delete(link.id);link.reject?.(Error(reason));}
  function shutdown(reason){
    online=false;
    connecting=false;
    clearInterval(heartbeat);
    clearTimeout(connectDeadline);
    for(const link of [...links.values()])closeLink(link,reason);
    for(const t of texts)if(t.recipients){
      for(const r of t.recipients)if(r.state==='pending')r.state='failed';
      textStatus(t);
    }
    peers.clear();
    rtcPeers.clear();
    catalogs.clear();
    update();
  }
  function disconnected(socket,code,hint=''){
    if(ws!==socket)return;
    // A stalled handshake/close need not emit onclose promptly. Retire the old
    // callbacks and schedule the bounded retry independently of that event.
    ws=null;shutdown(ended?'Passport 已结束共享':'网页会话断开，直连已结束');
    socket.close();
    if(!ended){const retry=retries<5;notice(hint||`Passport 网页会话断开（${code}），${retry?'正在重新连接':'请点击重新连接'}。共享源仍在本机。`,'connection');if(retry){const delay=Math.min(1000*2**retries++,15000);retryTimer=setTimeout(()=>connect(true),delay);}}
  }
  function ping(socket){
    if(ws!==socket||!online)return;
    if(Date.now()-lastPong>120000){disconnected(socket,1006,'Passport 会话超时，请检查网络或重新连接。');return;}
    try{signal({type:'ping'});}catch{disconnected(socket,1006,'Passport 信令连接拥堵，正在重新连接。');}
  }
  async function connect(automatic=false){
    if(automatic){
      const attempt=wakeRequest,epoch=generation;
      const previous=ws;ws=null;previous?.close();
      try{
        const r=await fetch('/api/session/resume',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mode:'transfer',generation}),signal:AbortSignal.timeout(3000)});
        if(attempt!==wakeRequest||epoch!==generation||ended)return;
        if(!r.ok){if(r.status===409||r.status===404||r.status===403){ended=true;clearTimeout(retryTimer);shutdown('会话已结束');notice('流转会话已结束，点击重新连接即可切换回来。');return;}throw Error('连接暂不可用');}
        const info=await r.json();if(attempt!==wakeRequest||epoch!==generation||ended)return;
        if(info.generation!==generation||!Number.isInteger(info.lease))throw Error('连接响应无效');lease=info.lease;
      }catch(e){if(attempt!==wakeRequest||epoch!==generation||ended)return;if(retries<5)retryTimer=setTimeout(()=>connect(true),Math.min(1000*2**retries++,15000));else notice('请点击重新连接。','connection');return;}
    }
    clearTimeout(retryTimer);ended=false;if(!automatic)retries=0;const previous=ws;ws=null;previous?.close();shutdown('会话重新连接');
    connecting=true;notice('','connection');
    ws=new WebSocket(`${location.protocol==='https:'?'wss':'ws'}://${location.host}/transfer/ws`);const socket=ws;
    connectDeadline=setTimeout(()=>{if(ws===socket&&!online)disconnected(socket,1006,'连接 Passport 超时，请检查网络或重新连接。');},8000);
    ws.onmessage=event=>{if(ws!==socket)return;try{const m=JSON.parse(event.data);if(m.type==='welcome'){clearTimeout(connectDeadline);retries=0;self=m.id;online=true;connecting=false;rename();announce();if(noticeKind==='connection')notice('');if(rtcIssue)notice(rtcIssue,'rtc');heartbeat=setInterval(()=>ping(socket),10000);lastPong=Date.now();update();}
      else if(m.type==='pong')lastPong=Date.now();
      else if(m.type==='peers'){const next=new Map(m.peers.map(x=>[x.id,x.name]));rtcPeers.clear();for(const p of m.peers)rtcPeers.set(p.id,p.rtc!==false);for(const link of [...links.values()])if(!next.has(link.id)||rtcPeers.get(link.id)===false)closeLink(link,'对端已离线或无法直传');for(const id of catalogs.keys())if(!next.has(id))catalogs.delete(id);peers.clear();for(const [id,name] of next)peers.set(id,name);for(const item of files.values())if(item.to&&!peers.has(item.to)){files.delete(item.meta.id);notice('指定接收端已离线，该文件已取消共享。');}update();}
      else if(m.type==='files'&&m.from!==self&&peers.has(m.from)){if(!Array.isArray(m.files)||m.files.length>P.MAX_FILES||m.files.some(f=>!P.validFile(f)))throw Error('文件列表格式无效');catalogs.set(m.from,m.files);update();}
      else if(m.type==='signal')handleSignal(m).catch(e=>{const link=links.get(m.from);if(link)closeLink(link,e.message);notice(e.message);});
      else if(m.type==='end'){ended=true;shutdown('Passport 已结束共享');notice('流转已结束，点击重新连接即可自动开启。');socket.close();}
      else if(m.type==='error'){const reason=m.code==='peer-offline'?'接收端已离线，请重新选择设备':m.message||'协商消息无效，请重试';const link=links.get(m.to);if(link)closeLink(link,reason);notice(reason);}
    }catch(e){notice(e.message);}};
    ws.onclose=event=>disconnected(socket,event.code);ws.onerror=()=>{if(ws===socket)notice('无法连接 Passport，请确认处于同一网络。','connection');};update();
  }
  let lastPong=0;
  function requireRTC(id){if(rtcIssue||typeof globalThis.RTCPeerConnection!=='function')throw Error(rtcIssue||rtcHint);if(rtcPeers.get(id)===false)throw Error('对端浏览器无法直传，请对端用访客窗口重试或检查 WebRTC 设置。');}
  function newLink(id){
    requireRTC(id);let pc;
    try{pc=new globalThis.RTCPeerConnection({iceServers:[]});}catch(e){rtcIssue=rtcHint;rename();notice(rtcIssue);update();throw Error(rtcIssue);}
    const link={id,pc,queue:[],closed:false,rx:null,tx:null};links.set(id,link);
    link.ready=new Promise((resolve,reject)=>{link.resolve=resolve;link.reject=reject;});link.ready.catch(()=>{});
    link.timer=setTimeout(()=>{closeLink(link,'无法直连：请检查设备隔离或防火墙后重试');notice('两端能访问网页仍可能无法直连，请检查访客网络、设备隔离或防火墙。');},20000);
    link.pc.onicecandidate=e=>{if(e.candidate&&online&&!link.closed)try{signal({type:'signal',to:id,candidate:e.candidate.toJSON()});}catch(err){closeLink(link,err.message);}};
    link.pc.onconnectionstatechange=()=>{if(['failed','disconnected','closed'].includes(link.pc.connectionState)&&!link.closed)closeLink(link,'直连中断，请保持页面在前台并重试');};
    link.pc.ondatachannel=e=>attach(link,e.channel);return link;
  }
  async function offer(link){if(link.offering||link.pc.localDescription)return;link.offering=true;attach(link,link.pc.createDataChannel('passport-content',{ordered:true}));await link.pc.setLocalDescription(await link.pc.createOffer());signal({type:'signal',to:link.id,description:link.pc.localDescription.toJSON()});}
  async function handleSignal(m){if(!online||!peers.has(m.from)||m.from===self)return;requireRTC(m.from);let link=links.get(m.from)||newLink(m.from);
    if(m.request){if(self<m.from)await offer(link);return;}
    if(m.description){if(m.description.type==='offer'&&self<m.from)throw Error('协商顺序无效');await link.pc.setRemoteDescription(m.description);for(const candidate of link.queue)await link.pc.addIceCandidate(candidate);link.queue=[];if(m.description.type==='offer'){await link.pc.setLocalDescription(await link.pc.createAnswer());signal({type:'signal',to:m.from,description:link.pc.localDescription.toJSON()});}}
    else if(m.candidate){if(link.pc.remoteDescription)await link.pc.addIceCandidate(m.candidate);else if(link.queue.length<64)link.queue.push(m.candidate);else throw Error('候选地址过多，请重试');}
  }
  async function ensure(id){if(!online||!peers.has(id))throw Error('接收端已离线');let link=links.get(id);if(!link){link=newLink(id);if(self<id)offer(link).catch(e=>closeLink(link,e.message));else signal({type:'signal',to:id,request:true});}await link.ready;if(link.closed)throw Error('连接已中断');return link;}
  function control(link,data){const text=JSON.stringify(data),limit=P.chunkSize(link.pc.sctp?.maxMessageSize);if(encoder.encode(text).length>limit)throw Error('文字或消息过长，请缩短后重试');if(link.closed||link.dc?.readyState!=='open')throw Error('直连已中断');link.dc.send(text);}
  function attach(link,dc){if(link.dc&&link.dc!==dc){dc.close();return;}link.dc=dc;dc.binaryType='arraybuffer';dc.bufferedAmountLowThreshold=P.WINDOW/2;
    dc.onopen=()=>{clearTimeout(link.timer);link.resolve(link);};dc.onclose=()=>{if(!link.closed)closeLink(link,'直连已关闭');};dc.onerror=()=>closeLink(link,'直连发生错误，请重试');
    let chain=Promise.resolve();dc.onmessage=e=>{chain=chain.then(()=>onData(link,e.data)).catch(err=>{if(link.rx)cancelTransfer(link.rx,err.message);if(link.tx)cancelTransfer(link.tx,err.message);notice(err.message);});};
  }
  async function onData(link,data){
    if(typeof data!=='string'){const t=link.rx;
      // An ordered channel may still deliver chunks queued before cancellation.
      // Only the next validated begin marks the boundary of the new byte stream.
      if(link.discardIncoming&&(!t||!t.chunk))return;
      if(!t||!t.chunk||t.done||t.status==='已校验，等待保存')throw Error('收到了无效文件分块');const chunk=data instanceof ArrayBuffer?new Uint8Array(data):new Uint8Array(await data.arrayBuffer());
      if(!chunk.length||chunk.length>t.chunk||t.offset+chunk.length>t.file.size)throw Error('文件分块超出预期');if(t.sink)await t.sink.write(chunk);else t.chunks.push(chunk);if(t.canceled)return;t.hash.update(chunk);t.offset+=chunk.length;t.last=Date.now();t.status='接收中';control(link,{type:'ack',id:t.id,offset:t.offset});update();return;
    }
    if(encoder.encode(data).length>P.CHUNK)throw Error('直连消息过大');const m=JSON.parse(data);
    if(['begin','finish','cancel'].includes(m.type)&&link.canceledReceives?.has(m.id))return;
    if(m.type==='text'){if(typeof m.text!=='string'||encoder.encode(m.text).length>P.TEXT_BYTES||!/^[a-f0-9]{24}$/.test(m.id))throw Error('文字格式无效');texts.unshift({text:m.text,name:peers.get(link.id),status:'已收到'});texts.splice(32);control(link,{type:'text-ok',id:m.id});update();}
    else if(m.type==='text-ok'){const t=texts.find(x=>x.id===m.id&&x.recipients?.some(r=>r.id===link.id));if(t){t.recipients.find(r=>r.id===link.id).state='received';textStatus(t);update();}}
    else if(m.type==='get'){const source=files.get(m.file);if(!source||(source.to&&source.to!==link.id)){control(link,{type:'cancel',id:m.id,reason:'文件已取消共享'});return;}if(!/^[a-f0-9]{24}$/.test(m.id)||link.tx||[...links.values()].filter(l=>l.tx).length>=2){control(link,{type:'cancel',id:m.id,reason:'发送端忙，请稍后重试'});return;}
      const t=record(link,m.id,source.meta,'发送');link.tx=t;t.source=source.file;t.chunk=P.chunkSize(link.pc.sctp?.maxMessageSize);t.acked=0;t.ready=false;t.complete=false;t.sent=0;control(link,{type:'begin',id:t.id,file:t.file,chunk:t.chunk});sendFile(t).catch(e=>cancelTransfer(t,e.message));
    }else if(m.type==='begin'){const t=link.rx;if(!t||t.id!==m.id||!P.validFile(m.file)||m.file.id!==t.file.id||m.file.size!==t.file.size||m.file.name!==t.file.name||!Number.isInteger(m.chunk)||m.chunk<1024||m.chunk>P.CHUNK)throw Error('文件信息不匹配');link.discardIncoming=false;t.chunk=m.chunk;t.status='接收中';t.last=Date.now();control(link,{type:'ready',id:t.id});update();}
    else if(m.type==='ready'){if(link.tx?.id===m.id){link.tx.ready=true;link.tx.last=Date.now();}}
    else if(m.type==='ack'){const t=link.tx;if(t?.id===m.id){if(!P.ackValid(m.offset,t.acked,t.sent))throw Error('接收进度无效');t.acked=m.offset;t.offset=m.offset;t.last=Date.now();update();}}
    else if(m.type==='finish'){const t=link.rx;if(!t||t.id!==m.id||t.offset!==t.file.size||m.size!==t.file.size||!/^[a-f0-9]{64}$/.test(m.sha256)||t.hash.hex()!==m.sha256)throw Error('文件校验失败，请重新接收');t.sha256=m.sha256;
      if(t.sink){await t.sink.close();if(t.canceled)return;t.done=true;t.status='已校验并保存';link.rx=null;control(link,{type:'complete',id:t.id,saved:'file'});}
      else{const blob=new Blob(t.chunks,{type:'application/octet-stream'});t.url=URL.createObjectURL(blob);t.status='已校验，等待保存';t.last=Date.now();t.chunks=[];}update();
    }else if(m.type==='complete'){const t=link.tx;if(t?.id===m.id&&t.finishing&&['file','download'].includes(m.saved)){t.complete=true;t.status=m.saved==='file'?'接收端已校验并保存':'接收端已校验并请求保存';}}
    else if(m.type==='cancel'){for(const t of [link.rx,link.tx])if(t?.id===m.id)cancelTransfer(t,typeof m.reason==='string'?m.reason.slice(0,120):'对端取消了传输',false);}
    else throw Error('直连消息类型无效');
  }
  async function wait(t,condition,timeout=30000){const at=Date.now();while(!condition()){if(t.canceled||t.link.closed)throw Error('传输已结束');if(Date.now()-at>timeout)throw Error('传输超时，请保持接收端在前台并重试');await new Promise(r=>setTimeout(r,8));}}
  async function sendFile(t){await wait(t,()=>t.ready);t.status='发送中';update();
    while(t.sent<t.file.size){await wait(t,()=>t.sent-t.acked<P.WINDOW&&t.link.dc.bufferedAmount<P.WINDOW);const end=Math.min(t.sent+t.chunk,t.file.size);const chunk=await t.source.slice(t.sent,end).arrayBuffer();if(t.canceled)throw Error('已取消');t.hash.update(chunk);t.link.dc.send(chunk);t.sent=end;}
    await wait(t,()=>t.acked===t.file.size);t.finishing=true;t.status='等待接收端校验和保存';t.sha256=t.hash.hex();control(t.link,{type:'finish',id:t.id,size:t.file.size,sha256:t.sha256});update();
    await wait(t,()=>t.complete,180000);t.done=true;if(t.link.tx===t)t.link.tx=null;t.source=null;update();
  }
  async function receive(id,file){
    requireRTC(id);if(!P.validFile(file))throw Error('文件信息无效');let sink;
    if(globalThis.isSecureContext&&typeof showSaveFilePicker==='function'){const handle=await showSaveFilePicker({suggestedName:file.name});sink=await handle.createWritable();}
    else if(file.size>memoryLimit)throw Error(`本浏览器使用内存接收，接收总容量上限 ${memoryLimitLabel}。请选择较小文件，或使用支持直接写文件的浏览器。`);
    const used=history.reduce((n,t)=>n+(t.reserved||0),0);if(!sink&&used+pendingReceive+file.size>memoryLimit)throw Error('接收容量已满，请先保存或取消正在接收的文件');
    const reservation=sink?0:file.size;pendingReceive+=reservation;
    let link,t;try{link=await ensure(id);if(link.rx)throw Error('正在接收此设备的另一文件，请稍后重试');t=record(link,uuid(),file,'接收');t.sink=sink;t.reserved=sink?0:file.size;link.rx=t;control(link,{type:'get',id:t.id,file:file.id});}
    catch(e){if(sink)await sink.abort().catch(()=>{});if(t)cancelTransfer(t,e.message);throw e;}finally{pendingReceive-=reservation;}
  }
  $('rename').onclick=()=>{try{rename();}catch(e){notice(e.message);}};
  $('reconnect').onclick=async()=>{
    const request=++wakeRequest;
    wakeAbort?.abort();const controller=new AbortController();wakeAbort=controller;
    clearTimeout(retryTimer);ended=false;connecting=true;update();
    const timeout=setTimeout(()=>controller.abort(),30000);
    try{
      const response=await fetch('/api/session/open',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mode:'transfer'}),signal:controller.signal});
      if(!response.ok)throw Error(await response.text());
      const info=await response.json();if(request!==wakeRequest)return;
      generation=info.generation;lease=info.lease;
      for(;;){
        if(request!==wakeRequest||controller.signal.aborted)throw new DOMException('Aborted','AbortError');
        try{
          const r=await fetch('/api/session',{cache:'no-store',signal:controller.signal});
          if(r.ok){const state=await r.json();if(request!==wakeRequest)return;if(state.generation!==generation||state.mode!=='transfer')throw Error('会话已被其他页面切换，请重新连接。');if(state.ready)break;}
        }catch(e){if(e.name==='AbortError'||e.message.includes('会话已被'))throw e;}
        await new Promise(resolve=>setTimeout(resolve,250));
      }
      if(request===wakeRequest)connect();
    }catch(e){if(request===wakeRequest){connecting=false;notice(e.name==='AbortError'?'无法访问 Passport，请检查网络后重新连接。':e.message,'connection');update();}}
    finally{clearTimeout(timeout);if(wakeAbort===controller)wakeAbort=null;}
  };
  async function sendText(){
    if(sendingText||!online)return;
    const value=$('text').value;
    try{
      requireRTC();
      if(!value||encoder.encode(value).length>P.TEXT_BYTES)throw Error('请输入文字，最多 8,000 字节');
      const ids=targets();
      if(!ids.length)throw Error(peers.size>1?'在线设备暂不能直传，请对端检查浏览器 WebRTC 设置。':'请让另一台设备先打开网页');
      sendingText=true;
      $('send-text').disabled=true;
      const t={id:uuid(),text:value,recipients:ids.map(id=>({id,name:peers.get(id),state:'pending'})),status:'等待接收确认'};
      texts.unshift(t);
      texts.splice(32);
      update();
      const results=await Promise.allSettled(t.recipients.map(async r=>{
        try{
          const link=await ensure(r.id);
          control(link,{type:'text',id:t.id,text:value});
        }catch(e){r.state='failed';throw e;}
      }));
      textStatus(t);
      if(results.some(r=>r.status==='fulfilled'))$('text').value='';
      const failure=results.find(r=>r.status==='rejected');
      notice(failure?failure.reason.message:'');
      setTimeout(()=>{
        for(const r of t.recipients)if(r.state==='pending')r.state='failed';
        textStatus(t);
        update();
      },20000);
    }catch(e){notice(e.message);}
    finally{sendingText=false;update();}
  }
  $('send-text').onclick=sendText;
  $('text').onkeydown=event=>{
    if(event.key!=='Enter'||event.shiftKey||event.isComposing||event.keyCode===229)return;
    event.preventDefault();
    if(!event.repeat&&!$('send-text').disabled)return sendText();
  };
  $('files').onchange=()=>{try{requireRTC();for(const file of $('files').files){if(files.size>=P.MAX_FILES)throw Error('最多共享 8 个文件，请先取消部分共享');const meta={id:uuid(),name:file.name,size:file.size,mime:file.type};if(!P.validFile(meta))throw Error('文件名过长或尺寸超过 8 GB');const to=$('target').value==='all'?0:Number($('target').value);requireRTC(to||undefined);files.set(meta.id,{meta,file,to});try{const list=[...files.values()].map(x=>({...x.meta,to:x.to}));if(encoder.encode(JSON.stringify(list)).length>2304)throw Error('文件列表过长，请减少文件或缩短文件名');}catch(e){files.delete(meta.id);throw e;}}announce();notice('');}catch(e){notice(e.message);try{announce();}catch{}}finally{$('files').value='';update();}};
  $('save-mode').textContent=globalThis.isSecureContext&&typeof showSaveFilePicker==='function'?'接收时选择保存位置。':`接收总容量上限 ${memoryLimitLabel}，校验后点击保存。下载结果请在浏览器中确认。`;
  try{$('name').value=localStorage.getItem('passport-transfer-name')||(mobile?'手机':'电脑');}catch{$('name').value=mobile?'手机':'电脑';}
  setInterval(()=>{for(const link of links.values()){const t=link.rx;if(t&&!t.done&&Date.now()-t.last>(t.url?180000:45000))cancelTransfer(t,t.url?'保存等待超时，请重新接收':'接收超时，请重试');}},5000);
  addEventListener('visibilitychange',()=>{if(document.visibilityState==='visible'&&online)ping(ws);});
  addEventListener('pagehide',()=>{++wakeRequest;wakeAbort?.abort();clearTimeout(retryTimer);ended=true;shutdown('页面已退出');ws?.close();});
  connect();
  // Exposed only as an application API, also used by the real-browser acceptance harness.
  globalThis.PassportTransfer={receive,files,links,history,texts,get self(){return self;},get peers(){return peers;}};
})();
