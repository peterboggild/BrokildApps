# The EL34, fitted to its data sheet

`el34_data.py` is the Mullard EL34 data sheet (January 1960 / 1964),
digitised from pages C1–C3 rasterised at 300 dpi. Each axis was calibrated on
its own printed ticks, and the strokes were read column by column.

- **C1**: anode and screen current against Vg1, at Va = Vg2 = 250, 325 and
  400 V.
- **C2**: the anode family at Vg2 = 250 V, for Vg1 = 0 … −20 V.
- **C3**: the triode-connected curves, Va = 250 … 500 V.
- **Page D1**: the tabulated points.

Two checks against the sheet's own table:

- C1 reads 102.5 mA at Va = Vg2 = 250 V, Vg1 = −12 V, against the tabulated
  100 mA at −12.2 V.
- C3 reads 70 mA at Va = 250 V, −15.6 V, against the tabulated triode point,
  70 mA at −15.5 V.

`fit.py` fits the pentode of `Source/Circuit.cpp` to all of it and scores
Koren's published fit the same way. Over every point at 10 mA and up:

| Model | RMS error |
|---|---|
| Koren's published EL34 | 35 % (screen current about 60 % low) |
| this fit | 10.4 % |

What is left is mostly the sheet disagreeing with itself. C1 at
Va = Vg2 = 400 V and C3's triode curve at 400 V are drawn about 20 % apart
at the same electrode voltages, and the fit splits the difference. The
tabulated points land within 1–6 %.

Not on the sheet, so still estimated: the grid current's size at a positive
grid. Its onset is on the sheet (+0.3 µA at −1.3 V), and that sets Cg.
