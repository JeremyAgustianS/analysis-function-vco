# VCV Rack Plugin Development Guide (MyPlugin trial)

This is a step-by-step record of how this trial plugin was set up and how to keep building on it.

**Paths used in this guide**

| Thing | Path |
|---|---|
| Rack SDK | `C:\Users\Wanyin\Desktop\PROJECTS\Rack-SDK` |
| This plugin repo | `C:\Users\Wanyin\Desktop\PROJECTS\MyPlugin` |
| Installed plugins | `C:\Users\Wanyin\AppData\Local\Rack2\plugins-win-x64\` |
| Rack log (errors) | `C:\Users\Wanyin\AppData\Local\Rack2\log.txt` |

Official refs:

- [Plugin Development Tutorial](https://vcvrack.com/manual/PluginDevelopmentTutorial)
- [Panel Guide](https://vcvrack.com/manual/Panel)
- [Voltage Standards](https://vcvrack.com/manual/VoltageStandards)
- [Building / env setup](https://vcvrack.com/manual/Building)

---

## Mental model (read this first)

```
Plugin  = the whole package / “repo” (MyPlugin)
Module  = one instrument/utility inside it (MyModule = your sine tester)
Panel   = SVG faceplate artwork + invisible position markers
DSP     = C++ process() code that runs every audio sample
```

- **One plugin** can contain **many modules** (VCO, VCA, filter, sequencer…).
- **One module** = usually **one** `.cpp` file (e.g. `MyModule.cpp`), **one** SVG panel, **one** entry in `plugin.json`.
- **Many knobs on one module** do **not** mean many `.cpp` files. They are just more params inside the same module.
- **Sine vs square vs saw** are different *algorithms* (or modes) inside a module — not automatically different modules. You could:
  - put wave-shape choice on one module (one knob/switch), or
  - ship separate modules (`SineVCO`, `SquareVCO`) if you want them as separate browser entries.

In this trial:

| Name | What it is |
|---|---|
| `MyPlugin` | Plugin **slug** / folder / repo name |
| `Test-Plugin` | Display name in Rack |
| `tpuwu` | Brand prefix in the Module Browser |
| `MyModule` | Module **slug** (technical ID in code / JSON) |
| `test_module` | Module display name |

`MyModule` is **not** “the sine algorithm.” It is just the name of this first module. The sine code lives in its `process()` function. You could rename the module later when you make a real plugin, or create a new plugin with a proper slug.

---

## 2. Point at the Rack SDK

All build / helper commands need `RACK_DIR`.

Open **MSYS2 MinGW 64-bit** (Start menu → search `mingw`), then:

```bash
export RACK_DIR=/c/Users/Wanyin/Desktop/PROJECTS/Rack-SDK
```

Optional: add that line to `~/.bashrc` so it persists.

> Use **MinGW 64-bit**, not PowerShell and not plain “MSYS2 MSYS”.

---

## 3. Create the plugin repo (`MyPlugin`)

`MyPlugin` is the **repo / plugin slug** — the folder name and unique ID. Fine for a trial. For a real release later, create a new plugin with the proper slug instead of renaming this one carelessly (slug becomes painful to change after Library publishing).

```bash
cd /c/Users/Wanyin/Desktop/PROJECTS
$RACK_DIR/helper.py createplugin MyPlugin
cd MyPlugin
```

`helper.py` asks for name, author, brand, etc., and writes `plugin.json`.

### Version must be `2.x.x`

For **Rack 2**, the plugin `"version"` in `plugin.json` **must start with `2`**.

```json
"version": "2.0.0"
```

If you use `0.0.1`, Rack finds the plugin then refuses it:

```text
Plugin version 0.0.1 does not match Rack ABI version 2
```

Then smoke-test the toolchain:

```bash
make
```

An empty plugin (zero modules) is OK for this check.

---

## 4. Create the panel SVG (faceplate)

### What a panel is

The panel is an SVG:

1. **Visible artwork** — background, labels, decorations.
2. **Hidden `components` layer** — colored shapes that mark *where* widgets go. Rack draws the real knobs/jacks; the circles are only position markers for `helper.py`.

Usually made in **Inkscape**. For testing, a minimal SVG is enough (this repo has `res/MyModule.svg`).

### Document setup (Inkscape)

- Units: **mm** (not px)
- Height: **128.5 mm**
- Width: multiple of **5.08 mm** (1 HP). Example: `20.32` mm = 4 HP

### Component colors

| Color | Hex | Means |
|---|---|---|
| Red | `#ff0000` | Param (knob, switch, button, slider) |
| Green | `#00ff00` | Input jack |
| Blue | `#0000ff` | Output jack |
| Magenta | `#ff00ff` | Light / LED |
| Yellow | `#ffff00` | Custom widget (you draw in C++) |

- Prefer **circles** centered on the control → generates `create*Centered(...)`.
- Name each shape in Object Properties → Label (e.g. `PITCH`, `FREQ`, `GATE`). Press Set/Enter.
- Convert text to paths before shipping (`Path → Object to Path`).
- **Hide** the `components` layer, save to `res/<ModuleSlug>.svg`.

### Multiple circles of the same color

Same color only means same *type*. Identity comes from the **label/name** and position.

Example — two red knobs and two green inputs:

| Shape | Color | Label | Becomes in C++ |
|---|---|---|---|
| circle | red | `PITCH` | `PITCH_PARAM` |
| circle | red | `LEVEL` | `LEVEL_PARAM` |
| circle | green | `PITCH` | `PITCH_INPUT` |
| circle | green | `FM` | `FM_INPUT` |
| circle | blue | `SINE` | `SINE_OUTPUT` |
| circle | blue | `SQUARE` | `SQUARE_OUTPUT` |

Rules:

1. Give every control a **unique label** within its type (don’t name two knobs both `KNOB` unless you like confusing enums).
2. Place them where you want them on the panel; `helper.py` copies `cx/cy` into `mm2px(Vec(...))`.
3. Same label on different types is fine and common (`PITCH` param + `PITCH` input).
4. After regenerating, each named control gets its own enum entry and its own `params[...]` / `inputs[...]` / `outputs[...]` index — still **one** module `.cpp`.

---

## 5. Generate the module source (`createmodule`)

Do this in **MinGW 64-bit**, inside the plugin repo, with `RACK_DIR` set:

```bash
cd /c/Users/Wanyin/Desktop/PROJECTS/MyPlugin
export RACK_DIR=/c/Users/Wanyin/Desktop/PROJECTS/Rack-SDK
$RACK_DIR/helper.py createmodule MyModule res/MyModule.svg src/MyModule.cpp
```

What it does:

1. Adds a module entry to `plugin.json` (slug `MyModule`, plus the name/tags you type).
2. Parses the SVG `components` layer.
3. Writes `src/MyModule.cpp` with enums, widget layout, and an empty `process()`.

If the SVG is missing:

```text
Panel not found at res/MyModule.svg.
```

Use tags from the [manifest tag list](https://vcvrack.com/manual/Manifest#modules-tags) (e.g. `Oscillator`, `VCO`), not random words like `hi`.

---

## 6. Register the module with the plugin

`createmodule` does **not** finish registration. You must wire it.

### `src/plugin.hpp`

Keep **both** declarations:

```cpp
extern Plugin* pluginInstance;
extern Model* modelMyModule;
```

### `src/plugin.cpp`

Call `addModel` **inside** `init()`:

```cpp
void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelMyModule);
}
```

For a second module later (example `MyFilter`):

```cpp
extern Model* modelMyFilter;          // plugin.hpp
p->addModel(modelMyFilter);           // plugin.cpp init()
```

Plus its own `src/MyFilter.cpp` + `res/MyFilter.svg` + `plugin.json` entry.

---

## 7. Writing algorithms (DSP) — step by step

### 7.1 What runs when

Every module class inherits `Module` and implements:

```cpp
void process(const ProcessArgs& args) override {
    // runs once per sample, e.g. 44100 or 48000 times per second
}
```

Useful fields:

| Thing | Meaning |
|---|---|
| `args.sampleTime` | Seconds per sample (`1 / sampleRate`) |
| `args.sampleRate` | Sample rate in Hz |
| `params[ID].getValue()` | Knob/switch value |
| `inputs[ID].getVoltage()` | Voltage from a cable (or 0 if unpatched, for normal get) |
| `inputs[ID].isConnected()` | Whether a cable is plugged in |
| `outputs[ID].setVoltage(v)` | Send voltage out |
| `lights[ID].setBrightness(b)` | LED 0..1 |

State that must survive between samples (phase, envelope level, filter memory) goes in **member variables**, not locals inside `process()`.

### 7.2 Anatomy of `MyModule.cpp`

Rough structure:

```text
struct MyModule : Module {
    enums for ParamId / InputId / OutputId / LightId
    member state (phase, ...)
    constructor: config(...) + configParam/Input/Output/Light
    process(...): the algorithm
};

struct MyModuleWidget : ModuleWidget {
    constructor: setPanel, addParam/addInput/addOutput/addChild(light)
};

Model* modelMyModule = createModel<MyModule, MyModuleWidget>("MyModule");
```

- **`MyModule`** = DSP + state.
- **`MyModuleWidget`** = on-screen panel + knob/jack widgets (UI only).
- **`modelMyModule`** = factory Rack uses to create instances; string `"MyModule"` must match the module slug in `plugin.json`.

### 7.3 One module vs many knobs vs many modules

| Goal | What to create |
|---|---|
| Extra knobs/jacks on the same device | More circles on the **same** SVG → regenerate or hand-edit **same** `.cpp` |
| Totally different device in the browser | New module slug → new SVG + new `.cpp` + register it |
| Sine and square from one box | One module, two outputs (or a mode switch) |
| Sine module and square module as separate products | Two modules / two `.cpp` files |

**Multiple knobs ≠ multiple `MyModule.cpp` files.**

### 7.4 Step-by-step: implement an algorithm

**Step A — Decide I/O**

Example VCO:

- Params: `PITCH`, maybe `SHAPE`
- Inputs: `PITCH` (1V/oct), maybe `FM`, `SYNC`
- Outputs: `SINE`, `SQUARE`
- Lights: optional activity LED

**Step B — Put markers on the panel** and generate/update enums.

**Step C — Config ranges in the constructor**

```cpp
configParam(PITCH_PARAM, -4.f, 4.f, 0.f, "Pitch", " V");
configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Level", "%", 0.f, 100.f);
configInput(PITCH_INPUT, "1V/oct");
configOutput(SINE_OUTPUT, "Sine");
```

**Step D — Add state members**

```cpp
float phase = 0.f;
```

**Step E — Read controls → compute → write outputs** every sample.

**Step F — `make install`, restart Rack, patch and listen.**

### 7.5 Worked example: sine oscillator (what you have)

Idea:

1. Read pitch knob (in volts/octaves offset).
2. Add pitch CV input (1V/oct).
3. Convert to frequency: \( f = 261.6256 \cdot 2^{pitch} \) (`dsp::FREQ_C4`).
4. Advance a 0..1 phase accumulator.
5. `sin(2π phase)` → scale to ±5 V audio.
6. Blink an LED at 1 Hz as a heartbeat.

```cpp
float pitch = params[PITCH_PARAM].getValue();
pitch += inputs[PITCH_INPUT].getVoltage();
float freq = dsp::FREQ_C4 * std::pow(2.f, pitch);

phase += freq * args.sampleTime;
if (phase >= 1.f)
    phase -= 1.f;

float sine = std::sin(2.f * M_PI * phase);
outputs[SINE_OUTPUT].setVoltage(5.f * sine);
```

### 7.6 Adding a second waveform on the **same** module

You do **not** need another `.cpp`. Add another blue circle labeled e.g. `SQUARE`, regenerate or hand-add `SQUARE_OUTPUT`, then in `process()`:

```cpp
float square = phase < 0.5f ? 5.f : -5.f;
outputs[SQUARE_OUTPUT].setVoltage(square);
```

### 7.7 Adding a second **module** (different browser entry)

```bash
$RACK_DIR/helper.py createmodule MyOtherModule res/MyOtherModule.svg src/MyOtherModule.cpp
```

Then register `modelMyOtherModule` in `plugin.hpp` / `plugin.cpp`. That is when you get another `.cpp`.

---

## 8. Controllers & signals: 1V/oct, gates, triggers, audio, CV

Rack patch cables carry **volts**. Your code reads/writes floats that mean voltages.

### 8.1 Audio

- Typical oscillator/audio out: **±5 V** (10 V peak-to-peak).
- Don’t obsessively hard-clip unless modeling hardware; Audio modules will handle the DAC end.

### 8.2 Pitch — **1 V / octave**

Standard for VCOs, filters’ cutoff CV, etc.:

\[
f = f_0 \cdot 2^{V}
\]

- For audio oscillators, default \( f_0 \) = **C4** = `dsp::FREQ_C4` (261.6256 Hz) when total pitch voltage is 0.
- +1 V → one octave up (freq × 2).
- −1 V → one octave down (freq × 0.5).

Pattern used everywhere:

```cpp
float volts = params[PITCH_PARAM].getValue();
volts += inputs[PITCH_INPUT].getVoltage();   // 1V/oct CV
float freq = dsp::FREQ_C4 * std::pow(2.f, volts);
```

MIDI-to-CV modules output this pitch CV so your oscillator tracks keyboard notes.

### 8.3 Unipolar vs bipolar CV

| Kind | Typical range | Examples |
|---|---|---|
| Unipolar CV | 0 V … 10 V | envelopes, gates held high |
| Bipolar CV | −5 V … +5 V | LFOs, bipolar modulation |

Always document what your jack expects in `configInput` / manual text.

### 8.4 Gates

- **Gate high** ≈ **10 V** while held (note on / sustain).
- **Gate low** ≈ **0 V**.
- Used for envelope generators: rise on note-on, release on note-off.

Simple read:

```cpp
bool gateOn = inputs[GATE_INPUT].getVoltage() >= 1.f;
```

Better: use a Schmitt trigger so noisy edges don’t chatter (see triggers).

### 8.5 Triggers

- Short pulse, often **10 V for ~1 ms**.
- Used for: drum hits, sequencer advance, ping filters, reset.

Use Rack helpers:

```cpp
dsp::SchmittTrigger trig;

void process(const ProcessArgs& args) override {
    if (trig.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f)) {
        // rising edge just happened — start a hit / advance a step
    }
}
```

Why Schmitt? Digital bandlimited edges can bounce around a single threshold; low/high thresholds fix false retriggers.

To *output* a trigger pulse:

```cpp
dsp::PulseGenerator pulse;
// on event:
pulse.trigger(1e-3f); // 1 ms
// every sample:
outputs[TRIG_OUTPUT].setVoltage(pulse.process(args.sampleTime) ? 10.f : 0.f);
```

### 8.6 How this connects in a patch (practical)

Typical voice:

```text
MIDI → CV   pitch out ──► VCO 1V/oct in
            gate out  ──► Envelope gate in
VCO audio out ──► VCA audio in
Envelope out  ──► VCA CV in
VCA out ──► Audio module (speakers/interface)
```

Your trial module is just the **VCO** piece: pitch knob + optional pitch CV in + audio out.

### 8.7 Checking cable presence

```cpp
if (inputs[FM_INPUT].isConnected()) {
    volts += inputs[FM_INPUT].getVoltage() * params[FM_AMT_PARAM].getValue();
}
```

### 8.8 Polyphony (later)

Cables can carry multiple channels (voices). `getVoltage()` is channel 0; polyphonic modules use `getPolyVoltage(c)` / channel counts. Add the `Polyphonic` tag if you support it. Monophonic modules should still behave sanely if fed poly CV (often use first channel or sum for audio).

---

## 9. Build, install, find it in Rack

```bash
cd /c/Users/Wanyin/Desktop/PROJECTS/MyPlugin
export RACK_DIR=/c/Users/Wanyin/Desktop/PROJECTS/Rack-SDK
make install
```

1. Fully quit VCV Rack and reopen.
2. Right-click empty rack (or press **Enter**) → Module Browser.
3. Search **`tpuwu`**, **`test_module`**, or **`MyModule`**.
4. Place module, patch output into **Audio**, turn pitch knob.

If missing, open `log.txt` and search for `MyPlugin`.

---

## 10. Everyday edit loop

1. Edit DSP in `src/*.cpp` and/or panel SVG in `res/`.
2. If you change component markers a lot, re-run `createmodule` (careful: can overwrite `.cpp` — say yes only if you intend to) **or** hand-edit enums/widget lines.
3. `make install`
4. Restart Rack and test.

Useful makes:

| Command | Does |
|---|---|
| `make` | Compile only |
| `make install` | Compile + package + copy into Rack user plugins |
| `make dist` | Package `.vcvplugin` into `dist/` |
| `make clean` | Wipe build artifacts |

---

## 11. File map of this repo

```text
MyPlugin/
  plugin.json          # plugin + module manifest (version 2.x.x!)
  Makefile             # includes Rack SDK plugin.mk
  res/
    MyModule.svg       # panel + components layer
  src/
    plugin.hpp         # declares pluginInstance + each model
    plugin.cpp         # init(): addModel(...)
    MyModule.cpp       # one module: DSP + widget
  dist/                # created by make dist/install
  plugin.dll           # local build output
```

---

## 12. Gotchas checklist

| Symptom | Likely cause |
|---|---|
| `python` / `make` missing in PowerShell | Use **MinGW 64-bit** shell |
| `Panel not found` | SVG path wrong / file not created yet |
| Module compiles but not in browser | Forgot `extern Model*` + `p->addModel(...)` |
| `addModel` compile error at top of file | Must be **inside** `init()` |
| Plugin folder exists but invisible in Rack | `"version"` not `2.x.x` |
| Module appears but silent | Empty `process()`, or not patched to Audio |
| Wrong jack positions | Components layer coordinates; regenerate or edit `mm2px` |
| Same-color controls confused | Give each shape a unique **label** |

---

## 13. Suggested next experiments

1. Add a `SQUARE` output on the same module.
2. Add an `FM` input and FM amount knob.
3. Add a `GATE` input + simple AD envelope multiplying the sine (mini VCA inside the module).
4. Create a second module file (`MyVCA.cpp`) to learn multi-module plugins.
5. When ready for a real product name: `createplugin YourRealSlug` with `"version": "2.0.0"` from the start.

---

## Quick command cheat sheet

```bash
# MinGW 64-bit shell
export RACK_DIR=/c/Users/Wanyin/Desktop/PROJECTS/Rack-SDK
cd /c/Users/Wanyin/Desktop/PROJECTS/MyPlugin

$RACK_DIR/helper.py createplugin MyPlugin
$RACK_DIR/helper.py createmodule MyModule res/MyModule.svg src/MyModule.cpp

make
make install
```

That’s the full pipeline: **SDK path → create plugin repo → panel → createmodule (MinGW) → register → write DSP / understand volts → make install → test in Rack.**
