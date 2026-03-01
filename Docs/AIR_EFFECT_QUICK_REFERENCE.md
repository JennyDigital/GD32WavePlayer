# Air Effect Filter - Quick Reference

## What It Does
Adds brightness and presence to audio samples by boosting high-frequency content.

## Key Parameters
| Parameter           | Value                                 | Description                       |
|---------------------|---------------------------------------|-----------------------------------|
| **Type**            | High-shelf one-pole filter            | Simple, CPU-efficient             |
| **Shelf Gain**      | 0.667 Q16                             | Moderate brightening (~6 dB boost) |
| **Cutoff Alpha**    | 0.75 (~5-6 kHz @ 22 kHz)              |                                   |
| **Default State**   | DISABLED                              | User selects when to enable       |
| **Bit Depth**       | 8-bit & 16-bit                        | Works with both playback paths    |

## Enable It
```c
// Auto-enables air effect when preset > 0
SetAirEffectPresetDb( 1 );  // +1 dB
SetAirEffectPresetDb( 2 );  // +2 dB (default)
SetAirEffectPresetDb( 3 );  // +3 dB
```

## Disable It
```c
// Auto-disables air effect when preset = 0
SetAirEffectPresetDb( 0 );
```

## When to Use
✓ Muffled samples → adds clarity  
✓ Dark recordings → adds presence  
✓ Quiet samples → adds energy  
✗ Already bright samples → may sound harsh  

## Filter Chain Order
```c
Input Sample
  ↓
16/8-bit Low-Pass Filter (if enabled)
  ↓
DC Blocking Filter (if enabled)
  ↓
AIR EFFECT ← YOU ARE HERE (if enabled)
  ↓
Fade In/Out
  ↓
Noise Gate (if enabled)
  ↓
Soft Clipping (if enabled)
  ↓
Output Sample
```

## Settings to Tune
If the effect is too subtle:
- Increase `AIR_EFFECT_SHELF_GAIN` (currently 43691)
  - Try 52429 for stronger boost
  - Max safe: 57344

If the effect is too harsh:
- Decrease `AIR_EFFECT_SHELF_GAIN`
  - Try 36700 for gentler effect

If you want higher frequency boost (48 kHz mode):
- Increase `AIR_EFFECT_CUTOFF` toward 65536
  - Current: 49152 (0.75 alpha)
  - Try: 55000 for higher shelving

## Files Modified
- `audio_engine.h`: Defines + struct
- `audio_engine.c`: Function + integration
- `main.c`: Config initialization

## Status
✅ **Implementation Complete**  
✅ **Fully Integrated**  
✅ **Zero Compilation Errors**  
✅ **Ready for Testing**
