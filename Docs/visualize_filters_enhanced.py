#!/usr/bin/env python3
"""
Enhanced Filter Characteristics Visualization for Audio Engine
Generates comprehensive frequency response and transfer function plots for DSP filters.
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

BASE_DIR = Path(__file__).resolve().parent

# Filter coefficients from audio_engine.h
DC_FILTER_ALPHA = 64225 / 65536  # 0.98
SOFT_DC_FILTER_ALPHA = 65216 / 65536  # 0.995

# 16-bit Biquad LPF levels (higher alpha = stronger filtering)
LPF_16BIT_VERY_SOFT = 40960 / 65536  # 0.625 - lightest filtering
LPF_16BIT_SOFT = 52429 / 65536  # ~0.80 - gentle filtering
LPF_16BIT_MEDIUM = 57344 / 65536  # 0.875 - balanced filtering
LPF_16BIT_FIRM = 60416 / 65536  # ~0.92 - firm filtering
LPF_16BIT_AGGRESSIVE = 63488 / 65536  # ~0.97 - strongest filtering
LPF_16BIT_ALPHA = LPF_16BIT_SOFT  # Default alpha for testing

# 8-bit LPF levels
LPF_VERY_SOFT = 61440 / 65536  # 0.9375
LPF_SOFT = 57344 / 65536  # 0.875
LPF_MEDIUM = 49152 / 65536  # 0.75
LPF_FIRM = 45056 / 65536  # 0.6875
LPF_AGGRESSIVE = 40960 / 65536  # 0.625

# Air Effect (High-Shelf Brightening Filter)
AIR_EFFECT_CUTOFF = 49152 / 65536  # 0.75 - ~5-6 kHz shelving frequency
AIR_EFFECT_SHELF_GAIN = 98304 / 65536  # ~1.5 - high-frequency shelf (≈ +1.6 dB at Nyquist with alpha=0.75)
AIR_EFFECT_SHELF_GAIN_MAX = 131072 / 65536  # 2.0x cap (matches code)
AIR_EFFECT_PRESETS_DB = [1.0, 2.0, 3.0]


def db_to_shelf_gain(db, alpha=AIR_EFFECT_CUTOFF, gain_max=AIR_EFFECT_SHELF_GAIN_MAX):
    """Convert desired HF boost in dB to shelf_gain (linear) using the Air Effect transfer function.

    H(pi) = 10^(db/20)
    G = (Hpi*(2 - alpha) - alpha) / (2*(1 - alpha))
    """
    one_minus_alpha = 1.0 - alpha
    Hpi = 10 ** (db / 20.0)
    G = (Hpi * (2.0 - alpha) - alpha) / (2.0 * one_minus_alpha)
    if G < 0.0:
        G = 0.0
    if G > gain_max:
        G = gain_max
    return G

# Sampling frequency
FS = 22000  # Hz (default playback speed)

def dc_blocking_filter_response(alpha, fs=FS):
    """
    Calculate frequency response of DC blocking filter (high-pass).
    H(z) = (1 - z^-1) / (1 - alpha*z^-1)
    """
    frequencies = np.logspace(0, np.log10(fs/2), 1000)
    omega = 2 * np.pi * frequencies / fs
    
    # Calculate magnitude response
    z = np.exp(1j * omega)
    numerator = 1 - z**(-1)
    denominator = 1 - alpha * z**(-1)
    H = numerator / denominator
    mag = np.maximum(np.abs(H), 1e-12)
    magnitude_db = 20 * np.log10(mag)
    
    return frequencies, magnitude_db

def lpf_8bit_response(alpha, fs=FS):
    """
    Calculate frequency response of simple 1-pole LPF for 8-bit samples.
    H(z) = alpha / (1 - (1-alpha)*z^-1)
    """
    frequencies = np.logspace(0, np.log10(fs/2), 1000)
    omega = 2 * np.pi * frequencies / fs
    
    z = np.exp(1j * omega)
    numerator = alpha
    denominator = 1 - (1 - alpha) * z**(-1)
    H = numerator / denominator
    mag = np.maximum(np.abs(H), 1e-12)
    magnitude_db = 20 * np.log10(mag)
    
    return frequencies, magnitude_db

def lpf_16bit_biquad_response(alpha, fs=FS):
    """
    Calculate frequency response of biquad LPF for 16-bit samples.
    Implementation from ApplyLowPassFilter16Bit():
    b0 = ((1-alpha)^2) / 2
    b1 = 2*b0
    b2 = b0
    a1 = -2*alpha
    a2 = alpha^2
    """
    frequencies = np.logspace(0, np.log10(fs/2), 1000)
    omega = 2 * np.pi * frequencies / fs
    
    # Calculate coefficients
    b0 = ((1 - alpha)**2) / 2
    b1 = 2 * b0
    b2 = b0
    a0 = 1
    a1 = -2 * alpha
    a2 = alpha**2
    
    z = np.exp(1j * omega)
    numerator = b0 + b1 * z**(-1) + b2 * z**(-2)
    denominator = a0 + a1 * z**(-1) + a2 * z**(-2)
    H = numerator / denominator
    mag = np.maximum(np.abs(H), 1e-12)
    magnitude_db = 20 * np.log10(mag)
    phase_rad = np.angle(H)
    
    return frequencies, magnitude_db, phase_rad

def find_cutoff_frequency(frequencies, magnitude_db):
    """Find the -3dB cutoff frequency."""
    # Find where magnitude crosses -3dB
    idx = np.argmin(np.abs(magnitude_db - (-3.0)))
    return frequencies[idx]

def air_effect_response(alpha=AIR_EFFECT_CUTOFF, shelf_gain=AIR_EFFECT_SHELF_GAIN, fs=FS):
    """
    Calculate frequency response of Air Effect (high-shelf brightening filter).
    Based on C implementation: 
    output = α*x + (1-α)*y_prev + (1-α)*shelf_gain*(x - x_prev)
    
    Transfer function:
    H(z) = [α + (1-α)*shelf_gain - (1-α)*shelf_gain*z^-1] / [1 - (1-α)*z^-1]
    """
    frequencies = np.logspace(0, np.log10(fs/2), 1000)
    omega = 2 * np.pi * frequencies / fs
    
    z = np.exp(1j * omega)
    one_minus_alpha = 1 - alpha
    
    # Numerator: b0 + b1*z^-1
    # b0 = α + (1-α)*shelf_gain
    # b1 = -(1-α)*shelf_gain
    b0 = alpha + one_minus_alpha * shelf_gain
    b1 = -one_minus_alpha * shelf_gain
    
    # Denominator: 1 - (1-α)*z^-1
    a0 = 1
    a1 = -one_minus_alpha
    
    numerator = b0 + b1 * z**(-1)
    denominator = a0 + a1 * z**(-1)
    
    H = numerator / denominator
    mag = np.maximum(np.abs(H), 1e-12)
    magnitude_db = 20 * np.log10(mag)
    
    return frequencies, magnitude_db

def soft_clipping_transfer():
    """
    Generate the soft clipping transfer function.
    Uses cubic smoothstep curve above/below threshold.
    """
    threshold = 28000
    max_val = 32767
    
    input_range = np.linspace(-32768, 32767, 2000)
    output = np.zeros_like(input_range)
    
    for i, s in enumerate(input_range):
        if s > threshold:
            excess = s - threshold
            range_val = max_val - threshold
            x = excess / range_val
            if x > 1.0:
                x = 1.0
            curve = 1.5 * x**2 - x**3
            output[i] = threshold + range_val * curve
        elif s < -threshold:
            excess = -threshold - s
            range_val = max_val - threshold
            x = excess / range_val
            if x > 1.0:
                x = 1.0
            curve = 1.5 * x**2 - x**3
            output[i] = -threshold - range_val * curve
        else:
            output[i] = s
    
    return input_range, output

# Create the figure with more subplots
fig = plt.figure(figsize=(16, 16))
gs = GridSpec(4, 2, figure=fig, hspace=0.5, wspace=0.3)

# Plot 1: DC Blocking Filters
ax1 = fig.add_subplot(gs[0, 0])
freq_dc, mag_dc = dc_blocking_filter_response(DC_FILTER_ALPHA)
freq_soft_dc, mag_soft_dc = dc_blocking_filter_response(SOFT_DC_FILTER_ALPHA)

ax1.semilogx(freq_dc, mag_dc, 'b-', linewidth=2, label=f'DC Block (α={DC_FILTER_ALPHA:.4f})')
ax1.semilogx(freq_soft_dc, mag_soft_dc, 'r--', linewidth=2, label=f'Soft DC Block (α={SOFT_DC_FILTER_ALPHA:.4f})')
ax1.grid(True, alpha=0.3, which='both')
ax1.set_xlabel('Frequency (Hz)')
ax1.set_ylabel('Magnitude (dB)')
ax1.set_title('DC Blocking Filter (High-Pass)')
ax1.legend()
ax1.set_ylim([-40, 5])

# Plot 2: 16-bit Biquad LPF with all levels
ax2 = fig.add_subplot(gs[0, 1])
freq_16vs, mag_16vs, _ = lpf_16bit_biquad_response(LPF_16BIT_VERY_SOFT)
freq_16s, mag_16s, _ = lpf_16bit_biquad_response(LPF_16BIT_SOFT)
freq_16m, mag_16m, _ = lpf_16bit_biquad_response(LPF_16BIT_MEDIUM)
freq_16f, mag_16f, _ = lpf_16bit_biquad_response(LPF_16BIT_FIRM)
freq_16a, mag_16a, _ = lpf_16bit_biquad_response(LPF_16BIT_AGGRESSIVE)

ax2.semilogx(freq_16vs, mag_16vs, 'c-', linewidth=2, label=f'Very Soft (α={LPF_16BIT_VERY_SOFT:.4f})')
ax2.semilogx(freq_16s, mag_16s, 'g-', linewidth=2, label=f'Soft (α={LPF_16BIT_SOFT:.4f})')
ax2.semilogx(freq_16m, mag_16m, 'orange', linewidth=2, label=f'Medium (α={LPF_16BIT_MEDIUM:.2f})')
ax2.semilogx(freq_16f, mag_16f, 'y-', linewidth=2, label=f'Firm (α={LPF_16BIT_FIRM:.4f})')
ax2.semilogx(freq_16a, mag_16a, 'r-', linewidth=2, label=f'Aggressive (α={LPF_16BIT_AGGRESSIVE:.3f})')
ax2.grid(True, alpha=0.3, which='both')
ax2.set_xlabel('Frequency (Hz)')
ax2.set_ylabel('Magnitude (dB)')
ax2.set_title('16-bit Biquad Low-Pass Filter (Various Levels)')
ax2.legend(fontsize=8)
ax2.set_ylim([-60, 5])
ax2.axhline(-3, color='k', linestyle='--', alpha=0.3, linewidth=1)

# Plot 3: 8-bit LPF with different aggressiveness levels
ax3 = fig.add_subplot(gs[1, 0])
freq_vs, mag_vs = lpf_8bit_response(LPF_VERY_SOFT)
freq_s, mag_s = lpf_8bit_response(LPF_SOFT)
freq_m, mag_m = lpf_8bit_response(LPF_MEDIUM)
freq_f, mag_f = lpf_8bit_response(LPF_FIRM)
freq_a, mag_a = lpf_8bit_response(LPF_AGGRESSIVE)

ax3.semilogx(freq_vs, mag_vs, 'c-', linewidth=2, label=f'Very Soft (α={LPF_VERY_SOFT:.4f})')
ax3.semilogx(freq_s, mag_s, 'b-', linewidth=2, label=f'Soft (α={LPF_SOFT:.4f})')
ax3.semilogx(freq_m, mag_m, 'orange', linewidth=2, label=f'Medium (α={LPF_MEDIUM:.2f})')
ax3.semilogx(freq_f, mag_f, 'y-', linewidth=2, label=f'Firm (α={LPF_FIRM:.4f})')
ax3.semilogx(freq_a, mag_a, 'r-', linewidth=2, label=f'Aggressive (α={LPF_AGGRESSIVE:.3f})')
ax3.grid(True, alpha=0.3, which='both')
ax3.set_xlabel('Frequency (Hz)')
ax3.set_ylabel('Magnitude (dB)')
ax3.set_title('8-bit Low-Pass Filter (Various Levels)')
ax3.legend(fontsize=8)
ax3.set_ylim([-40, 5])
ax3.axhline(-3, color='k', linestyle='--', alpha=0.3, linewidth=1)

# Plot 4: Soft Clipping Transfer Function
ax4 = fig.add_subplot(gs[1, 1])
input_clip, output_clip = soft_clipping_transfer()

ax4.plot(input_clip, output_clip, 'purple', linewidth=2, label='Soft Clipping')
ax4.plot([-32768, 32767], [-32768, 32767], 'k--', alpha=0.3, label='Linear (no clipping)')
ax4.axvline(28000, color='r', linestyle=':', alpha=0.5, label='Threshold')
ax4.axvline(-28000, color='r', linestyle=':', alpha=0.5)
ax4.grid(True, alpha=0.3)
ax4.set_xlabel('Input Sample Value')
ax4.set_ylabel('Output Sample Value')
ax4.set_title('Soft Clipping Transfer Function')
ax4.legend()
ax4.set_xlim([-33000, 33000])
ax4.set_ylim([-33000, 33000])

# Plot 5: Combined Frequency Response (All Filters) - spans full width
ax5 = fig.add_subplot(gs[2, :])
freq_dc, mag_dc = dc_blocking_filter_response(SOFT_DC_FILTER_ALPHA)
freq_16bit, mag_16bit, _ = lpf_16bit_biquad_response(LPF_16BIT_SOFT)

# Calculate combined response (DC block + 16-bit LPF)
combined_mag = mag_dc + mag_16bit

ax5.semilogx(freq_dc, mag_dc, 'b-', linewidth=1.5, alpha=0.7, label='Soft DC Block')
ax5.semilogx(freq_16bit, mag_16bit, 'g-', linewidth=1.5, alpha=0.7, label='16-bit Biquad LPF (Soft)')
ax5.semilogx(freq_dc, combined_mag, 'r-', linewidth=2.5, label='Combined Response (DC Block + LPF)')
ax5.grid(True, alpha=0.3, which='both')
ax5.set_xlabel('Frequency (Hz)')
ax5.set_ylabel('Magnitude (dB)')
ax5.set_title('Combined Filter Chain Response (16-bit path)')
ax5.legend()
ax5.set_ylim([-80, 5])
ax5.axhline(0, color='k', linestyle='-', alpha=0.2)
ax5.axhline(-3, color='k', linestyle='--', alpha=0.2)

# Plot 6: Phase Response for 16-bit Biquad LPF
ax6 = fig.add_subplot(gs[3, 0])
freq_16vs, _, phase_16vs = lpf_16bit_biquad_response(LPF_16BIT_VERY_SOFT)
freq_16s, _, phase_16s = lpf_16bit_biquad_response(LPF_16BIT_SOFT)
freq_16m, _, phase_16m = lpf_16bit_biquad_response(LPF_16BIT_MEDIUM)
freq_16f, _, phase_16f = lpf_16bit_biquad_response(LPF_16BIT_FIRM)
freq_16a, _, phase_16a = lpf_16bit_biquad_response(LPF_16BIT_AGGRESSIVE)

ax6.semilogx(freq_16vs, np.degrees(phase_16vs), 'c-', linewidth=2, label='Very Soft')
ax6.semilogx(freq_16s, np.degrees(phase_16s), 'g-', linewidth=2, label='Soft')
ax6.semilogx(freq_16m, np.degrees(phase_16m), 'orange', linewidth=2, label='Medium')
ax6.semilogx(freq_16f, np.degrees(phase_16f), 'y-', linewidth=2, label='Firm')
ax6.semilogx(freq_16a, np.degrees(phase_16a), 'r-', linewidth=2, label='Aggressive')
ax6.grid(True, alpha=0.3, which='both')
ax6.set_xlabel('Frequency (Hz)')
ax6.set_ylabel('Phase (degrees)')
ax6.set_title('16-bit Biquad LPF Phase Response')
ax6.legend(fontsize=8)

# Plot 7: Air Effect High-Shelf Brightening Filter (show presets)
ax7 = fig.add_subplot(gs[3, 1])
colors = ['darkgreen', 'orange', 'purple']
linestyles = ['-', '--', ':']
for idx, db in enumerate(AIR_EFFECT_PRESETS_DB):
    shelf_gain = db_to_shelf_gain(db, AIR_EFFECT_CUTOFF, AIR_EFFECT_SHELF_GAIN_MAX)
    freq_air, mag_air = air_effect_response(AIR_EFFECT_CUTOFF, shelf_gain)
    label = f"{db:+.0f} dB preset (G={shelf_gain:.2f}x)"
    ax7.semilogx(freq_air, mag_air, color=colors[idx % len(colors)], linestyle=linestyles[idx % len(linestyles)], linewidth=2.2, label=label)

ax7.axhline(0, color='k', linestyle='-', alpha=0.2)
ax7.axhline(3, color='k', linestyle='--', alpha=0.3, linewidth=1, label='+3 dB guide')
ax7.grid(True, alpha=0.3, which='both')
ax7.set_xlabel('Frequency (Hz)')
ax7.set_ylabel('Magnitude (dB)')
ax7.set_title('Air Effect Presets (+1, +2, +3 dB)\nα=0.75, Shelf Gain max=2.0x')
ax7.legend(fontsize=8, loc='upper left')
ax7.set_ylim([-10, 10])

# Add overall title for page 1
fig.suptitle('Figure 1: Comprehensive Filter Frequency Response Analysis\nAudio Engine DSP Filter Characteristics - Page 1: Filter Responses\nGD32 Audio Engine @ 22 kHz Sample Rate', 
             fontsize=14, fontweight='bold', y=0.998)

# Save the first figure
output_file = BASE_DIR / 'filter_characteristics_enhanced.png'
plt.savefig(output_file, dpi=300, bbox_inches='tight')
print(f"Enhanced filter characteristics plot saved to: {output_file}")

# ========== PAGE 2: SUMMARY TABLE ==========
# Create a second figure for the summary/characteristics table
fig2 = plt.figure(figsize=(16, 14))
ax_summary = fig2.add_subplot(111)
ax_summary.axis('off')

# Create the summary text
# Calculate cutoff frequencies (reuse from above)
cutoff_8vs = find_cutoff_frequency(*lpf_8bit_response(LPF_VERY_SOFT))
cutoff_8s = find_cutoff_frequency(*lpf_8bit_response(LPF_SOFT))
cutoff_8m = find_cutoff_frequency(*lpf_8bit_response(LPF_MEDIUM))
cutoff_8f = find_cutoff_frequency(*lpf_8bit_response(LPF_FIRM))
cutoff_8a = find_cutoff_frequency(*lpf_8bit_response(LPF_AGGRESSIVE))

freq_temp, mag_temp, _ = lpf_16bit_biquad_response(LPF_16BIT_VERY_SOFT)
cutoff_16vs = find_cutoff_frequency(freq_temp, mag_temp)
freq_temp, mag_temp, _ = lpf_16bit_biquad_response(LPF_16BIT_SOFT)
cutoff_16s = find_cutoff_frequency(freq_temp, mag_temp)
freq_temp, mag_temp, _ = lpf_16bit_biquad_response(LPF_16BIT_MEDIUM)
cutoff_16m = find_cutoff_frequency(freq_temp, mag_temp)
freq_temp, mag_temp, _ = lpf_16bit_biquad_response(LPF_16BIT_FIRM)
cutoff_16f = find_cutoff_frequency(freq_temp, mag_temp)
freq_temp, mag_temp, _ = lpf_16bit_biquad_response(LPF_16BIT_AGGRESSIVE)
cutoff_16a = find_cutoff_frequency(freq_temp, mag_temp)

info_text = f"""Audio Engine Filter Characteristics - Complete Summary
Sample Rate: {FS} Hz (Nyquist: {FS/2} Hz)

═══════════════════════════════════════════════════════════════════════════════

8-bit Low-Pass Filter (One-Pole Architecture)
──────────────────────────────────────────────
Level          Alpha    -3dB Cutoff Frequency      % of Sample Rate
─────────────────────────────────────────────────────────────────────
Very Soft      0.9375   {cutoff_8vs:6.0f} Hz                   {cutoff_8vs/FS*100:5.2f}%
Soft           0.8750   {cutoff_8s:6.0f} Hz                   {cutoff_8s/FS*100:5.2f}%
Medium         0.7500   {cutoff_8m:6.0f} Hz                   {cutoff_8m/FS*100:5.2f}%
Firm           0.6875   {cutoff_8f:6.0f} Hz                   {cutoff_8f/FS*100:5.2f}%
Aggressive     0.6250   {cutoff_8a:6.0f} Hz                   {cutoff_8a/FS*100:5.2f}%

═══════════════════════════════════════════════════════════════════════════════

16-bit Biquad Low-Pass Filter (2nd-Order Architecture)
──────────────────────────────────────────────────────
Level          Alpha    -3dB Cutoff Frequency      % of Sample Rate
─────────────────────────────────────────────────────────────────────
Very Soft      0.6250   {cutoff_16vs:6.0f} Hz                   {cutoff_16vs/FS*100:5.2f}%
Soft           0.8000   {cutoff_16s:6.0f} Hz                   {cutoff_16s/FS*100:5.2f}%
Medium         0.8750   {cutoff_16m:6.0f} Hz                   {cutoff_16m/FS*100:5.2f}%
Firm           0.9200   {cutoff_16f:6.0f} Hz                   {cutoff_16f/FS*100:5.2f}%
Aggressive     0.9700   {cutoff_16a:6.0f} Hz                   {cutoff_16a/FS*100:5.2f}%

═══════════════════════════════════════════════════════════════════════════════

DC Blocking Filters (High-Pass)
────────────────────────────────
Standard DC Filter      α = 0.98     ~44 Hz cutoff frequency
Soft DC Filter          α = 0.995    ~22 Hz cutoff frequency

═══════════════════════════════════════════════════════════════════════════════

Air Effect Filter (High-Shelf Brightening)
───────────────────────────────────────────
Type:                One-pole high-shelf filter
Cutoff Alpha:        {AIR_EFFECT_CUTOFF:.4f} (≈ 5–6 kHz shelving frequency @ {FS} Hz)
Shelf Gain:          {AIR_EFFECT_SHELF_GAIN:.4f} (≈ +1.6 dB HF boost @ α=0.75)
Presets Shown:       +1 dB, +2 dB, +3 dB (clamped to 2.0x max)
Purpose:             Adds presence and brightness to audio
Runtime Control:     enable_air_effect; SetAirEffectPresetDb(), SetAirEffectGainDb()

═══════════════════════════════════════════════════════════════════════════════

Soft Clipping Curve
───────────────────
Threshold:           ±28,000 (±85% of full scale ±32,767)
Curve Function:      Cubic smoothstep (s(x) = 1.5x² - x³)
Purpose:             Prevents harsh digital distortion

═══════════════════════════════════════════════════════════════════════════════

Implementation Notes
────────────────────
✓ All filters use fixed-point integer math (Q16 format)
✓ No floating-point operations required (MCU efficient)
✓ Biquad filters provide steeper rolloff and better control
✓ DC blocking prevents output offset drift over time
✓ Air effect works with both 8-bit and 16-bit audio paths
✓ Soft clipping is optional but recommended for safety
✓ Filter state is reset at the start of each sample playback
✓ Warm-up cycles (16 passes) suppress biquad startup transients

═══════════════════════════════════════════════════════════════════════════════
Generated: 2026-01-25 | GD32 Audio Engine Documentation
"""

ax_summary.text(0.05, 0.98, info_text, transform=ax_summary.transAxes, 
                fontsize=8, verticalalignment='top', family='monospace',
                bbox=dict(boxstyle='round', facecolor='#f0f0f0', alpha=0.5, pad=1))

fig2.suptitle('Audio Engine Filter Characteristics - Page 2: Complete Summary\nAll Cutoff Frequencies and Filter Parameters', 
              fontsize=16, fontweight='bold', y=0.985)

# Save the second figure
output_file2 = BASE_DIR / 'filter_characteristics_summary_page2.png'
plt.savefig(output_file2, dpi=300, bbox_inches='tight')
print(f"Summary characteristics page saved to: {output_file2}")

# Display both figures
plt.show()
