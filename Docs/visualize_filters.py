#!/usr/bin/env python3
"""
Filter Characteristics Visualization for Audio Engine
Generates frequency response and transfer function plots for the DSP filters.
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
LPF_16BIT_ALPHA = LPF_16BIT_SOFT  # Default alpha for testing (from audio_engine.h)

# 8-bit LPF levels (one-pole filter, narrower range for stability)
LPF_VERY_SOFT = 61440 / 65536  # 0.9375
LPF_SOFT = 57344 / 65536  # 0.875
LPF_MEDIUM = 49152 / 65536  # 0.75
LPF_FIRM = 45056 / 65536  # 0.6875
LPF_AGGRESSIVE = 40960 / 65536  # 0.625

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
    magnitude_db = 20 * np.log10(np.abs(H))
    
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
    magnitude_db = 20 * np.log10(np.abs(H))
    
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
    magnitude_db = 20 * np.log10(np.abs(H))
    
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

# Create the figure
fig = plt.figure(figsize=(14, 10))
gs = GridSpec(3, 2, figure=fig, hspace=0.3, wspace=0.3)

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

# Plot 2: 16-bit Biquad LPF
ax2 = fig.add_subplot(gs[0, 1])
freq_vs, mag_vs = lpf_16bit_biquad_response(LPF_16BIT_VERY_SOFT)
freq_s, mag_s = lpf_16bit_biquad_response(LPF_16BIT_SOFT)
freq_m, mag_m = lpf_16bit_biquad_response(LPF_16BIT_MEDIUM)
freq_f, mag_f = lpf_16bit_biquad_response(LPF_16BIT_FIRM)
freq_a, mag_a = lpf_16bit_biquad_response(LPF_16BIT_AGGRESSIVE)

ax2.semilogx(freq_vs, mag_vs, 'c-', linewidth=2, label=f'Very Soft (α={LPF_16BIT_VERY_SOFT:.4f})')
ax2.semilogx(freq_s, mag_s, 'b-', linewidth=2, label=f'Soft (α={LPF_16BIT_SOFT:.4f})')
ax2.semilogx(freq_m, mag_m, 'orange', linewidth=2, label=f'Medium (α={LPF_16BIT_MEDIUM:.4f})')
ax2.semilogx(freq_f, mag_f, 'y-', linewidth=2, label=f'Firm (α={LPF_16BIT_FIRM:.4f})')
ax2.semilogx(freq_a, mag_a, 'r-', linewidth=2, label=f'Aggressive (α={LPF_16BIT_AGGRESSIVE:.4f})')
ax2.grid(True, alpha=0.3, which='both')
ax2.set_xlabel('Frequency (Hz)')
ax2.set_ylabel('Magnitude (dB)')
ax2.set_title('16-bit Biquad Low-Pass Filter (Various Levels)')
ax2.legend()
ax2.set_ylim([-60, 5])

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
ax3.legend()
ax3.set_ylim([-40, 5])

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

# Plot 5: Combined Frequency Response (All Filters)
ax5 = fig.add_subplot(gs[2, :])
freq_dc, mag_dc = dc_blocking_filter_response(SOFT_DC_FILTER_ALPHA)
freq_16bit, mag_16bit = lpf_16bit_biquad_response(LPF_16BIT_ALPHA)

# Calculate combined response (DC block + 16-bit LPF)
combined_mag = mag_dc + mag_16bit

ax5.semilogx(freq_dc, mag_dc, 'b-', linewidth=1.5, alpha=0.7, label='Soft DC Block')
ax5.semilogx(freq_16bit, mag_16bit, 'g-', linewidth=1.5, alpha=0.7, label='16-bit Biquad LPF')
ax5.semilogx(freq_dc, combined_mag, 'r-', linewidth=2.5, label='Combined Response (DC Block + LPF)')
ax5.grid(True, alpha=0.3, which='both')
ax5.set_xlabel('Frequency (Hz)')
ax5.set_ylabel('Magnitude (dB)')
ax5.set_title('Combined Filter Chain Response (16-bit path)')
ax5.legend()
ax5.set_ylim([-80, 5])
ax5.axhline(0, color='k', linestyle='-', alpha=0.2)
ax5.axhline(-3, color='k', linestyle='--', alpha=0.2, label='-3dB')

# Add overall title
fig.suptitle('Audio Engine DSP Filter Characteristics\nSample Rate: 22 kHz', 
             fontsize=16, fontweight='bold')

# Save the figure
output_file = BASE_DIR / 'filter_characteristics.png'
plt.savefig(output_file, dpi=300, bbox_inches='tight')
print(f"Filter characteristics plot saved to: {output_file}")

# Also display
plt.show()
