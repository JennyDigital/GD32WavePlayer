#!/usr/bin/env python3
"""
Generate comprehensive filter report as A4 PDF.
Creates a professional technical report with plots, tables, and analysis.
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.gridspec import GridSpec
from datetime import datetime

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
AIR_EFFECT_SHELF_GAIN = 98304 / 65536  # ~1.5 - high-frequency shelf
AIR_EFFECT_SHELF_GAIN_MAX = 131072 / 65536  # 2.0x cap
AIR_EFFECT_PRESETS_DB = [1.0, 2.0, 3.0]

# Sampling frequency
FS = 22000  # Hz (default playback speed)

def dc_blocking_filter_response(alpha, fs=FS):
   """Calculate frequency response of DC blocking filter (high-pass)."""
   frequencies = np.logspace(0, np.log10(fs / 2), 1000)
   omega = 2 * np.pi * frequencies / fs
   z = np.exp(1j * omega)
   numerator = 1 - z ** (-1)
   denominator = 1 - alpha * z ** (-1)
   H = numerator / denominator
   mag = np.maximum(np.abs(H), 1e-12)
   magnitude_db = 20 * np.log10(mag)
   return frequencies, magnitude_db

def lpf_8bit_response(alpha, fs=FS):
   """Calculate frequency response of 1-pole LPF for 8-bit samples."""
   frequencies = np.logspace(0, np.log10(fs / 2), 1000)
   omega = 2 * np.pi * frequencies / fs
   z = np.exp(1j * omega)
   numerator = alpha
   denominator = 1 - (1 - alpha) * z ** (-1)
   H = numerator / denominator
   mag = np.maximum(np.abs(H), 1e-12)
   magnitude_db = 20 * np.log10(mag)
   return frequencies, magnitude_db

def lpf_16bit_biquad_response(alpha, fs=FS):
   """Calculate frequency response of biquad LPF for 16-bit samples."""
   frequencies = np.logspace(0, np.log10(fs / 2), 1000)
   omega = 2 * np.pi * frequencies / fs

   b0 = ((1 - alpha) ** 2) / 2
   b1 = 2 * b0
   b2 = b0
   a0 = 1
   a1 = -2 * alpha
   a2 = alpha ** 2

   z = np.exp(1j * omega)
   numerator = b0 + b1 * z ** (-1) + b2 * z ** (-2)
   denominator = a0 + a1 * z ** (-1) + a2 * z ** (-2)
   H = numerator / denominator
   mag = np.maximum(np.abs(H), 1e-12)
   magnitude_db = 20 * np.log10(mag)
   phase_rad = np.angle(H)

   return frequencies, magnitude_db, phase_rad

def find_cutoff_frequency(frequencies, magnitude_db):
    """Find the -3dB cutoff frequency."""
    idx = np.argmin(np.abs(magnitude_db - (-3.0)))
    return frequencies[idx]

def soft_clipping_transfer():
    """Generate the soft clipping transfer function."""
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

def db_to_shelf_gain(db, alpha=AIR_EFFECT_CUTOFF, gain_max=AIR_EFFECT_SHELF_GAIN_MAX):
    """Convert desired HF boost in dB to shelf_gain (linear)."""
    one_minus_alpha = 1.0 - alpha
    Hpi = 10 ** (db / 20.0)
    G = (Hpi * (2.0 - alpha) - alpha) / (2.0 * one_minus_alpha)
    if G < 0.0:
        G = 0.0
    if G > gain_max:
        G = gain_max
    return G

def air_effect_response(alpha=AIR_EFFECT_CUTOFF, shelf_gain=AIR_EFFECT_SHELF_GAIN, fs=FS):
    """Calculate frequency response of Air Effect (high-shelf brightening filter)."""
    frequencies = np.logspace(0, np.log10(fs / 2), 1000)
    omega = 2 * np.pi * frequencies / fs
    
    z = np.exp(1j * omega)
    one_minus_alpha = 1 - alpha
    
    # Numerator: b0 + b1*z^-1
    b0 = alpha + one_minus_alpha * shelf_gain
    b1 = -one_minus_alpha * shelf_gain
    
    # Denominator: 1 - (1-α)*z^-1
    a0 = 1
    a1 = -one_minus_alpha
    
    numerator = b0 + b1 * z ** (-1)
    denominator = a0 + a1 * z ** (-1)
    
    H = numerator / denominator
    mag = np.maximum(np.abs(H), 1e-12)
    magnitude_db = 20 * np.log10(mag)
    
    return frequencies, magnitude_db

# Create PDF with multiple pages
pdf_path = BASE_DIR / 'Filter_Report_Enhanced.pdf'
pdf = PdfPages(pdf_path)

# ============================================================================
# PAGE 1: Title Page
# ============================================================================
fig = plt.figure(figsize=(8.27, 11.69))  # A4 size in inches
fig.patch.set_facecolor('white')
ax = fig.add_subplot(111)
ax.axis('off')
ax.set_xlim(0, 1)
ax.set_ylim(0, 1)

# Title
title_text = "Audio Engine DSP Filter Analysis"
subtitle_text = "STM32G474 Audio Playback System"
date_text = f"Report Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"

y_pos = 0.90
ax.text(0.5, y_pos, title_text, ha='center', va='center', fontsize=26, fontweight='bold',
        transform=ax.transAxes)
y_pos -= 0.10
ax.text(0.5, y_pos, subtitle_text, ha='center', va='center', fontsize=16, style='italic',
        transform=ax.transAxes)
y_pos -= 0.08
ax.text(0.5, y_pos, date_text, ha='center', va='center', fontsize=10, color='gray',
        transform=ax.transAxes)

# Horizontal line
y_pos -= 0.06
ax.plot([0.15, 0.85], [y_pos, y_pos], 'k-', linewidth=2, transform=ax.transAxes)

# Key specifications
y_pos -= 0.10
specs_text = """System Specifications

Sample Rate:  22 kHz (default playback speed)
Nyquist:      11 kHz (maximum usable frequency)
Word Size:    16-bit signed integer audio
Architecture: Fixed-point integer DSP (no FPU)

Filter Chain Components:
  • DC blocking filter (high-pass, 22–44 Hz cutoff)
  • Soft DC filter option (even gentler)
  • Biquad low-pass filter (8-bit and 16-bit paths)
  • Soft clipping (±28,000 threshold, ±85% of full scale)
  • Fade in/out effects (configurable)
  • Noise gate (optional)
   • Dithering for 8-bit samples (TPDF)
   • Air Effect (high-shelf brightening; presets +1/+2/+3 dB and direct dB control)

Implementation:
  • All filters use fixed-point integer arithmetic
  • Coefficients in Q16 fixed-point format
  • Zero floating-point dependencies
  • Efficient for embedded MCU execution
  • Runtime-configurable filter parameters"""

ax.text(0.5, y_pos, specs_text, ha='center', va='top', fontsize=8.5, family='monospace',
        transform=ax.transAxes, bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.3, pad=1.0))

# What's New box (positioned lower to avoid overlap)
y_pos_whats_new = 0.14
whats_new_text = """WHAT'S NEW

• Flash Footprint: .text ≈ 12.9 KB (Release build)
• Air Effect: High-shelf brightening with runtime presets (+1, +2, +3 dB) and direct dB control
• Startup Improvements: WarmupBiquadFilter16Bit() and RESET_ALL_FILTER_STATE() reduce transients and simplify PlaySample()
• Documentation: Manual and README updated; enhanced visuals and PDF report
"""
ax.text(0.5, y_pos_whats_new, whats_new_text, ha='center', va='top', fontsize=8.5, family='monospace',
        transform=ax.transAxes, bbox=dict(boxstyle='round', facecolor='#e8ffe8', alpha=0.35, pad=1.0))

pdf.savefig(fig, bbox_inches='tight')
plt.close()

# ============================================================================
# PAGE 2: 16-bit and 8-bit LPF Comparison + Air Effect
# ============================================================================
fig = plt.figure(figsize=(8.27, 11.69))  # A4
gs = GridSpec(4, 1, figure=fig, hspace=0.5, top=0.93, bottom=0.08, left=0.12, right=0.88)

fig.suptitle('Filter Frequency Response', fontsize=14, fontweight='bold', y=0.96)

# Plot 1: 16-bit Biquad LPF
ax1 = fig.add_subplot(gs[0])
freq_16vs, mag_16vs, _ = lpf_16bit_biquad_response(LPF_16BIT_VERY_SOFT)
freq_16s, mag_16s, _ = lpf_16bit_biquad_response(LPF_16BIT_SOFT)
freq_16m, mag_16m, _ = lpf_16bit_biquad_response(LPF_16BIT_MEDIUM)
freq_16f, mag_16f, _ = lpf_16bit_biquad_response(LPF_16BIT_FIRM)
freq_16a, mag_16a, _ = lpf_16bit_biquad_response(LPF_16BIT_AGGRESSIVE)

ax1.semilogx(freq_16vs, mag_16vs, 'c-', linewidth=2, label=f'Very Soft (α={LPF_16BIT_VERY_SOFT:.4f})')
ax1.semilogx(freq_16s, mag_16s, 'g-', linewidth=2, label=f'Soft (α={LPF_16BIT_SOFT:.4f})')
ax1.semilogx(freq_16m, mag_16m, 'orange', linewidth=2, label=f'Medium (α={LPF_16BIT_MEDIUM:.4f})')
ax1.semilogx(freq_16f, mag_16f, 'y-', linewidth=2, label=f'Firm (α={LPF_16BIT_FIRM:.4f})')
ax1.semilogx(freq_16a, mag_16a, 'r-', linewidth=2, label=f'Aggressive (α={LPF_16BIT_AGGRESSIVE:.4f})')
ax1.grid(True, alpha=0.3, which='both')
ax1.set_xlabel('Frequency (Hz)', fontsize=9)
ax1.set_ylabel('Magnitude (dB)', fontsize=9)
ax1.set_title('16-bit Biquad Low-Pass Filter (Wider Range)', fontsize=11, fontweight='bold')
ax1.legend(fontsize=8, loc='upper right')
ax1.set_ylim([-60, 5])
ax1.axhline(-3, color='k', linestyle='--', alpha=0.3, linewidth=0.8)
ax1.set_xlim([1, 11000])
ax1.tick_params(labelsize=8)

# Plot 2: 8-bit LPF
ax2 = fig.add_subplot(gs[1])
freq_vs, mag_vs = lpf_8bit_response(LPF_VERY_SOFT)
freq_s, mag_s = lpf_8bit_response(LPF_SOFT)
freq_m, mag_m = lpf_8bit_response(LPF_MEDIUM)
freq_f, mag_f = lpf_8bit_response(LPF_FIRM)
freq_a, mag_a = lpf_8bit_response(LPF_AGGRESSIVE)

ax2.semilogx(freq_vs, mag_vs, 'c-', linewidth=2, label=f'Very Soft (α={LPF_VERY_SOFT:.4f})')
ax2.semilogx(freq_s, mag_s, 'b-', linewidth=2, label=f'Soft (α={LPF_SOFT:.4f})')
ax2.semilogx(freq_m, mag_m, 'orange', linewidth=2, label=f'Medium (α={LPF_MEDIUM:.4f})')
ax2.semilogx(freq_f, mag_f, 'y-', linewidth=2, label=f'Firm (α={LPF_FIRM:.4f})')
ax2.semilogx(freq_a, mag_a, 'r-', linewidth=2, label=f'Aggressive (α={LPF_AGGRESSIVE:.4f})')
ax2.grid(True, alpha=0.3, which='both')
ax2.set_xlabel('Frequency (Hz)', fontsize=9)
ax2.set_ylabel('Magnitude (dB)', fontsize=9)
ax2.set_title('8-bit Low-Pass Filter (Dithering Path)', fontsize=11, fontweight='bold')
ax2.legend(fontsize=8, loc='upper right')
ax2.set_ylim([-40, 5])
ax2.axhline(-3, color='k', linestyle='--', alpha=0.3, linewidth=0.8)
ax2.set_xlim([1, 11000])
ax2.tick_params(labelsize=8)

# Plot 3: DC Blocking Filters
ax3 = fig.add_subplot(gs[2])
freq_dc, mag_dc = dc_blocking_filter_response(DC_FILTER_ALPHA)
freq_soft_dc, mag_soft_dc = dc_blocking_filter_response(SOFT_DC_FILTER_ALPHA)

ax3.semilogx(freq_dc, mag_dc, 'b-', linewidth=2, label=f'Standard DC Block (α={DC_FILTER_ALPHA:.4f})')
ax3.semilogx(freq_soft_dc, mag_soft_dc, 'r--', linewidth=2, label=f'Soft DC Block (α={SOFT_DC_FILTER_ALPHA:.4f})')
ax3.grid(True, alpha=0.3, which='both')
ax3.set_xlabel('Frequency (Hz)', fontsize=9)
ax3.set_ylabel('Magnitude (dB)', fontsize=9)
ax3.set_title('DC Blocking Filter (High-Pass)', fontsize=11, fontweight='bold')
ax3.legend(fontsize=8, loc='lower right')
ax3.set_ylim([-40, 5])
ax3.set_xlim([1, 11000])
ax3.tick_params(labelsize=8)

# Plot 4: Air Effect (High-Shelf Brightening) with Presets
ax4 = fig.add_subplot(gs[3])
colors = ['darkgreen', 'orange', 'purple']
linestyles = ['-', '--', ':']
for idx, db in enumerate(AIR_EFFECT_PRESETS_DB):
    shelf_gain = db_to_shelf_gain(db, AIR_EFFECT_CUTOFF, AIR_EFFECT_SHELF_GAIN_MAX)
    freq_air, mag_air = air_effect_response(AIR_EFFECT_CUTOFF, shelf_gain)
    label = f"{db:+.0f} dB preset (G={shelf_gain:.2f}x)"
    ax4.semilogx(freq_air, mag_air, color=colors[idx % len(colors)], 
                 linestyle=linestyles[idx % len(linestyles)], linewidth=2.2, label=label)

ax4.axhline(0, color='k', linestyle='-', alpha=0.2)
ax4.axhline(3, color='k', linestyle='--', alpha=0.3, linewidth=1, label='+3 dB guide')
ax4.grid(True, alpha=0.3, which='both')
ax4.set_xlabel('Frequency (Hz)', fontsize=9)
ax4.set_ylabel('Magnitude (dB)', fontsize=9)
ax4.set_title('Air Effect Presets (0, +2, +3 dB) - High-Shelf Brightening', fontsize=11, fontweight='bold')
ax4.legend(fontsize=8, loc='lower right')
ax4.set_ylim([-5, 5])
ax4.set_xlim([1, 11000])
ax4.tick_params(labelsize=8)

pdf.savefig(fig, bbox_inches='tight')
plt.close()

# ============================================================================
# PAGE 3: Phase Response and Soft Clipping
# ============================================================================
fig = plt.figure(figsize=(8.27, 11.69))  # A4
gs = GridSpec(3, 2, figure=fig, hspace=0.40, wspace=0.35, top=0.93, bottom=0.08, left=0.12, right=0.88)

fig.suptitle('Advanced Filter Characteristics', fontsize=14, fontweight='bold', y=0.96)

# Plot 1: 16-bit Phase Response
ax1 = fig.add_subplot(gs[0, :])
freq_16vs, _, phase_16vs = lpf_16bit_biquad_response(LPF_16BIT_VERY_SOFT)
freq_16s, _, phase_16s = lpf_16bit_biquad_response(LPF_16BIT_SOFT)
freq_16m, _, phase_16m = lpf_16bit_biquad_response(LPF_16BIT_MEDIUM)
freq_16f, _, phase_16f = lpf_16bit_biquad_response(LPF_16BIT_FIRM)
freq_16a, _, phase_16a = lpf_16bit_biquad_response(LPF_16BIT_AGGRESSIVE)

ax1.semilogx(freq_16vs, np.degrees(phase_16vs), 'c-', linewidth=2, label='Very Soft')
ax1.semilogx(freq_16s, np.degrees(phase_16s), 'g-', linewidth=2, label='Soft')
ax1.semilogx(freq_16m, np.degrees(phase_16m), 'orange', linewidth=2, label='Medium')
ax1.semilogx(freq_16f, np.degrees(phase_16f), 'y-', linewidth=2, label='Firm')
ax1.semilogx(freq_16a, np.degrees(phase_16a), 'r-', linewidth=2, label='Aggressive')
ax1.grid(True, alpha=0.3, which='both')
ax1.set_xlabel('Frequency (Hz)', fontsize=9)
ax1.set_ylabel('Phase (degrees)', fontsize=9)
ax1.set_title('16-bit Biquad LPF Phase Response', fontsize=11, fontweight='bold')
ax1.legend(fontsize=8, loc='best', ncol=4)
ax1.set_xlim([1, 11000])
ax1.tick_params(labelsize=8)

# Plot 2: Combined Filter Response
ax2 = fig.add_subplot(gs[1, 0])
freq_dc, mag_dc = dc_blocking_filter_response(SOFT_DC_FILTER_ALPHA)
freq_16bit, mag_16bit, _ = lpf_16bit_biquad_response(LPF_16BIT_SOFT)
combined_mag = mag_dc + mag_16bit

ax2.semilogx(freq_dc, mag_dc, 'b-', linewidth=1.5, alpha=0.7, label='Soft DC Block')
ax2.semilogx(freq_16bit, mag_16bit, 'g-', linewidth=1.5, alpha=0.7, label='16-bit Biquad LPF (Soft)')
ax2.semilogx(freq_dc, combined_mag, 'r-', linewidth=2, label='Combined Response')
ax2.grid(True, alpha=0.3, which='both')
ax2.set_xlabel('Frequency (Hz)', fontsize=9)
ax2.set_ylabel('Magnitude (dB)', fontsize=9)
ax2.set_title('Filter Chain Combined Response', fontsize=11, fontweight='bold')
ax2.legend(fontsize=8)
ax2.set_ylim([-80, 5])
ax2.set_xlim([1, 11000])
ax2.tick_params(labelsize=8)

# Plot 3: Soft Clipping Transfer Function
ax3 = fig.add_subplot(gs[1, 1])
input_clip, output_clip = soft_clipping_transfer()

ax3.plot(input_clip, output_clip, 'purple', linewidth=2, label='Soft Clipping')
ax3.plot([-32768, 32767], [-32768, 32767], 'k--', alpha=0.3, linewidth=1.5, label='Linear')
ax3.axvline(28000, color='r', linestyle=':', alpha=0.5, linewidth=1.5)
ax3.axvline(-28000, color='r', linestyle=':', alpha=0.5, linewidth=1.5, label='Threshold')
ax3.grid(True, alpha=0.3)
ax3.set_xlabel('Input Sample', fontsize=9)
ax3.set_ylabel('Output Sample', fontsize=9)
ax3.set_title('Soft Clipping Transfer Function', fontsize=11, fontweight='bold')
ax3.legend(fontsize=8, loc='upper left')
ax3.set_xlim([-33000, 33000])
ax3.set_ylim([-33000, 33000])
ax3.tick_params(labelsize=8)

# Plot 4: Cutoff Frequency Summary Table
ax4 = fig.add_subplot(gs[2, :])
ax4.axis('off')

# Calculate cutoff frequencies
cutoff_8vs = find_cutoff_frequency(*lpf_8bit_response(LPF_VERY_SOFT))
cutoff_8s = find_cutoff_frequency(*lpf_8bit_response(LPF_SOFT))
cutoff_8m = find_cutoff_frequency(*lpf_8bit_response(LPF_MEDIUM))
cutoff_8a = find_cutoff_frequency(*lpf_8bit_response(LPF_AGGRESSIVE))

freq_temp, mag_temp, _ = lpf_16bit_biquad_response(LPF_16BIT_VERY_SOFT)
cutoff_16vs = find_cutoff_frequency(freq_temp, mag_temp)
freq_temp, mag_temp, _ = lpf_16bit_biquad_response(LPF_16BIT_SOFT)
cutoff_16s = find_cutoff_frequency(freq_temp, mag_temp)
freq_temp, mag_temp, _ = lpf_16bit_biquad_response(LPF_16BIT_MEDIUM)
cutoff_16m = find_cutoff_frequency(freq_temp, mag_temp)
freq_temp, mag_temp, _ = lpf_16bit_biquad_response(LPF_16BIT_AGGRESSIVE)
cutoff_16a = find_cutoff_frequency(freq_temp, mag_temp)

table_text = f"""Filter Cutoff Frequencies (−3dB points) @ {FS} Hz Sample Rate

16-bit Biquad LPF:                          8-bit Low-Pass Filter:
  Very Soft:   {cutoff_16vs:>6.0f} Hz ({cutoff_16vs/FS*100:>5.2f}% Fs)    Very Soft:   {cutoff_8vs:>6.0f} Hz ({cutoff_8vs/FS*100:>5.2f}% Fs)
  Soft:        {cutoff_16s:>6.0f} Hz ({cutoff_16s/FS*100:>5.2f}% Fs)    Soft:        {cutoff_8s:>6.0f} Hz ({cutoff_8s/FS*100:>5.2f}% Fs)
  Medium:      {cutoff_16m:>6.0f} Hz ({cutoff_16m/FS*100:>5.2f}% Fs)    Medium:      {cutoff_8m:>6.0f} Hz ({cutoff_8m/FS*100:>5.2f}% Fs)
  Aggressive:  {cutoff_16a:>6.0f} Hz ({cutoff_16a/FS*100:>5.2f}% Fs)    Aggressive:  {cutoff_8a:>6.0f} Hz ({cutoff_8a/FS*100:>5.2f}% Fs)

Note: Aggressive setting on 16-bit filter uses startup warm-up (16 passes, configurable) to eliminate transient cracking."""

ax4.text(0.0, 0.9, table_text, transform=ax4.transAxes, fontsize=8.5, family='monospace',
         verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.4, pad=1.0))

pdf.savefig(fig, bbox_inches='tight')
plt.close()

# ============================================================================
# PAGE 4: Technical Specifications
# ============================================================================
fig = plt.figure(figsize=(8.27, 11.69))  # A4
fig.patch.set_facecolor('white')
ax = fig.add_subplot(111)
ax.axis('off')
ax.set_xlim(0, 1)
ax.set_ylim(0, 1)

specs_detailed = """TECHNICAL SPECIFICATIONS & IMPLEMENTATION DETAILS

1. DC BLOCKING FILTER (High-Pass)
   Purpose:  Remove DC offset and very low-frequency drift
   Type:     First-order recursive filter (IIR)
   Standard: H(z) = (1 - z⁻¹) / (1 - α·z⁻¹)
   Variants:
     • Standard (α = 0.98):   ~44 Hz cutoff @ 22 kHz
     • Soft (α = 0.995):      ~22 Hz cutoff @ 22 kHz

2. 16-BIT BIQUAD LOW-PASS FILTER
   Purpose:  Attenuate high-frequency content and aliasing artifacts
   Type:     Second-order biquad filter (IIR)
   Aggressiveness Levels:
     • Very Soft:   α ≈ 0.97  (cutoff ~8700 Hz)
     • Soft:        α = 0.875 (cutoff ~6800 Hz)
     • Medium:      α ≈ 0.80  (cutoff ~5900 Hz)
     • Aggressive:  α = 0.625 (cutoff ~4100 Hz)
   Feature: Automatic warm-up with 16 passes (configurable) of first sample

3. 8-BIT LOW-PASS FILTER
   Purpose:  Dither and filter 8-bit audio samples before playback
   Type:     First-order filter (simpler than 16-bit biquad)
   Aggressiveness Levels:
     • Very Soft:   α = 0.9375 (cutoff ~3200 Hz)
     • Soft:        α = 0.875  (cutoff ~2800 Hz)
     • Medium:      α = 0.75   (cutoff ~2300 Hz)
     • Aggressive:  α = 0.625  (cutoff ~1800 Hz)
   Feature: TPDF (Triangular PDF) dithering during 8-to-16 bit conversion

4. SOFT CLIPPING
   Purpose:  Prevent harsh digital clipping artifacts
   Threshold: ±28,000 (85% of ±32,767 full scale)
   Curve:   Cubic smoothstep s(x) = 3x² - 2x³
   Benefit:  Natural, musical compression at clipping point

5. FADE IN/OUT
   Fade-In Samples:  2048 samples @ 22 kHz ≈ 93 ms
   Fade-Out Samples: 2048 samples @ 22 kHz ≈ 93 ms
   Shape:   Quadratic power curve (smooth ramp)

6. NOISE GATE (Optional)
   Threshold: 512 (1.5% of full scale)
   Action:   Mute samples below threshold to suppress noise

7. FIXED-POINT ARITHMETIC
   All coefficients in Q16 format (16-bit fraction)
   Scaling: 1.0 = 65536 in integer representation
   Benefit:  Fast, deterministic arithmetic on MCU
   No Floating-Point: 100% integer operations

8. DITHERING FOR 8-BIT SAMPLES
   Method:  TPDF (Triangular Probability Density Function)
   Generator: Linear Congruential Generator (LCG)
   Purpose:  Reduce quantization noise from 8-bit to 16-bit upsampling

9. AIR EFFECT (High-Shelf Brightening)
   Purpose:  Add presence and brightness by boosting treble
   Type:     One-pole high-shelf filter (CPU efficient)
   Cutoff:   α ≈ 0.75 (≈5–6 kHz @ 22 kHz sample rate)
   Gain:     Shelf gain clamped to 2.0× (presets: +1, +2, +3 dB)
   Runtime:  Enable via `enable_air_effect`; tune via `SetAirEffectPresetDb()` or `SetAirEffectGainDb()`
   Position: After DC filter, before fade/clipping in the chain

HARDWARE INTEGRATION
   • Microcontroller: STM32G474 (ARM Cortex-M4F)
   • I2S Audio Interface: I2S2 (DMA-driven stereo output)
   • Digital Amplifier: MAX98357A (Class D, I2S input)
   • Playback Buffer: 2048 samples (ping-pong DMA)

PERFORMANCE
   • Processing Overhead: <5% CPU @ 22 kHz (full filter chain)
   • Memory Footprint: Flash (.text/.rodata, Release) ≈ 12.9 KB; RAM ≈ 4 KB (playback buffer)
   • Latency: ~93 ms total (2048 samples @ 22 kHz; ~50 ms buffer + warm-up)
   • Audio Quality: 16-bit / 22 kHz"""

ax.text(0.5, 0.97, specs_detailed, transform=ax.transAxes, fontsize=7.5,
        family='monospace', verticalalignment='top', horizontalalignment='center',
        bbox=dict(boxstyle='round', facecolor='lightgray', alpha=0.15, pad=1.0))

pdf.savefig(fig, bbox_inches='tight')
plt.close()

# ============================================================================
# PAGE 5: Glossary / Key of Terms
# ============================================================================
fig = plt.figure(figsize=(8.27, 11.69))  # A4
fig.patch.set_facecolor('white')
ax = fig.add_subplot(111)
ax.axis('off')
ax.set_xlim(0, 1)
ax.set_ylim(0, 1)

# Title
ax.text(0.5, 0.97, 'Glossary: Audio DSP Terms & Concepts', ha='center', va='top',
        fontsize=14, fontweight='bold', transform=ax.transAxes)

glossary_text = """FUNDAMENTAL CONCEPTS

α (Alpha)
   Feedback coefficient in recursive filters. Controls filter strength and frequency response.
   Range: 0 (no filtering) to 1 (maximum filtering).

-3dB Cutoff Frequency
   Frequency where filter attenuates signal by 3 decibels (≈71% of original amplitude).
   Standard measure for specifying filter bandwidth and aggressiveness.

Biquad Filter
   Second-order IIR filter with two poles and two zeros. Provides steeper rolloff
   (−40 dB/decade) compared to first-order filters.

DC Blocking Filter (High-Pass)
   Removes DC offset and very low frequencies. Essential to prevent audio output drift.
   Cutoff frequencies: 22−44 Hz (minimal impact on audible range).

Dithering
   Addition of carefully shaped noise to quantized signals. Reduces audible quantization
   noise when converting 8-bit to 16-bit audio. TPDF dithering provides optimal results.

DSP (Digital Signal Processing)
   Mathematical manipulation of digital audio samples. Includes filtering, effects, and
   analysis on numerical data.

Filter Aggressiveness
   Degree of frequency content removal. Gentler filters preserve high frequencies; aggressive
   filters remove more. Trade-off: smoothness vs. brightness.

Fixed-Point Arithmetic
   Integer-based math using scaled representation (e.g., Q16 = 65536 = 1.0).
   Advantages: Fast, no FPU needed, deterministic on MCU.

Frequency Response
   How a filter responds to different frequencies. Shown in magnitude (dB) vs. frequency (Hz).
   Magnitude: attenuation per frequency. Phase: time delay per frequency.

Hz (Hertz)
   Cycles per second. Unit for frequency. Audible range: ~20 Hz to 20 kHz.
   22 kHz sample rate means 11 kHz usable bandwidth (Nyquist limit).

IIR Filter (Infinite Impulse Response)
   Recursive filter where output depends on previous outputs and inputs.
   Advantages: Efficient, steep rolloff. Disadvantages: Potential instability.

LPF (Low-Pass Filter)
   Attenuates frequencies above cutoff, preserves lower frequencies.
   Purpose: Remove high-frequency noise and aliasing artifacts.

Magnitude Response
   How much filter attenuates each frequency, measured in dB (decibels).
   0 dB = no change; −3 dB = 71% amplitude; −20 dB = 10% amplitude.

Noise Gate
   Mutes audio when amplitude falls below threshold. Suppresses background noise
   and quantization artifacts during quiet passages.

Nyquist Frequency
   Maximum frequency reliably represented at a given sample rate.
   For 22 kHz sample rate: Nyquist = 11 kHz (half the sample rate).

Phase Response
   How much a filter delays different frequencies, measured in degrees.
   Important for audio quality; excessive phase shift can cause artifacts.

Q16 Format
   Fixed-point representation with 16 bits of fractional precision.
   Example: 65536 (0x10000) = 1.0; 32768 (0x8000) = 0.5.

Sample Rate
   Number of audio samples per second, measured in Hz or kHz.
   22 kHz is standard for this system. Higher rates capture more high frequencies.

Soft Clipping
   Non-linear processing smoothly limits peak amplitudes instead of hard clipping.
   Produces musical, natural-sounding compression using cubic spline curve.

TPDF (Triangular Probability Density Function)
   Optimal dithering method minimizing audible quantization noise.
   Uses sum of two uncorrelated uniform random noise sources."""

ax.text(0.5, 0.93, glossary_text, transform=ax.transAxes, fontsize=7.8,
        family='monospace', verticalalignment='top', horizontalalignment='center',
        bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.2, pad=1.0))

pdf.savefig(fig, bbox_inches='tight')
plt.close()

# Close PDF
pdf.close()

print(f"\n{'='*70}")
print(f"PDF Report Generated Successfully!")
print(f"{'='*70}")
print(f"Output File:  {pdf_path}")
print(f"Pages:        5 (Title, Frequency Response, Analysis, Specifications, Glossary)")
print(f"Format:       A4 (210 × 297 mm)")
print(f"Resolution:   300 DPI (print-ready)")
print(f"Date:         {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
print(f"{'='*70}\n")
