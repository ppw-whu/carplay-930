const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const header = fs.readFileSync(path.join(__dirname, '..', 'ControlPage.h'), 'utf8');
const html = header.match(/R"CARPAGE\(([\s\S]*)\)CARPAGE"/)[1];
const source = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const flush = () => new Promise(resolve => setImmediate(resolve));

function mockCar() {
  const state = {source:'none',left:0,right:0,ps3_online:false,remote_mode:false,ps3_speed_percent:80,aux_level:0,wifi_session:0,ip:'192.168.4.1'};
  let nextSession = 100;
  const neutral = () => Object.assign(state, {left:0,right:0,aux_level:0});
  const reply = (status, body) => ({status,ok:status===200,json:async()=>body});
  function handle(urlText) {
    const url = new URL(urlText, 'http://127.0.0.1');
    if (url.pathname === '/status') return reply(200, {...state});
    if (url.pathname === '/claim') {
      neutral(); Object.assign(state,{source:'wifi',remote_mode:false,wifi_session:++nextSession});
      return reply(200,{ok:true,session:state.wifi_session,valid_ms:500});
    }
    if (url.pathname === '/stop') {
      neutral(); Object.assign(state,{source:'none',remote_mode:false,wifi_session:0});
      return reply(200,{ok:true});
    }
    const token = Number(url.searchParams.get('session'));
    if (url.pathname === '/move' && state.remote_mode) return reply(409,{code:'ps3_busy'});
    if (!token || token !== state.wifi_session) return reply(409,{code:'control_changed'});
    if (url.pathname === '/release') {
      neutral(); Object.assign(state,{source:'none',wifi_session:0}); return reply(200,{ok:true});
    }
    if (url.pathname === '/move') {
      const speed=Number(url.searchParams.get('speed')), turn=Number(url.searchParams.get('turn'));
      Object.assign(state,{source:'wifi',left:speed+turn,right:speed-turn,aux_level:Number(url.searchParams.get('aux'))});
      return reply(200,{ok:true});
    }
    return reply(404,{error:'not found'});
  }
  return {state,handle,reply,ps3(speed=40,aux=2) {
    Object.assign(state,{remote_mode:true,ps3_online:true,ps3_speed_percent:speed,wifi_session:0,source:'ps3',left:102,right:102,aux_level:aux});
  }};
}

if (process.argv.includes('--serve')) {
  const http = require('node:http'), car = mockCar(), events=[];
  http.createServer(async(req,res)=>{
    const url = new URL(req.url,'http://127.0.0.1');
    res.setHeader('Cache-Control','no-store');
    if(url.pathname==='/'){res.setHeader('Content-Type','text/html; charset=utf-8');res.end(html);return;}
    if(url.pathname==='/__ps3') car.ps3(Number(url.searchParams.get('speed')||40));
    const result = url.pathname==='/__ps3' ? car.reply(200,{...car.state}) : car.handle(req.url);
    if(url.pathname!=='/status') events.push({url:req.url,...car.state});
    res.statusCode=url.pathname==='/__events'?200:result.status;
    res.setHeader('Content-Type','application/json');
    res.end(JSON.stringify(url.pathname==='/__events'?events:await result.json()));
  }).listen(8765,'127.0.0.1',()=>console.log('Mock car preview: http://127.0.0.1:8765 (no hardware connected)'));
} else {
  class Element {
    constructor(dataset={}) {this.handlers={};this.dataset=dataset;this.textContent='';this.disabled=false;this.attributes={};this.classes=new Set();this.classList={toggle:(name,on)=>on?this.classes.add(name):this.classes.delete(name)};}
    addEventListener(type,fn){this.handlers[type]=fn;}
    setAttribute(name,value){this.attributes[name]=value;}
    setPointerCapture(){}
  }
  async function harness() {
    const car=mockCar(), requests=[], waiting=[];
    const elements=Object.fromEntries(['activate','stop','speedLabel','speedValue','speedHelp','connection','mode','notice','panel','auxCycle','auxState'].map(id=>[id,new Element()]));
    const buttons=['forward','backward','left','right'].map(direction=>new Element({direction}));
    const gears=[...html.matchAll(/data-speed-percent="(\d+)"/g)].map(match=>new Element({speedPercent:match[1]}));
    assert.deepEqual(gears.map(button=>Number(button.dataset.speedPercent)),[20,40,60,80,100]);
    const windowHandlers={}, documentHandlers={};
    let now=0, defer=null, failure=null;
    const context=vm.createContext({
      document:{hidden:false,getElementById:id=>elements[id],querySelectorAll:selector=>selector==='[data-direction]'?buttons:gears,addEventListener:(t,fn)=>documentHandlers[t]=fn},
      window:{addEventListener:(t,fn)=>windowHandlers[t]=fn},
      navigator:{sendBeacon:url=>{requests.push({url,beacon:true});car.handle(url);return true;}},
      performance:{now:()=>now},AbortController,
      setTimeout:()=>1,clearTimeout:()=>{},setInterval:()=>1,
      fetch:async(url,options)=>{
        requests.push({url,method:options.method});
        const pathname=new URL(url,'http://127.0.0.1').pathname;
        if(failure&&failure.path===pathname){const error=failure;failure=null;if(error.network)throw new Error('offline');return car.reply(error.code,{error:'test'});}
        if(defer&&defer.path===pathname){
          const item=defer;defer=null;
          const response=item.stage==='response'?car.handle(url):null;
          return new Promise(resolve=>waiting.push(()=>resolve(response||car.handle(url))));
        }
        return car.handle(url);
      }
    });
    vm.runInContext(source,context);await flush();
    const call=async code=>{vm.runInContext(code,context);await flush();};
    const key=async(type,name,extra={})=>{windowHandlers[type]({key:name,repeat:false,preventDefault(){},...extra});await flush();};
    const click=async id=>{elements[id].handlers.click();await flush();};
    const speed=async percent=>{gears.find(button=>Number(button.dataset.speedPercent)===percent).handlers.click();await flush();};
    const matching=pathname=>requests.filter(r=>new URL(r.url,'http://127.0.0.1').pathname===pathname);
    const isArmed=()=>vm.runInContext('armed',context);
    const token=()=>vm.runInContext('session',context);
    const last=()=>requests.filter(r=>r.url!=='/status').at(-1);
    const expectMove=(speed,turn,aux=0)=>{
      const url=new URL(last().url,'http://127.0.0.1');assert.equal(url.pathname,'/move');
      assert.deepEqual(Object.fromEntries(url.searchParams),{speed:String(speed),turn:String(turn),ms:'500',aux:String(aux),session:String(token())});
      assert.ok(token()>0);
    };
    const expectRelease=expected=>assert.equal(last().url,'/release?session='+expected);
    return {car,requests,elements,buttons,gears,context,windowHandlers,documentHandlers,call,key,click,speed,matching,isArmed,token,last,expectMove,expectRelease,
      defer:(path,stage='request')=>{assert.equal(defer,null);defer={path,stage};},
      fail:(path,code=500,network=false)=>{failure={path,code,network};},
      finish:async()=>{assert.ok(waiting.length);waiting.shift()();await flush();},
      tick:async(ms=101)=>{now+=ms;await call('pump()');}
    };
  }

  (async()=>{
    // Percentage gears, unchanged keyboard/touch semantics, live adjustment and aux independence.
    const h=await harness();
    assert.equal(h.isArmed(),false);assert.equal(h.elements.speedValue.textContent,'4档 · 80%');
    const beforeSelection=h.requests.length;await h.speed(40);assert.equal(h.elements.speedValue.textContent,'2档 · 40%');assert.equal(h.requests.length,beforeSelection);await h.speed(80);
    await h.key('keydown','w');await h.key('keydown','Enter',{ctrlKey:true});assert.equal(h.matching('/claim').length,0);
    await h.key('keyup','w');await h.key('keydown','Enter',{ctrlKey:true});h.expectMove(0,0);assert.equal(h.isArmed(),true);
    assert.equal(h.matching('/claim').length,1);assert.equal(h.matching('/claim')[0].method,'POST');
    const cases=[['w',204,0],['ArrowUp',204,0],['s',-204,0],['ArrowDown',-204,0],['a',0,-204],['ArrowLeft',0,-204],['d',0,204],['ArrowRight',0,204]];
    for(const [key,speed,turn]of cases){await h.key('keydown',key);h.expectMove(speed,turn);await h.key('keyup',key);h.expectMove(0,0);}
    await h.key('keydown','w');
    for(const percent of [20,40,60,80,100]){
      await h.speed(percent);h.expectMove(percent*255/100,0);assert.equal(h.elements.speedValue.textContent,percent/20+'档 · '+percent+'%');
      assert.equal(h.gears.every(button=>!button.disabled),true);assert.equal(h.gears.find(button=>Number(button.dataset.speedPercent)===percent).attributes['aria-pressed'],'true');
      await h.tick();h.expectMove(percent*255/100,0);
    }
    await h.speed(80);await h.key('keydown','d');h.expectMove(0,204);await h.key('keyup','d');h.expectMove(204,0);
    await h.key('keydown','ArrowUp');await h.key('keyup','w');h.expectMove(204,0);await h.key('keyup','ArrowUp');h.expectMove(0,0);
    h.defer('/move','response');await h.key('keydown','w');const waitingCount=h.requests.length;
    await h.speed(20);await h.speed(60);assert.equal(h.requests.length,waitingCount);
    await h.finish();await h.call('pump()');h.expectMove(153,0);await h.speed(80);await h.key('keyup','w');
    const forward=h.buttons[0];forward.handlers.pointerdown({pointerType:'touch',pointerId:1,preventDefault(){}});await flush();h.expectMove(204,0);
    forward.handlers.pointercancel({pointerId:1});await flush();h.expectMove(0,0);
    for(const level of [1,2,3,0]){await h.click('auxCycle');h.expectMove(0,0,level);}
    await h.click('auxCycle');await h.key('keydown','w');h.expectMove(204,0,1);await h.speed(40);h.expectMove(102,0,1);await h.speed(80);
    await h.key('keyup','w');h.expectMove(0,0,1);await h.tick();h.expectMove(0,0,1);
    h.defer('/move','response');await h.click('auxCycle');const queuedCount=h.requests.length;await h.click('auxCycle');assert.equal(h.requests.length,queuedCount);
    await h.finish();await h.call('pump()');h.expectMove(0,0,3);
    const blurToken=h.token();h.windowHandlers.blur();await flush();h.expectRelease(blurToken);assert.equal(h.isArmed(),false);
    await h.tick();h.expectRelease(blurToken);const inactiveCount=h.requests.length;await h.click('auxCycle');assert.equal(h.requests.length,inactiveCount);
    await h.click('activate');h.expectMove(0,0,0);assert.notEqual(h.token(),blurToken);
    h.context.document.hidden=true;h.documentHandlers.visibilitychange();await flush();assert.equal(h.isArmed(),false);
    h.context.document.hidden=false;await h.click('activate');const hideToken=h.token();h.windowHandlers.pagehide();assert.equal(h.isArmed(),false);assert.equal(h.last().beacon,true);h.expectRelease(hideToken);

    // PS3 takeover is observed without a stop/release; explicit claim hands control back neutrally.
    const r=await harness();await r.click('activate');await r.click('auxCycle');await r.key('keydown','w');
    const beforeRemote=r.requests.length;r.car.ps3(40,2);await r.call('refreshStatus()');
    assert.equal(r.isArmed(),false);assert.equal(r.requests.length,beforeRemote+1);assert.equal(r.elements.auxState.textContent,'2档');
    assert.equal(r.elements.speedLabel.textContent,'手柄车轮速度');assert.equal(r.elements.speedValue.textContent,'2档 · 40%');
    assert.equal(r.gears.every(button=>button.disabled),true);assert.equal(r.elements.activate.disabled,false);assert.equal(r.elements.activate.textContent,'切换到网页控制');
    const remoteCount=r.requests.length;await r.speed(100);await r.click('auxCycle');assert.equal(r.requests.length,remoteCount);
    r.car.state.ps3_speed_percent=70;await r.call('refreshStatus()');assert.equal(r.elements.speedValue.textContent,'2档 · 40%');
    await r.click('activate');r.expectMove(0,0,0);assert.equal(r.car.state.remote_mode,false);assert.equal(r.elements.speedValue.textContent,'4档 · 80%');assert.equal(r.car.state.aux_level,0);
    // Immediate 409 from a heartbeat also disarms, never automatically reclaims.
    r.car.ps3();await r.tick();assert.equal(r.isArmed(),false);assert.match(r.elements.notice.textContent,/PS3/);
    const claims=r.matching('/claim').length;await r.tick(1000);assert.equal(r.matching('/claim').length,claims);assert.equal(r.matching('/stop').length,0);
    await r.click('activate');await r.click('auxCycle');await r.key('keydown','Escape');assert.equal(r.last().url,'/stop');assert.equal(r.car.state.aux_level,0);

    // Another web client and an expired session both revoke the old page, without touching the new owner.
    const other=await harness();await other.click('activate');const oldToken=other.token();other.car.handle('/claim');
    const newToken=other.car.state.wifi_session;other.car.handle('/move?speed=80&turn=0&aux=2&session='+newToken);
    await other.tick();assert.equal(other.isArmed(),false);assert.equal(other.car.state.left,80);assert.equal(other.car.state.aux_level,2);
    assert.equal(other.matching('/release').length,0);assert.equal(other.matching('/stop').length,0);
    assert.notEqual(newToken,oldToken);await other.click('activate');other.car.state.wifi_session=0;await other.call('refreshStatus()');assert.equal(other.isArmed(),false);

    // An old /release arriving after a new owner claimed must be scoped to its original token.
    const delayed=await harness();await delayed.click('activate');const delayedToken=delayed.token();delayed.defer('/release');delayed.windowHandlers.blur();await flush();
    delayed.car.handle('/claim');const latestToken=delayed.car.state.wifi_session;delayed.car.handle('/move?speed=99&turn=0&aux=3&session='+latestToken);
    await delayed.finish();delayed.expectRelease(delayedToken);assert.equal(delayed.car.state.wifi_session,latestToken);assert.equal(delayed.car.state.left,99);assert.equal(delayed.car.state.aux_level,3);

    // Claim pending: no movement is enabled, and blur cancels activation even if response is late.
    const late=await harness();late.car.ps3();late.defer('/claim','response');await late.click('activate');
    const lateToken=late.car.state.wifi_session;assert.equal(late.isArmed(),false);assert.equal(late.buttons.every(button=>button.disabled),true);assert.equal(late.elements.activate.disabled,true);
    late.windowHandlers.blur();await flush();late.car.handle('/claim');const afterLate=late.car.state.wifi_session;
    late.car.handle('/move?speed=65&turn=0&aux=1&session='+afterLate);await late.finish();
    assert.equal(late.isArmed(),false);late.expectRelease(lateToken);assert.equal(late.car.state.wifi_session,afterLate);assert.equal(late.car.state.left,65);assert.equal(late.matching('/move').length,0);
    const held=await harness();held.defer('/claim','response');await held.click('activate');await held.key('keydown','w');await held.finish();assert.equal(held.isArmed(),false);assert.equal(held.matching('/move').length,0);
    await held.key('keyup','w');await held.click('activate');held.expectMove(0,0);

    // Read-only status response begun before a claim cannot undo the new ownership.
    const stale=await harness();stale.car.ps3();stale.defer('/status','response');await stale.call('refreshStatus()');await stale.click('activate');const activeToken=stale.token();
    await stale.finish();assert.equal(stale.isArmed(),true);assert.equal(stale.token(),activeToken);assert.equal(stale.elements.mode.textContent,'网页控制中');

    // Network failure releases only the failed session; claim failure never retries automatically.
    const failure=await harness();failure.fail('/claim',503);await failure.click('activate');assert.equal(failure.isArmed(),false);assert.equal(failure.matching('/move').length,0);
    await failure.tick(1000);assert.equal(failure.matching('/claim').length,1);await failure.click('activate');const failedToken=failure.token();
    failure.fail('/move',500,true);await failure.key('keydown','w');assert.equal(failure.isArmed(),false);failure.expectRelease(failedToken);
    await failure.click('activate');failure.expectMove(0,0,0);
    // Explicit global stop during an in-flight command is never downgraded to release.
    failure.defer('/move','response');await failure.click('auxCycle');await failure.click('stop');await failure.finish();assert.equal(failure.last().url,'/stop');assert.equal(failure.car.state.aux_level,0);
    const stopClaim=await harness();stopClaim.defer('/claim','response');await stopClaim.click('activate');const stoppedToken=stopClaim.car.state.wifi_session;
    await stopClaim.click('stop');await stopClaim.finish();assert.equal(stopClaim.isArmed(),false);assert.equal(stopClaim.matching('/stop').length,1);stopClaim.expectRelease(stoppedToken);assert.equal(stopClaim.car.state.wifi_session,0);
    console.log('PASS web controls: percentage gears, keyboard/touch/aux, neutral claim, scoped sessions, PS3 + browser takeover, stale requests/status, cancellation races, timeouts and explicit global stop');
  })().catch(error=>{console.error(error);process.exitCode=1;});
}
