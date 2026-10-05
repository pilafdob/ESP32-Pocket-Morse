import { Engine } from './engine.js';
export const hello = ['....', '.', '.-..', '.-..', '---'];
export function compose(engine, device, letters = hello) {
  for (const letter of letters) {
    for (const symbol of letter) engine.press(device, symbol === '.' ? 0 : 1);
    engine.step(1050);
  }
}
export function send(engine, device) {
  engine.button(device, 0, true); engine.button(device, 1, true); engine.step(950);
  engine.button(device, 0, false); engine.button(device, 1, false); engine.step(60);
}
export function accept(engine, device) {
  const bothTap = () => {
    engine.button(device, 0, true); engine.button(device, 1, true); engine.step(90);
    engine.button(device, 0, false); engine.button(device, 1, false); engine.step(60);
  };
  if (engine.state(device).mode === 'COMPOSE' && engine.state(device).unreadCount) {
    bothTap(); bothTap(); engine.step(500); return;
  }
  bothTap(); engine.step(500);
}
export function runScenarios(wasm) {
  const engine = new Engine(wasm), results = [];
  const check = (condition, message) => { if (!condition) throw new Error(message); };
  const scenario = (name, callback) => {
    engine.reset();
    try { callback(); results.push({ name, passed: true }); }
    catch (error) { results.push({ name, passed: false, error: error.message }); }
  };
  scenario('Two-button HELLO, encrypted receipt, accept-to-read, compose again', () => {
    compose(engine, 0); check(engine.state(0).draft === 'HELLO', 'Composition mismatch');
    send(engine, 0); engine.step(250);
    check(engine.state(1).mode === 'COMPOSE' && engine.state(1).received === '' &&
      engine.state(1).unreadCount === 1 && engine.frame(1).some(item => item.text === 'NEW MSG'),
      'Incoming notification interrupted compose or was not shown');
    check(engine.state(0).delivery === 'DELIVERED' && engine.state(0).attempts === 1, 'Receipt missing');
    accept(engine, 1); check(engine.state(1).received === 'HELLO', 'Accepted text missing');
    accept(engine, 1); check(engine.state(1).mode === 'COMPOSE', 'Did not return to compose');
  });
  scenario('Dropped text retries successfully', () => {
    compose(engine, 0); engine.drop(0, 1); send(engine, 0); engine.step(1100);
    check(engine.state(0).delivery === 'DELIVERED' && engine.state(0).attempts === 2, 'Retry failed');
    check(engine.state(1).receivedCount === 1, 'Wrong receive count');
  });
  scenario('Dropped encrypted ACK does not duplicate the message', () => {
    compose(engine, 0); engine.drop(1, 2); send(engine, 0); engine.step(1100);
    check(engine.state(0).delivery === 'DELIVERED', 'Repeated text not ACKed');
    check(engine.state(1).receivedCount === 1 && engine.state(1).duplicates === 1, 'Duplicate handling failed');
  });
  scenario('Disconnected send keeps its draft and identity for retry', () => {
    compose(engine, 0); engine.offline(true); send(engine, 0); engine.step(3000);
    check(engine.state(0).delivery === 'FAILED' && engine.state(0).attempts === 4, 'Expected final failure');
    check(engine.state(0).draft === 'HELLO', 'Failed draft lost');
    const id = engine.state(0).messageId;
    engine.offline(false); send(engine, 0); engine.step(250);
    check(engine.state(0).delivery === 'DELIVERED' && engine.state(0).messageId === id, 'Recovery changed identity');
  });
  scenario('Both devices send simultaneously and accept separately', () => {
    compose(engine, 0, ['...', '---', '...']); compose(engine, 1, ['---', '-.-']);
    for (let d = 0; d < 2; d++) { engine.button(d, 0, true); engine.button(d, 1, true); }
    engine.step(950);
    for (let d = 0; d < 2; d++) { engine.button(d, 0, false); engine.button(d, 1, false); }
    engine.step(300);
    check(engine.state(0).delivery === 'DELIVERED' && engine.state(1).delivery === 'DELIVERED', 'Bidirectional ACK failed');
    accept(engine, 0); accept(engine, 1);
    check(engine.state(0).received === 'OK' && engine.state(1).received === 'SOS', 'Bidirectional receive failed');
  });
  scenario('Authenticated heartbeat connects, times out and reconnects', () => {
    engine.step(400); check(engine.state(0).connected && engine.state(1).connected, 'No peer link');
    engine.offline(true); engine.step(6500);
    check(!engine.state(0).connected && !engine.state(1).connected, 'Stale connection');
    engine.offline(false); engine.step(2400);
    check(engine.state(0).connected && engine.state(1).connected, 'No reconnect');
  });
  scenario('Tampered ciphertext is rejected; clean traffic recovers', () => {
    engine.wasm._sim_tamper(); engine.step(400);
    check(engine.events().some(e => e.rejected), 'Corrupt packet was not rejected');
    engine.step(2400); check(engine.state(0).connected && engine.state(1).connected, 'Recovery failed');
  });
  scenario('An incoming message preserves the recipient’s draft', () => {
    compose(engine, 1, ['...', '---']); compose(engine, 0); send(engine, 0); engine.step(250);
    check(engine.state(1).draft === 'SO' && engine.state(1).mode === 'COMPOSE' &&
      engine.state(1).unreadCount === 1 && engine.frame(1).some(item => item.text === 'NEW MSG'),
      'Incoming message interrupted the draft or notification was missing');
    accept(engine, 1); accept(engine, 1);
    check(engine.state(1).draft === 'SO' && engine.state(1).mode === 'COMPOSE', 'Recipient draft lost');
  });
  scenario('Multiple messages queue, remain hidden, and can be browsed', () => {
    compose(engine, 0, ['.']); send(engine, 0); engine.step(250);
    compose(engine, 0, ['-']); send(engine, 0); engine.step(3000);
    check(engine.state(0).delivery === 'DELIVERED' && engine.state(1).inboxCount === 2 &&
      engine.state(1).unreadCount === 2 && engine.state(1).received === '', 'Inbox did not queue two hidden messages');
    accept(engine, 1); check(engine.state(1).received === 'E' && engine.state(1).unreadCount === 1 && engine.state(1).inboxPosition === 1, 'First message missing');
    engine.press(1, 1); check(engine.state(1).received === 'T' && engine.state(1).unreadCount === 0 && engine.state(1).inboxPosition === 2, 'Second message missing');
    engine.press(1, 0); check(engine.state(1).received === 'E' && engine.state(1).inboxPosition === 1, 'Previous message missing');
    accept(engine, 1); check(engine.state(1).mode === 'COMPOSE', 'Compose not restored');
    const tapBoth = () => { engine.button(1, 0, true); engine.button(1, 1, true); engine.step(80);
      engine.button(1, 0, false); engine.button(1, 1, false); engine.step(60); };
    tapBoth(); tapBoth(); engine.step(500);
    check(engine.state(1).mode === 'READING' && engine.state(1).received === 'E' &&
      engine.state(1).inboxPosition === 1, 'Double tap did not reopen saved read message');
  });
  scenario('Unread inbox remains accessible without losing a draft', () => {
    compose(engine, 1, ['...', '---']);
    compose(engine, 0, ['.']); send(engine, 0); engine.step(250);
    compose(engine, 0, ['-']); send(engine, 0); engine.step(250);
    accept(engine, 1); accept(engine, 1);
    check(engine.state(1).draft === 'SO' && engine.state(1).unreadCount === 1, 'Draft or unread message lost');
    const tapBoth = () => { engine.button(1, 0, true); engine.button(1, 1, true); engine.step(80);
      engine.button(1, 0, false); engine.button(1, 1, false); engine.step(60); };
    tapBoth(); tapBoth(); engine.step(500);
    check(engine.state(1).mode === 'READING' && engine.state(1).draft === 'SO' &&
      engine.state(1).received === 'T', 'Unread inbox shortcut failed');
  });
  scenario('Holding DOT immediately deletes the currently open message', () => {
    compose(engine, 0, ['.']); send(engine, 0); engine.step(250); accept(engine, 1);
    engine.press(1, 0, 900);
    check(engine.state(1).inboxCount === 0 && engine.state(1).mode === 'COMPOSE' &&
      engine.state(1).notice === 'MESSAGE DELETED', 'Hold DOT did not immediately delete the message');
  });
  scenario('Auto-confirm timer, word/letter erasure, recall and dictionary', () => {
    engine.press(0, 0); engine.step(200);
    check(engine.state(0).progress > 30 && engine.state(0).progress < 70, 'Dial did not fill');
    engine.step(280); check(engine.state(0).draft === 'E', 'Letter did not auto-confirm after 500 ms');
    engine.press(0, 1, 900); check(engine.state(0).draft === '', 'Hold DASH did not erase letter');
    compose(engine, 0, ['.', '-']); check(engine.state(0).draft === 'ET', 'Composition failed');
    engine.press(0, 0, 900); check(engine.state(0).draft === '', 'Hold DOT did not erase word');
    compose(engine, 0, ['.']); send(engine, 0); engine.step(250);
    check(engine.state(0).delivery === 'DELIVERED', 'Message not delivered');
    const chord = () => { engine.button(0, 0, true); engine.button(0, 1, true); engine.step(80);
      engine.button(0, 0, false); engine.button(0, 1, false); engine.step(60); };
    chord(); chord(); chord(); engine.step(500); check(engine.state(0).draft === 'E', 'Last sent not recalled');
    chord(); chord(); chord(); chord(); check(engine.state(0).mode === 'COMPOSE', 'Dictionary opened before the gesture settled');
    engine.step(800); check(engine.state(0).mode === 'DICTIONARY', 'Dictionary did not open after the delay');
    engine.press(0, 1); check(engine.state(0).dictionaryPage === 1, 'Dictionary did not page');
    chord(); check(engine.state(0).mode === 'COMPOSE', 'Dictionary did not close');
  });
  scenario('Display idle keeps the peer link alive and incoming traffic wakes the screen', () => {
    engine.step(400);
    check(engine.state(0).connected && engine.state(1).connected, 'Peer link not established');
    engine.step(60000);
    check(engine.state(0).displayIdle && engine.state(1).displayIdle, 'Displays did not enter idle');
    check(engine.state(0).connected && engine.state(1).connected, 'Peer link expired during display idle');
    engine.press(0, 0); // First press wakes Device A; it is intentionally consumed.
    check(!engine.state(0).displayIdle, 'Button did not wake Device A');
    engine.press(0, 0); engine.step(550);
    check(engine.state(0).draft === 'E', 'Could not compose after waking Device A');
    send(engine, 0); engine.step(300);
    check(engine.state(1).receivedCount === 1 && !engine.state(1).displayIdle,
      'Incoming encrypted message did not wake Device B');
    check(engine.state(0).connected && engine.state(1).connected, 'Peer link did not remain/recover after wake');
  });
  scenario('Blinking draft cursor tracks insertion point after a space', () => {
    const caret = (x, y) => engine.frame(0).some(command => command.text === '_' && command.x === x && command.y === y);
    check(engine.state(0).cursorVisible && caret(5, 113), 'Initial portrait draft cursor missing');
    engine.step(500);
    check(!engine.state(0).cursorVisible && !caret(5, 113), 'Draft cursor did not blink off');
    compose(engine, 0, ['.']);
    check(engine.state(0).draft === 'E', 'Failed to compose before cursor space check');
    const tapBoth = () => { engine.button(0, 0, true); engine.button(0, 1, true); engine.step(80);
      engine.button(0, 0, false); engine.button(0, 1, false); engine.step(60); };
    tapBoth(); engine.step(500);
    check(engine.state(0).draft === 'E ', 'BOTH tap did not add a visible trailing space');
    if (!engine.state(0).cursorVisible) engine.step(500);
    check(engine.state(0).cursorVisible && caret(17, 113), 'Cursor did not follow the draft after its space');
  });
  scenario('Punctuation is Morse on the encrypted wire and JSON-safe', () => {
    compose(engine, 0, ['.-..-.', '..--..']);
    check(engine.state(0).draft === '"?', 'Punctuation composition failed');
    check(engine.frame(0).length > 0, 'Screen JSON failed');
    send(engine, 0); engine.step(250); accept(engine, 1);
    check(engine.state(1).received === '"?', 'Punctuation transport failed');
  });
  return results;
}
