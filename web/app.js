import createMorse from './generated/morse.mjs';
import { Engine } from './engine.js';
import { hello, runScenarios } from './scenarios.js';

const $ = selector => document.querySelector(selector);
const views = [], shortcuts = [['q', 'e'], ['i', 'p']];
const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,:?\'-/()"=+@';
// Reference card only. Decoding always occurs in the shared C++ core.
const codes = ['.-','-...','-.-.','-..','.','..-.','--.','....','..','.---','-.-','.-..','--','-.','---','.--.','--.-','.-.','...','-','..-','...-','.--','-..-','-.--','--..','-----','.----','..---','...--','....-','.....','-....','--...','---..','----.','.-.-.-','--..--','---...','..--..','.----.','-....-','-..-.','-.--.','-.--.-','.-..-.','-...-','.-.-.','--.-.'];
for (let i = 0; i < alphabet.length; i++) {
  const cell = document.createElement('div'), letter = document.createElement('b'), code = document.createElement('span');
  letter.textContent = alphabet[i]; code.textContent = codes[i]; cell.append(letter, code); $('#alphabet').append(cell);
}
for (let d = 0; d < 2; d++) {
  const view = $('#device-template').content.firstElementChild.cloneNode(true);
  view.dataset.device = d;
  view.querySelector('.device-code').textContent = `COMMUNICATOR 0${d + 1}`;
  view.querySelector('.device-name').textContent = `Device ${d ? 'B' : 'A'}`;
  view.querySelector('canvas').setAttribute('aria-label', `Device ${d ? 'B' : 'A'} screen`);
  for (const button of view.querySelectorAll('.pad')) {
    const key = Number(button.dataset.key);
    button.querySelector('kbd').textContent = shortcuts[d][key].toUpperCase();
    button.setAttribute('aria-label', `${d ? 'B' : 'A'} ${['DOT', 'DASH'][key]}`);
    button.disabled = true;
  }
  view.querySelector('.chord').disabled = true;
  $('.devices').append(view); views.push(view);
}

const color = rgb => `rgb(${Math.round(((rgb >> 11) & 31) * 255 / 31)},${Math.round(((rgb >> 5) & 63) * 255 / 63)},${Math.round((rgb & 31) * 255 / 31)})`;
let engine, demoQueue = [], demoActive = false, landscape = false;
const sources = Array.from({ length: 2 }, () => Array.from({ length: 2 }, () => new Set()));
const lastFrames = ['', ''], lastAccessible = ['', ''];

function input(d, key, down, source) {
  if (!engine) return;
  const set = sources[d][key];
  if (down) set.add(source); else set.delete(source);
  engine.button(d, key, set.size > 0);
  views[d].querySelector(`[data-key="${key}"]`).classList.toggle('held', set.size > 0);
}
function releaseAll() {
  if (!engine) return;
  for (let d = 0; d < 2; d++) for (let k = 0; k < 2; k++) {
    sources[d][k].clear(); engine.button(d, k, false);
    views[d].querySelector(`[data-key="${k}"]`).classList.remove('held');
  }
  for (const view of views) view.querySelector('.chord').classList.remove('held');
}
function bindPad(button, d, keys) {
  const hold = (down, source) => {
    if (demoActive) return;
    for (const key of keys) input(d, key, down, source);
    button.classList.toggle('held', down);
  };
  button.addEventListener('pointerdown', event => {
    if (event.button !== 0) return;
    event.preventDefault(); button.focus(); button.setPointerCapture(event.pointerId);
    hold(true, `pointer-${event.pointerId}`);
  });
  const up = event => hold(false, `pointer-${event.pointerId}`);
  button.addEventListener('pointerup', up); button.addEventListener('pointercancel', up);
  button.addEventListener('lostpointercapture', up);
  button.addEventListener('keydown', event => {
    if ([' ', 'Enter'].includes(event.key)) { event.preventDefault(); if (!event.repeat) hold(true, 'activation'); }
  });
  button.addEventListener('keyup', event => {
    if ([' ', 'Enter'].includes(event.key)) { event.preventDefault(); hold(false, 'activation'); }
  });
  button.addEventListener('blur', () => hold(false, 'activation'));
}
for (let d = 0; d < 2; d++) {
  for (const button of views[d].querySelectorAll('.pad')) bindPad(button, d, [Number(button.dataset.key)]);
  bindPad(views[d].querySelector('.chord'), d, [0, 1]);
}
for (const type of ['keydown', 'keyup']) document.addEventListener(type, event => {
  if (!engine || demoActive || event.ctrlKey || event.metaKey || event.altKey || event.repeat || event.target.tagName === 'INPUT') return;
  for (let d = 0; d < 2; d++) {
    const key = shortcuts[d].indexOf(event.key.toLowerCase());
    if (key >= 0) { event.preventDefault(); input(d, key, type === 'keydown', 'shortcut'); }
  }
});
window.addEventListener('blur', releaseAll);
document.addEventListener('visibilitychange', () => { if (document.hidden) releaseAll(); });

function logEvents(events) {
  for (const e of events) {
    if ((e.type === 3 || e.type === 4) && !e.dropped && !e.rejected) continue;
    $('#log .empty-log')?.remove();
    const row = document.createElement('li'), time = document.createElement('time'), message = document.createElement('span');
    time.textContent = `${(e.at / 1000).toFixed(2)}s`;
    const name = { 1: 'TEXT', 2: 'ACK', 3: 'PING', 4: 'PONG' }[e.type];
    message.textContent = e.rejected ? 'Authentication / replay check rejected packet' :
      `${e.from ? 'B → A' : 'A → B'}  ${name} #${e.id}  ${e.dropped ? 'DROPPED' : 'encrypted · 109 B'}`;
    if (e.dropped || e.rejected) row.className = 'dropped';
    row.append(time, message); $('#log').prepend(row);
    while ($('#log').children.length > 60) $('#log').lastElementChild.remove();
  }
}
function paint() {
  for (let d = 0; d < 2; d++) {
    const state = engine.state(d), frame = engine.frame(d, landscape), serialized = JSON.stringify(frame), view = views[d];
    if (serialized !== lastFrames[d]) {
      const ctx = view.querySelector('canvas').getContext('2d');
      ctx.setTransform(3, 0, 0, 3, 0, 0); ctx.textBaseline = 'top';
      for (const draw of frame) {
        if (draw.clear !== undefined) { ctx.fillStyle = color(draw.clear); ctx.fillRect(0, 0, landscape ? 240 : 135, landscape ? 135 : 240); continue; }
        ctx.fillStyle = color(draw.color);
        if (draw.dial) {
          ctx.lineWidth = 1.4; ctx.beginPath(); ctx.arc(draw.x, draw.y, 8, -Math.PI / 2, -Math.PI / 2 + (Math.PI * 2 * draw.progress / 100)); ctx.strokeStyle = color(draw.color); ctx.stroke();
          ctx.beginPath(); ctx.arc(draw.x, draw.y, 8, 0, Math.PI * 2); ctx.strokeStyle = '#555'; ctx.lineWidth = 0.5; ctx.stroke();
          continue;
        }
          if (draw.bars) {
            for (let bar = 0; bar < 3; bar++) {
              const height = (bar + 1) * 2;
              ctx.fillStyle = color(bar < draw.level ? draw.color : 0x4a69);
              ctx.fillRect(draw.x + bar * 4, draw.y + 6 - height, 3, height);
            }
            continue;
          }
        ctx.font = `${draw.size * 8}px monospace`;
        // Fixed six-pixel advance, matching the firmware GLCD layout.
        for (let i = 0; i < draw.text.length; i++) ctx.fillText(draw.text[i], draw.x + i * draw.size * 6, draw.y);
      }
      lastFrames[d] = serialized;
    }
    const status = view.querySelector('.delivery'); status.textContent = state.delivery; status.dataset.state = state.delivery;
    view.querySelector('.attempts').textContent = state.attempts ? `Attempt ${state.attempts}/4 · message #${state.messageId}` : 'No message sent';
    const badge = view.querySelector('.peer-badge'); badge.textContent = state.displayIdle ? 'DISPLAY IDLE · RADIO ON' : (state.connected ? 'LINK · PEER ONLINE' : 'UNLINK · NO PEER REPLY'); badge.classList.toggle('connected', state.connected);
    const batteryDescription = state.batteryValid ? `simulated battery estimate ${state.batteryPercent} percent` : 'simulated battery estimate unavailable';
    const accessible = `Device ${d ? 'B' : 'A'}. ${state.displayIdle ? 'Display idle; ESP-NOW radio remains active' : (state.connected ? 'Connected' : 'Disconnected')}. Signal strength ${state.signalBars} of 3 bars. ${batteryDescription}. ${state.mode}. ${state.delivery}. ${state.unreadCount} unread of ${state.inboxCount} saved; approximately ${state.inboxRemaining} slots remain. Morse ${state.sequence || 'empty'}. Draft ${state.draft || 'empty'}.${state.mode === 'READING' ? ` Message ${state.inboxPosition} of ${state.inboxCount}. Received ${state.received}.` : ''} ${state.notice}`;
    if (accessible !== lastAccessible[d]) { view.querySelector('.screen-reader').textContent = accessible; lastAccessible[d] = accessible; }
  }
  logEvents(engine.events());
}
function demo() {
  if (demoActive) return;
  releaseAll(); engine.reset(); $('#offline').checked = false; $('#log').replaceChildren();
  demoActive = true; $('#demo').disabled = true;
  let at = 200;
  const press = (keys, duration = 100) => {
    for (const key of keys) demoQueue.push({ at, key, down: true });
    at += duration;
    for (const key of keys) demoQueue.push({ at, key, down: false });
    at += 100;
  };
  for (const letter of hello) {
    for (const s of letter) press([s === '.' ? 0 : 1]);
    at += 1100;
  }
  press([0, 1], 950); demoQueue.push({ at: at + 300, done: true });
}
$('#demo').addEventListener('click', demo);
$('#drop-text').addEventListener('click', () => { engine.drop(0, 1); $('#fault-status').textContent = 'Armed: next A → B text will be dropped.'; });
$('#drop-ack').addEventListener('click', () => { engine.drop(1, 2); $('#fault-status').textContent = 'Armed: next B → A receipt ACK will be dropped.'; });
$('#tamper').addEventListener('click', () => { engine.wasm._sim_tamper(); $('#fault-status').textContent = 'Armed: next packet will be modified after encryption.'; });
$('#offline').addEventListener('change', event => { engine.offline(event.target.checked); $('#fault-status').textContent = event.target.checked ? 'Disconnected. LINK expires within 6 seconds.' : 'Link restored. Waiting for an authenticated peer reply.'; });
$('#landscape').addEventListener('change', event => {
  landscape = event.target.checked;
  for (const [d, view] of views.entries()) {
    view.classList.toggle('landscape', landscape);
    const canvas = view.querySelector('canvas');
    canvas.width = landscape ? 720 : 405; canvas.height = landscape ? 405 : 720;
    view.querySelector('.resolution').textContent = landscape ? '240 × 135' : '135 × 240';
    lastFrames[d] = '';
  }
});
$('#clear-log').addEventListener('click', () => $('#log').replaceChildren());
$('#reset').addEventListener('click', () => {
  releaseAll(); demoQueue = []; demoActive = false; engine.reset(); $('#demo').disabled = false;
  $('#offline').checked = false; $('#log').replaceChildren(); $('#fault-status').textContent = 'New simulated pair and fresh random key.';
});
$('#checks').addEventListener('click', async () => {
  $('#checks').disabled = true; $('#test-result').textContent = 'Running isolated encrypted-delivery scenarios…';
  try {
    const results = runScenarios(await createMorse());
    window.morseCheckResults = results;
    const failed = results.filter(r => !r.passed);
    $('#test-result').textContent = failed.length ? failed.map(r => `${r.name}: ${r.error}`).join(' / ') : `${results.length}/${results.length} checks passed: inbox, deletion, delivery, retry, two buttons and tampering.`;
  } catch (error) { $('#test-result').textContent = error.message; }
  finally { $('#checks').disabled = false; }
});

try {
  engine = new Engine(await createMorse());
  document.querySelectorAll('button:disabled,input:disabled').forEach(el => el.disabled = false);
  $('#loading').textContent = '';
  // Read-only inspection for browser QA. User actions still go through the actual controls.
  window.morseLab = { state: d => engine.state(d) };
  let previous = performance.now(), accumulated = 0;
  function animate(now) {
    if (!document.hidden) {
      accumulated += Math.min(100, now - previous);
      while (accumulated >= 10) {
        while (demoQueue.length && demoQueue[0].at <= engine.time) {
          const event = demoQueue.shift();
          if (event.done) { demoActive = false; $('#demo').disabled = false; }
          else input(0, event.key, event.down, 'demo');
        }
        engine.step(10); accumulated -= 10;
      }
      paint();
    }
    previous = now; requestAnimationFrame(animate);
  }
  paint(); requestAnimationFrame(animate);
} catch (error) {
  $('#loading').textContent = `Engine unavailable: ${error.message}. Run npm run build, then npm start.`;
  console.error(error);
}
