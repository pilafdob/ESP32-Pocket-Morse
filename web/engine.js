// Only a bridge and a clock: all application decisions live in C++.
export class Engine {
  constructor(wasm) { this.wasm = wasm; this.reset(); }
  reset() { this.time = 0; this.wasm._sim_reset(); this.wasm._sim_tick(0); }
  step(ms = 10) {
    const target = this.time + ms;
    while (this.time < target) {
      this.time = Math.min(this.time + 10, target);
      this.wasm._sim_tick(this.time >>> 0);
    }
  }
  button(device, button, down) { this.wasm._sim_button(device, button, down ? 1 : 0); }
  press(device, button, duration = 80) {
    this.button(device, button, true); this.step(duration);
    this.button(device, button, false); this.step(60);
  }
  drop(from, type) { this.wasm._sim_drop(from, type); }
  offline(value) { this.wasm._sim_offline(value ? 1 : 0); }
  state(device) { return JSON.parse(this.wasm.UTF8ToString(this.wasm._sim_state(device))); }
  frame(device, landscape = false) { return JSON.parse(this.wasm.UTF8ToString(this.wasm._sim_frame(device, landscape ? 1 : 0))); }
  events() {
    const events = [];
    for (;;) {
      const value = this.wasm.UTF8ToString(this.wasm._sim_event());
      if (!value) return events;
      events.push(JSON.parse(value));
    }
  }
}
