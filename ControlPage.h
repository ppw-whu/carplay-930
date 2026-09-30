#pragma once
#include <Arduino.h>

// 全部资源随固件提供，连接小车热点后无需互联网。
const char CONTROL_PAGE[] PROGMEM = R"CARPAGE(<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>小车控制</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#eef3f8;color:#172941;font:16px system-ui,-apple-system,"Microsoft YaHei",sans-serif}main{max-width:620px;margin:auto;padding:24px 18px}h1{font-size:27px;margin:0 0 8px}p{line-height:1.6;margin:8px 0;color:#58697b}.top{display:flex;justify-content:space-between;align-items:center;gap:12px}.chip{font-size:13px;border-radius:24px;padding:7px 12px;background:#dfe8f3;white-space:nowrap}.card{margin-top:18px;padding:20px;background:white;border:1px solid #dfe7f0;border-radius:20px;box-shadow:0 8px 28px #19375608}.mode{display:flex;justify-content:space-between;align-items:center;margin-bottom:14px}.mode strong{color:#126b62}button{font:inherit;cursor:pointer;border:0;border-radius:12px;touch-action:none;user-select:none;-webkit-user-select:none;-webkit-touch-callout:none}button:focus-visible{outline:3px solid #55a7f0;outline-offset:3px}.activate{width:100%;padding:14px;background:#1768d1;color:white;font-weight:650}.activate:disabled{background:#c2d9ef;color:#2a537a;cursor:default}.speed{display:flex;gap:14px;align-items:center;margin-top:22px}.speed input{flex:1;min-width:0;accent-color:#1768d1}.pad{max-width:340px;margin:22px auto 10px;display:grid;grid-template-columns:repeat(3,1fr);gap:10px}.drive{height:86px;background:#eaf1fb;color:#1c4d88;font-weight:650;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:4px}.drive span{font-size:29px;line-height:1}.drive.active{background:#1768d1;color:white}.forward{grid-column:2}.left{grid-column:1;grid-row:2}.stop{grid-column:2;grid-row:2;background:#ffe7e6;color:#b8232d;font-weight:700}.right{grid-column:3;grid-row:2}.backward{grid-column:2;grid-row:3}.notice{font-size:14px;min-height:46px;text-align:center;margin:16px 0 0}.keys{font-size:14px;line-height:1.9}.keys b{color:#172941}.footer{font-size:12px;text-align:center;margin-top:18px}.active-mode{border-color:#74b3ee}@media(max-width:380px){main{padding:16px 12px}.card{padding:16px}.drive{height:76px}}
</style>
<style>
.aux{margin-top:20px;padding-top:18px;border-top:1px solid #dfe7f0}.aux p{font-size:14px}.aux button{width:100%;padding:14px;background:#e8f5ef;color:#126b62;font-weight:650}.aux button:disabled{opacity:.5;cursor:default}
.speed{justify-content:space-between}.speed output{font-weight:700;color:#1768d1}.speed-gears{display:grid;grid-template-columns:repeat(5,1fr);gap:6px;margin-top:12px}.gear{padding:12px 2px;background:#eaf1fb;color:#1c4d88;display:flex;flex-direction:column;gap:4px;align-items:center}.gear span{font-size:12px}.gear.selected{background:#1768d1;color:white}.gear:disabled{opacity:.65;cursor:default}.speed-help{font-size:13px;margin-top:10px}
.drive:disabled{opacity:.55;cursor:default}
</style>
</head>
<body><main>
<div class="top"><h1>小车控制</h1><span id="connection" class="chip">正在连接</span></div>
<p>连接 Carplay-ESP32 热点后即可使用。</p>
<section id="panel" class="card">
<div class="mode"><span>控制模式</span><strong id="mode">未激活</strong></div>
<button id="activate" class="activate">激活网页控制</button>
<div class="speed"><span id="speedLabel">网页车轮速度</span><output id="speedValue" aria-live="polite">4档 · 80%</output></div>
<div class="speed-gears" role="group" aria-label="车轮速度档位">
<button class="gear" type="button" data-speed-percent="20" aria-pressed="false"><b>20%</b><span>1档</span></button>
<button class="gear" type="button" data-speed-percent="40" aria-pressed="false"><b>40%</b><span>2档</span></button>
<button class="gear" type="button" data-speed-percent="60" aria-pressed="false"><b>60%</b><span>3档</span></button>
<button class="gear selected" type="button" data-speed-percent="80" aria-pressed="true"><b>80%</b><span>4档</span></button>
<button class="gear" type="button" data-speed-percent="100" aria-pressed="false"><b>100%</b><span>5档</span></button>
</div>
<p id="speedHelp" class="speed-help">点选速度档位，可在行驶时调整。仅影响车轮，不改变新增电机档位。</p>
<div class="pad">
<button class="drive forward" data-direction="forward" aria-label="前进"><span>↑</span>前进</button>
<button class="drive left" data-direction="left" aria-label="原地左旋"><span>↶</span>左旋</button>
<button id="stop" class="stop" aria-label="全部停车并退出">全部停车<br>退出</button>
<button class="drive right" data-direction="right" aria-label="原地右旋"><span>↷</span>右旋</button>
<button class="drive backward" data-direction="backward" aria-label="后退"><span>↓</span>后退</button>
</div>
<div class="aux">
<div class="mode"><span>新增电机</span><strong id="auxState" aria-live="polite">关闭</strong></div>
<button id="auxCycle" type="button" disabled>切换档位</button>
<p>每点一次：关闭 → 1档 → 2档 → 3档 → 关闭。松开行驶键只停车轮；“全部停车”会关闭所有电机。</p>
</div>
<p id="notice" class="notice" role="status" aria-live="polite">先激活，再按住方向按钮；松开即停止车轮。</p>
</section>
<section class="card keys">
<b>电脑键盘</b><br>Ctrl + Enter 激活 · Esc 停车并退出<br>
W / ↑ 前进 · S / ↓ 后退<br>A / ← 原地左旋 · D / → 原地右旋<br>
<b>PS3 手柄</b><br>L1 + R1 激活 · 左摇杆上下前后 · 右摇杆左右旋转<br>□ 切换车轮速度：20% → 40% → 60% → 80% → 100% → 20%<br>△ 切换新增电机档位 · × 全部停车并退出
<p>网页点击“切换到网页控制”即可接管；手柄摇杆回中后按 L1 + R1 可接管网页。每次切换，所有电机先停止。</p>
</section>
<p class="footer">切换控制端先停车 · 切出网页只释放本页控制 · 通信超时自动停车</p>
</main>
<script>
'use strict';
const ui = Object.fromEntries(['activate','stop','speedLabel','speedValue','speedHelp','connection','mode','notice','panel','auxCycle','auxState'].map(id => [id, document.getElementById(id)]));
const buttons = [...document.querySelectorAll('[data-direction]')];
const speedButtons = [...document.querySelectorAll('[data-speed-percent]')];
const keyDirections = {w:'forward',ArrowUp:'forward',s:'backward',ArrowDown:'backward',a:'left',ArrowLeft:'left',d:'right',ArrowRight:'right'};
const held = new Map();
let armed = false, inFlight = false, pendingStop = false, lastSent = -Infinity, lastSignature = '', remoteMode = false, statusBusy = false;
let auxLevel = 0, auxReported = 0;
let webSpeedPercent = 80, ps3SpeedPercent = 80;
let session = 0, claiming = false, controlEpoch = 0;

function draw() {
  ui.mode.textContent = claiming ? '正在切换' : remoteMode ? 'PS3 遥控中' : armed ? '网页控制中' : '未激活';
  ui.activate.disabled = armed || claiming || inFlight || Boolean(pendingStop);
  ui.activate.textContent = claiming ? '正在切换到网页控制……' : armed ? '已激活 · 松开运动键即可停车' : remoteMode ? '切换到网页控制' : '激活网页控制';
  const displayedSpeed = remoteMode ? ps3SpeedPercent : webSpeedPercent;
  ui.speedLabel.textContent = remoteMode ? '手柄车轮速度' : '网页车轮速度';
  ui.speedValue.textContent = `${displayedSpeed / 20}档 · ${displayedSpeed}%`;
  ui.speedHelp.textContent = remoteMode ? `PS3 遥控中，按 □ 切换车轮速度。网页预设为 ${webSpeedPercent}%。` : '点选速度档位，可在行驶时调整。仅影响车轮，不改变新增电机档位。';
  for (const button of speedButtons) {
    const selected = Number(button.dataset.speedPercent) === displayedSpeed;
    button.disabled = remoteMode || claiming;
    button.classList.toggle('selected', selected);
    button.setAttribute('aria-pressed', String(selected));
  }
  ui.auxCycle.disabled = !armed || remoteMode;
  const displayedAux = armed && !remoteMode ? auxLevel : auxReported;
  ui.auxState.textContent = displayedAux ? `${displayedAux}档` : '关闭';
  ui.auxCycle.textContent = displayedAux === 3 ? '切换档位：关闭' : `切换档位：${displayedAux + 1}档`;
  ui.panel.classList.toggle('active-mode', armed);
  for (const button of buttons) {
    button.disabled = !armed;
    button.classList.toggle('active', armed && [...held.values()].includes(button.dataset.direction));
  }
}
function movement() {
  const directions = new Set(held.values()), power = Math.round(255 * webSpeedPercent / 100);
  const turn = (Number(directions.has('right')) - Number(directions.has('left'))) * power;
  const speed = turn ? 0 : (Number(directions.has('forward')) - Number(directions.has('backward'))) * power;
  return {speed, turn};
}
function releaseUrl(token) { return `/release?session=${token}`; }
function queueRelease(token) {
  if (token && !pendingStop) pendingStop = {path:'/release', session:token};
}
function deactivate(message, sendRelease = true) {
  const previousSession = session;
  ++controlEpoch; armed = false; session = 0; held.clear(); auxLevel = 0;
  if (sendRelease) queueRelease(previousSession);
  ui.notice.textContent = message; draw(); void pump();
}
async function activate() {
  if (armed || claiming || pendingStop || inFlight || document.hidden) return;
  if (held.size) { ui.notice.textContent = '请先松开运动键，再激活。'; return; }
  const attempt = ++controlEpoch;
  claiming = true;
  ui.notice.textContent = '正在切换控制权，所有电机会先停止。'; draw();
  const controller = new AbortController(), timer = setTimeout(() => controller.abort(), 400);
  try {
    const response = await fetch('/claim', {method:'POST', cache:'no-store', signal:controller.signal});
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const result = await response.json();
    if (!Number.isInteger(result.session) || result.session <= 0 || result.session > 4294967295) throw new Error('session');
    // 切出网页或点击停车后，迟到的激活响应只能释放自己的会话，不能重新激活。
    if (attempt !== controlEpoch || document.hidden || held.size) {
      queueRelease(result.session);
      if (attempt === controlEpoch && held.size) ui.notice.textContent = '请先松开运动键，再激活。';
    } else {
      session = result.session; armed = true; remoteMode = false;
      auxLevel = 0; auxReported = 0; lastSignature = ''; lastSent = -Infinity;
      ui.notice.textContent = '网页已接管，所有电机已停止；按住方向按钮开始行驶。';
    }
  } catch (error) {
    if (attempt === controlEpoch) {
      ui.notice.textContent = '切换失败，请检查连接后再次点击激活。';
      ui.connection.textContent = '连接中断';
    }
  } finally {
    clearTimeout(timer); claiming = false; draw(); void pump();
  }
}
function stop() {
  pendingStop = {path:'/stop', session:0};
  deactivate('已退出网页控制，正在发送停车指令。');
}
async function pump() {
  if (inFlight) return;
  const stopRequest = pendingStop, isStop = Boolean(stopRequest);
  if (!isStop && !armed) return;
  const command = {...movement(), aux:auxLevel, session}, signature = `${command.speed},${command.turn},${command.aux}`;
  if (!isStop && signature === lastSignature && performance.now() - lastSent < 100) return;
  if (isStop) pendingStop = false;
  const requestEpoch = controlEpoch;
  inFlight = true; lastSent = performance.now(); lastSignature = signature;
  const controller = new AbortController(), timer = setTimeout(() => controller.abort(), 400);
  try {
    const url = isStop ? stopRequest.path === '/stop' ? '/stop' : releaseUrl(stopRequest.session) : `/move?speed=${command.speed}&turn=${command.turn}&ms=500&aux=${command.aux}&session=${command.session}`;
    const response = await fetch(url, {method:'POST', cache:'no-store', signal:controller.signal});
    if (response.status === 409 && !isStop) {
      const result = await response.json();
      if (requestEpoch === controlEpoch && session === command.session) {
        remoteMode = result.code === 'ps3_busy';
        deactivate(remoteMode ? 'PS3 已接管。需要网页控制时，点击“切换到网页控制”。' : '控制权已变更或超时。需要控制时，请重新点击激活。', false);
      }
    } else if (response.status === 409 && isStop && stopRequest.path === '/release') {
      // 会话已失效：旧网页的释放请求不得影响手柄或其他网页。
    } else if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    } else if (requestEpoch === controlEpoch) {
      ui.connection.textContent = '小车已连接';
      if (isStop) {
        if (stopRequest.path === '/stop') { remoteMode = false; auxReported = 0; }
        else if (!remoteMode) auxReported = 0;
        ui.notice.textContent = stopRequest.path === '/stop' ? '所有电机已关闭，已退出控制。' : '已退出网页控制。';
      } else if (armed && session === command.session) {
        auxReported = command.aux;
        remoteMode = false;
        ui.notice.textContent = '已激活：按住行驶，松开停车；旋转操作优先。';
      }
      draw();
    }
  } catch (error) {
    // 不重放失败的运动命令，只释放该会话；固件仍有独立的 500ms 超时保护。
    if (!isStop) queueRelease(command.session);
    if (requestEpoch === controlEpoch) {
      deactivate('通信中断，已退出控制。重新连接热点后，请再次激活。', false);
      ui.connection.textContent = '连接中断';
    }
  } finally {
    clearTimeout(timer); inFlight = false; draw();
    if (pendingStop) void pump();
  }
}
async function refreshStatus() {
  if (statusBusy || claiming || document.hidden) return;
  statusBusy = true;
  const statusEpoch = controlEpoch;
  const controller = new AbortController(), timer = setTimeout(() => controller.abort(), 700);
  try {
    const response = await fetch('/status', {cache:'no-store', signal:controller.signal});
    if (!response.ok) throw new Error('status');
    const status = await response.json();
    if (statusEpoch !== controlEpoch || claiming) return;
    remoteMode = Boolean(status.remote_mode);
    if ([20,40,60,80,100].includes(status.ps3_speed_percent)) ps3SpeedPercent = status.ps3_speed_percent;
    auxReported = Number.isInteger(status.aux_level) && status.aux_level >= 0 && status.aux_level <= 3 ? status.aux_level : 0;
    ui.connection.textContent = status.ps3_online ? '小车在线 · 手柄在线' : '小车已连接';
    if (armed && (remoteMode || status.wifi_session !== session)) deactivate(remoteMode ? 'PS3 已接管。需要网页控制时，点击“切换到网页控制”。' : '控制权已变更或超时。需要控制时，请重新点击激活。', false);
    draw();
  } catch (error) {
    if (!armed) ui.connection.textContent = '等待小车连接';
  } finally { clearTimeout(timer); statusBusy = false; }
}
ui.activate.addEventListener('click', activate);
ui.stop.addEventListener('click', stop);
ui.auxCycle.addEventListener('click', () => {
  if (!armed || remoteMode || document.hidden) return;
  auxLevel = (auxLevel + 1) % 4;
  draw(); void pump();
});
for (const button of speedButtons) button.addEventListener('click', () => {
  if (remoteMode || claiming || document.hidden) return;
  webSpeedPercent = Number(button.dataset.speedPercent);
  draw(); void pump();
});
window.addEventListener('keydown', event => {
  if (event.key === 'Escape') { event.preventDefault(); stop(); return; }
  if (event.ctrlKey && event.key === 'Enter') { event.preventDefault(); if (!event.repeat) activate(); return; }
  const key = event.key.length === 1 ? event.key.toLowerCase() : event.key;
  if (!keyDirections[key]) return;
  event.preventDefault(); held.set(`key:${key}`, keyDirections[key]); draw(); void pump();
});
window.addEventListener('keyup', event => {
  const key = event.key.length === 1 ? event.key.toLowerCase() : event.key;
  if (!keyDirections[key]) return;
  event.preventDefault(); held.delete(`key:${key}`); draw(); void pump();
});
for (const button of buttons) {
  button.addEventListener('contextmenu', event => event.preventDefault());
  button.addEventListener('pointerdown', event => {
    if (!armed) { ui.notice.textContent = '请先点击“激活网页控制”。'; return; }
    if (event.pointerType === 'mouse' && event.button !== 0) return;
    event.preventDefault(); button.setPointerCapture(event.pointerId);
    held.set(`pointer:${event.pointerId}`, button.dataset.direction); draw(); void pump();
  });
  const release = event => { held.delete(`pointer:${event.pointerId}`); draw(); void pump(); };
  button.addEventListener('pointerup', release);
  button.addEventListener('pointercancel', release);
  button.addEventListener('lostpointercapture', release);
}
window.addEventListener('blur', () => deactivate('网页失去焦点，已退出控制。'));
document.addEventListener('visibilitychange', () => {
  if (document.hidden) deactivate('网页已切到后台，已退出控制。');
  else void refreshStatus();
});
window.addEventListener('pagehide', () => {
  const exiting = pendingStop || (session ? {path:'/release', session} : null);
  if (exiting) navigator.sendBeacon(exiting.path === '/stop' ? '/stop' : releaseUrl(exiting.session), '');
  ++controlEpoch; armed = false; session = 0; held.clear(); auxLevel = 0;
});
setInterval(() => { void pump(); }, 40);
setInterval(() => { void refreshStatus(); }, 1000);
draw(); void refreshStatus();
</script>
</body></html>)CARPAGE";
