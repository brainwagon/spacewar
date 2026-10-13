# Spacewar and PDP-1 demos

Tom Duff started to do this; I thought I would as well.

This project emulates the PDP-1 display and runs Spacewar! alongside
two other PDP-1 programs: Minskytron and Snowflake.

## Build

You need a C compiler, `make`, and [raylib](https://www.raylib.com/).
The Spacewar! program image, `spacewar.mem.h`, must also be present in
the project directory. It is generated data and is not tracked by git.
The Minskytron and Snowflake program images are included in
`demo_images.h`.

Build all three programs with `make`:

```sh
make
./spacewar
./minskytron
./snowflake
```

Run `make check` to check PDP-1 hardware multiplication and Snowflake's
centered display-coordinate coverage.

## Controls

All programs use `1` for real PDP-1 speed, `2` for half speed, `3` for
double speed, `4` for maximum speed, `TAB` to toggle help, and `ESC` to
quit.

### Spacewar!

- Ship 1: `A` / `D` rotate, `S` thrust, `W` fire.
- Ship 0: `J` / `L` rotate, `K` thrust, `I` fire.
- `P` toggles autopilot; `M` heavy dots; `V` heavy star;
  `F` FPS display; `H` HUD; `G` logo.

### Minskytron

Click the 18 Test Word switches to change the value, shown in octal.
DEC bit numbering is used: switch 0 is the leftmost and most
significant bit. Press `R` to restart; Minskytron reads the Test Word
when it starts.

### Snowflake

Click sense switches 1 and 2, or use `F1` and `F2`, to toggle them.
Switch 2 selects 8-dot mode; switches 1 and 2 together select
single-dot mode.

## Program images

The machine-code images in `demo_images.h` were decoded from the
Minskytron and Snowflake paper tapes. Snowflake uses the later PDP-1
hardware multiply option; Spacewar! uses the original step multiply
and divide instructions.

— Mark
