# Pomodoro Timer – Digital Twin

Two twins of the same firmware (the code from your design document, unchanged):

| Layer | What runs | Fidelity | Needs |
|-------|-----------|----------|-------|
| **A. Host twin** (`host/`) | `app.c` + `sched.c` compiled natively, fake HAL, virtual clock | Logic, timing, state machine, debounce, watchdog | gcc + make (Linux / macOS / WSL) |
| **B. Wokwi twin** (`wokwi/`) | The real Arduino firmware on a virtual ATmega328P Nano, I2C LCD, 7 buttons, buzzer | Pin map, ISRs, I2C, compile errors | A browser |

## A. Host twin

    cd host
    make test            # 11 automated scenarios, ~2 s
    make run             # interactive terminal twin, real time
    ./twin --speed 60    # interactive, simulated time 60x faster (30 min = 30 s)

Keys: q/a = H+/H-, w/s = M+/M-, e/d = S+/S-, space = START, x = quit.

Why it is trustworthy: `firmware/` is byte-for-byte the code you would flash.
Only `hal_pc.c` replaces `port_arduino.ino`. The clock is virtual, so a 30-minute
run is measured exactly and tests are repeatable.

## B. Wokwi twin

1. Open https://wokwi.com -> New Project -> Arduino Nano.
2. Replace the project files with everything in `wokwi/`
   (sketch.ino, app.cpp, sched.cpp, app.h, sched.h, hal.h, libraries.txt, diagram.json).
   Tip: paste diagram.json into the `diagram.json` tab; add the other files with the "+" tab button.
3. Press the green Start button. Click the virtual buttons.
4. If a wire shows as unconnected, drag it to the pin in the table below.

| Signal | Nano pin | Signal | Nano pin |
|--------|----------|--------|----------|
| H+ / H- | D8 / D9 | START | D2 |
| M+ / M- | D10 / D11 | Buzzer + | D3 |
| S+ / S- | D12 / D13 | LCD SDA / SCL | A4 / A5 |

Each button: one leg to the pin, other leg to GND (internal pull-up, no resistors).

Note: Wokwi's buzzer makes sound only for toggling tones; a steady HIGH may be
silent. Watch for the buzzer part animating, or add an LED + 220 ohm on D3.

## Promoting the twin to real hardware
Same sketch folder -> Arduino IDE (rename .cpp files if needed), install
"LiquidCrystal I2C" (Frank de Brabander), board Arduino Nano, flash.

## Findings from the twin (see host/twin_main.c)
* Pass: 30-min default, 90 s timer, debounce, wrap-around, LCD rates (200 ms / 1 s),
  edit lock while running, alarm 10 s timeout, silence, millis() rollover, watchdog.
* Timing: first beep lands 500-520 ms after zero (buzzer task period + button-task latency).
* **Quirk:** START while RUNNING restarts the countdown, but `sched_enable()` does not
  re-phase the already-enabled counter task, so the first second can be up to 1 s short.
  Fix (1 line) in `start_countdown()`: call `sched_enable(T_COUNTER, false);` before
  `sched_enable(T_COUNTER, true);`.
* Presses in the first 50 ms after boot are ignored (debounce timestamp starts at 0). Harmless.
