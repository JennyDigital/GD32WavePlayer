# Audio Engine Flowchart

This document contains flowcharts showing the architecture and data flow of the audio engine.

## 🎯 Main System Architecture

```mermaid
flowchart TB
  subgraph Application["Application Layer"]
    User["User Code<br/>(main.c)"]
    Callbacks["Hardware Callbacks<br/>• ReadVolume()<br/>• DAC_MasterSwitch()<br/>• MX_I2S2_Init()"]
  end

  subgraph Engine["Audio Engine Core"]
    Init["AudioEngine_Init()"]
    Play["PlaySample()"]
    Control["Playback Control<br/>• Pause/Resume<br/>• WaitForEnd"]
    Config["Filter Configuration<br/>• SetFilterConfig()<br/>• SetLpf16BitLevel()<br/>• SetAirEffectPresetDb()"]
  end

  subgraph DSP["DSP Processing Pipeline"]
    Process16["ProcessNextWaveChunk()<br/>(16-bit)"]
    Process8["ProcessNextWaveChunk_8_bit()<br/>(8-bit)"]
    Filters["Filter Chain<br/>• Biquad LPF<br/>• DC Block<br/>• Air Effect<br/>• Soft Clip"]
  end

  subgraph Hardware["Hardware Layer"]
    I2S["I2S2 Peripheral<br/>(DMA Mode)"]
    DMA["DMA1 Channel 1<br/>(Circular Buffer)"]
    DAC["MAX98357A<br/>Digital Amplifier"]
    Speaker["Speaker Output"]
  end

  subgraph ISR["Interrupt Service Routines"]
    HalfCplt["HAL_I2S_TxHalfCpltCallback()"]
    FullCplt["HAL_I2S_TxCpltCallback()"]
  end

  User -->|"1. Initialize"| Init
  Callbacks -->|"Provide"| Init
  User -->|"2. Configure Filters"| Config
  User -->|"3. Start Playback"| Play
  User -->|"4. Control"| Control

  Play -->|"Start DMA"| I2S
  I2S <-->|"Transfer"| DMA
  
  DMA -->|"Half Complete"| HalfCplt
  DMA -->|"Full Complete"| FullCplt
  
  HalfCplt -->|"Process First Half"| Process16
  HalfCplt -->|"or"| Process8
  FullCplt -->|"Process Second Half"| Process16
  FullCplt -->|"or"| Process8
  
  Process16 --> Filters
  Process8 --> Filters
  
  Filters -->|"Write to Buffer"| DMA
  DMA -->|"I2S Stream"| DAC
  DAC --> Speaker

  style User fill:#e1f5ff,color:#000000
  style Engine fill:#fff4e1,color:#000000
  style DSP fill:#ffe1f5,color:#000000
  style Hardware fill:#e1ffe1,color:#000000
  style ISR fill:#ffe1e1,color:#000000
```

## 🔄 Playback Initialization Flow

```mermaid
flowchart TD
  Start([Application calls<br/>PlaySample]) --> CheckState{Playback<br/>Already Active?}
  
  CheckState -->|Yes| ReturnError[Return PB_Error]
  CheckState -->|No| StoreParams["Store Parameters:<br/>• Sample pointer<br/>• Sample size<br/>• Sample rate<br/>• Bit depth<br/>• Mode: mono or stereo"]
  
  StoreParams --> ResetState["Reset Engine State:<br/>• Clear filter states<br/>• Reset fade counters<br/>• Initialize pointers"]
  
  ResetState --> CheckDepth{16-bit or<br/>8-bit?}
  
  CheckDepth -->|16-bit| Check16Filters{Biquad LPF<br/>Enabled?}
  CheckDepth -->|8-bit| StartDMA
  
  Check16Filters -->|Yes| Warmup["Warm-up Biquad Filter<br/>16 cycles<br/>Prevent startup transient"]
  Check16Filters -->|No| StartDMA
  
  Warmup --> StartDMA["Start I2S DMA Transfer:<br/>HAL_I2S_Transmit_DMA"]
  
  StartDMA --> CheckDMAResult{DMA Start<br/>Success?}
  
  CheckDMAResult -->|No| ReturnError
  CheckDMAResult -->|Yes| SetState["Set State = PB_Playing"]
  
  SetState --> EnableDAC["Enable DAC:<br/>AudioEngine_DACSwitch: DAC_ON"]
  
  EnableDAC --> ReturnSuccess[Return PB_Playing]
  ReturnError --> End1([End])
  ReturnSuccess --> End2([End])

  style Start fill:#e1f5ff,color:#000000
  style ReturnSuccess fill:#e1ffe1,color:#000000
  style ReturnError fill:#ffe1e1,color:#000000
```

## 🎚️ DMA Interrupt Processing Flow

```mermaid
flowchart TD
  DMAInt([DMA Interrupt Triggered]) --> WhichHalf{Which Half?}
  
  WhichHalf -->|First Half| HalfCplt["HAL_I2S_TxHalfCpltCallback()"]
  WhichHalf -->|Second Half| FullCplt["HAL_I2S_TxCpltCallback()"]
  
  HalfCplt --> SetHalf1["Set half_to_fill = FIRST"]
  FullCplt --> SetHalf2["Set half_to_fill = SECOND"]
  
  SetHalf1 --> CheckDepth1{Sample Depth?}
  SetHalf2 --> CheckDepth1
  
  CheckDepth1 -->|16-bit| Process16["ProcessNextWaveChunk()<br/>(int16_t*)"]
  CheckDepth1 -->|8-bit| Process8["ProcessNextWaveChunk_8_bit()<br/>(uint8_t*)"]
  
  Process16 --> ChunkLoop16["For each sample in chunk<br/>(CHUNK_SZ samples)"]
  Process8 --> ChunkLoop8["For each sample in chunk<br/>(CHUNK_SZ samples)"]
  
  ChunkLoop16 --> ReadVol16["Read Volume:<br/>AudioEngine_ReadVolume()"]
  ChunkLoop8 --> Convert8to16["Convert 8-bit to 16-bit<br/>+ TPDF Dithering"]
  
  Convert8to16 --> ReadVol8["Read Volume:<br/>AudioEngine_ReadVolume()"]
  
  ReadVol16 --> ApplyFilters16["Apply DSP Filter Chain<br/>(see Filter Pipeline)"]
  ReadVol8 --> ApplyFilters8["Apply DSP Filter Chain<br/>(see Filter Pipeline)"]
  
  ApplyFilters16 --> ApplyVol16["Apply Volume:<br/>sample × (volume/255)"]
  ApplyFilters8 --> ApplyVol8["Apply Volume:<br/>sample × (volume/255)"]
  
  ApplyVol16 --> WriteBuffer16["Write to DMA Buffer"]
  ApplyVol8 --> WriteBuffer8["Write to DMA Buffer"]
  
  WriteBuffer16 --> CheckEnd{More samples<br/>to play?}
  WriteBuffer8 --> CheckEnd
  
  CheckEnd -->|Yes| AdvancePtr["AdvanceSamplePointer()<br/>Move to next chunk"]
  CheckEnd -->|No| FadeOut["Apply Fade-Out<br/>Return PB_Idle"]
  
  AdvancePtr --> ReturnPlaying[Return PB_Playing]
  FadeOut --> StopDMA["Stop DMA Transfer"]
  StopDMA --> DisableDAC["Disable DAC:<br/>AudioEngine_DACSwitch(DAC_OFF)"]
  DisableDAC --> ReturnIdle[Return PB_Idle]
  
  ReturnPlaying --> ISRExit([ISR Complete])
  ReturnIdle --> ISRExit

  style DMAInt fill:#ffe1e1,color:#000000
  style ReturnPlaying fill:#e1ffe1,color:#000000
  style ReturnIdle fill:#fff4e1,color:#000000
```

## 🎛️ DSP Filter Chain Pipeline (16-bit)

```mermaid
flowchart LR
  Input["Input Sample<br/>(16-bit signed)"] --> BiquadCheck{Biquad LPF<br/>Enabled?}
  
  BiquadCheck -->|Yes| Biquad["Biquad Low-Pass Filter<br/>α = 0.625 to 0.97<br/>Second-order IIR"]
  BiquadCheck -->|No| DCCheck
  
  Biquad --> DCCheck{DC Blocking<br/>Enabled?}
  
  DCCheck -->|Yes| DCFilter["DC Blocking Filter<br/>α = 0.98 (standard)<br/>or 0.995 (soft)<br/>Remove DC offset"]
  DCCheck -->|No| AirCheck
  
  DCFilter --> AirCheck{Air Effect<br/>Enabled?}
  
  AirCheck -->|Yes| AirEffect["Air Effect High-Shelf<br/>Boost high frequencies<br/>+1, +2, or +3 dB presets"]
  AirCheck -->|No| FadeCheck
  
  AirEffect --> FadeCheck{Fade In/Out<br/>Active?}
  
  FadeCheck -->|Yes| Fade["Apply Fade Ramp<br/>Quadratic curve<br/>Smooth transitions"]
  FadeCheck -->|No| NoiseCheck
  
  Fade --> NoiseCheck{Noise Gate<br/>Enabled?}
  
  NoiseCheck -->|Yes| NoiseGate["Noise Gate<br/>Threshold: ±512<br/>Silence low-level noise"]
  NoiseCheck -->|No| ClipCheck
  
  NoiseGate --> ClipCheck{Soft Clipping<br/>Enabled?}
  
  ClipCheck -->|Yes| Clip["Soft Clipping<br/>Cubic curve above ±28,000<br/>Prevent harsh distortion"]
  ClipCheck -->|No| Output
  
  Clip --> Output["Output Sample<br/>(Filtered 16-bit)"]

  style Input fill:#e1f5ff,color:#000000
  style Output fill:#e1ffe1,color:#000000
  style Biquad fill:#ffe1f5,color:#000000
  style DCFilter fill:#fff4e1,color:#000000
  style AirEffect fill:#f5e1ff,color:#000000
  style Fade fill:#e1ffe1,color:#000000
  style NoiseGate fill:#ffe1e1,color:#000000
  style Clip fill:#ffe1f5,color:#000000
```

## 🎛️ DSP Filter Chain Pipeline (8-bit)

```mermaid
flowchart LR
  Input8["Input Sample<br/>(8-bit unsigned)"] --> Convert["Convert to 16-bit:<br/>• Subtract 127<br/>• Scale to ±32K<br/>• Add TPDF dithering"]
  
  Convert --> OnePoleCheck{One-Pole LPF<br/>Enabled?}
  
  OnePoleCheck -->|Yes| OnePole["One-Pole Low-Pass Filter<br/>α = 0.625 to 0.9375<br/>First-order IIR<br/>+ Makeup gain"]
  OnePoleCheck -->|No| DCCheck8
  
  OnePole --> DCCheck8{DC Blocking<br/>Enabled?}
  
  DCCheck8 -->|Yes| DC8["DC Blocking Filter<br/>(Same as 16-bit)"]
  DCCheck8 -->|No| Rest8["Remaining Pipeline:<br/>• Air Effect<br/>• Fade<br/>• Noise Gate<br/>• Soft Clipping<br/>(Same as 16-bit)"]
  
  DC8 --> Rest8
  
  Rest8 --> Output8["Output Sample<br/>(Filtered 16-bit)"]

  style Input8 fill:#e1f5ff,color:#000000
  style Convert fill:#fff4e1,color:#000000
  style OnePole fill:#ffe1f5,color:#000000
  style Output8 fill:#e1ffe1,color:#000000
```

## 🔧 Filter Configuration Flow

```mermaid
flowchart TD
  UserConfig([User calls<br/>SetFilterConfig]) --> Batch{Batch Update or<br/>Single Function?}
  
  Batch -->|Batch| GetCurrent["GetFilterConfig<br/>Read current settings"]
  Batch -->|Single| DirectSet["Call specific setter:<br/>• SetLpf16BitLevel<br/>• SetAirEffectPresetDb<br/>• SetSoftClippingEnable"]
  
  GetCurrent --> Modify["Modify FilterConfig_TypeDef:<br/>• enable_16bit_biquad_lpf<br/>• enable_air_effect<br/>• lpf_16bit_level<br/>• etc."]
  
  Modify --> SetBatch["SetFilterConfig<br/>Apply all changes"]
  
  SetBatch --> UpdateState["Update Internal State:<br/>• Alpha coefficients<br/>• Enable flags<br/>• Preset indices"]
  DirectSet --> UpdateState
  
  UpdateState --> CheckActive{Playback<br/>Active?}
  
  CheckActive -->|Yes| ApplyNext["Filters take effect<br/>on next DMA chunk"]
  CheckActive -->|No| Ready["Ready for next<br/>PlaySample call"]
  
  ApplyNext --> Done1([Configuration Complete])
  Ready --> Done1

  style UserConfig fill:#e1f5ff,color:#000000
  style Done1 fill:#e1ffe1,color:#000000
```

## 🎚️ Volume Control Flow

```mermaid
flowchart TD
  DMAChunk([Processing DMA Chunk]) --> ReadVolCall["Call: AudioEngine_ReadVolume()"]
  
  ReadVolCall --> CheckMode{Digital or<br/>Analog Volume?}
  
  CheckMode -->|Digital| ReadGPIO["Read GPIO Pins:<br/>OPT1, OPT2, OPT3<br/>(3-bit encoding)"]
  CheckMode -->|Analog| ReadADC["Read ADC:<br/>12-bit potentiometer<br/>value"]
  
  ReadGPIO --> PackBits["Pack bits:<br/>v = (OPT3<<2 | OPT2<<1 | OPT1)"]
  ReadADC --> ScaleADC["Scale ADC:<br/>v = (raw_adc * 65535) / 4095"]
  
  PackBits --> InvertGPIO["Invert:<br/>v = 7 - v"]
  ScaleADC --> EnsureMin1["Ensure minimum:<br/>if (v < 1) v = 1"]
  
  InvertGPIO --> ScaleGPIO["Scale to 1-65535:<br/>v = (v * 65535) / 7"]
  
  ScaleGPIO --> NonLinearCheck{Non-Linear<br/>Response Enabled?}
  EnsureMin1 --> NonLinearCheck
  
  NonLinearCheck -->|Yes| ApplyGamma["Apply Gamma Curve:<br/>output = input^(1/γ)<br/>γ = 2.0 (default)<br/>Power law response"]
  NonLinearCheck -->|No| ReturnLinear["Return linear<br/>volume (1-65535)"]
  
  ApplyGamma --> ReturnNonLinear["Return non-linear<br/>volume (1-65535)"]
  
  ReturnLinear --> ApplyToSample["Apply to sample:<br/>sample * (volume/65535)"]
  ReturnNonLinear --> ApplyToSample
  
  ApplyToSample --> Continue([Continue DSP Pipeline])

  style DMAChunk fill:#ffe1e1,color:#000000
  style ApplyGamma fill:#f5e1ff,color:#000000
  style Continue fill:#e1ffe1,color:#000000
```

## 📊 State Machine Diagram

```mermaid
stateDiagram-v2
  [*] --> PB_Idle: AudioEngine_Init()
  
  PB_Idle --> PB_Playing: PlaySample()
  PB_Idle --> PB_Error: Init Failed
  
  PB_Playing --> PB_Paused: PausePlayback()
  PB_Playing --> PB_Idle: Sample Complete
  PB_Playing --> PB_PlayingFailed: DMA Error
  PB_Playing --> PB_Playing: DMA Interrupts<br/>Process Chunks
  
  PB_Paused --> PB_Playing: ResumePlayback()
  PB_Paused --> PB_Idle: Sample Complete<br/>while paused
  
  PB_PlayingFailed --> PB_Idle: ShutDownAudio()
  PB_Error --> PB_Idle: ShutDownAudio()
  
  PB_Idle --> [*]: ShutDownAudio()

  note right of PB_Playing
    Active DMA transfer
    Processing audio chunks
    Filters applied in real-time
  end note
  
  note right of PB_Paused
    DMA transfer continues
    Output faded to silence
    Can resume with fade-in
  end note
```

## 🔄 Air Effect Preset Auto-Control

```mermaid
flowchart TD
  UserCall([User calls<br/>SetAirEffectPresetDb preset_index]) --> StoreIndex["Store preset_index"]
  
  StoreIndex --> CheckIndex{preset_index is 0?}
  
  CheckIndex -->|Yes| DisableAir["SetAirEffectEnable 0<br/>Disable air effect"]
  CheckIndex -->|No| GetPresetDB["Get dB value from preset:<br/>preset 1 = +1 dB<br/>preset 2 = +2 dB<br/>preset 3 = +3 dB"]
  
  GetPresetDB --> ConvertDB["Convert dB to Q16 gain:<br/>gain_q16 = 65536 × 10 to the power dB over 20"]
  
  ConvertDB --> SetGain["Set air effect gain:<br/>air_effect_shelf_gain = gain_q16"]
  
  SetGain --> EnableAir["SetAirEffectEnable 1<br/>Enable air effect"]
  
  DisableAir --> UpdateConfig["Update filter_cfg.enable_air_effect"]
  EnableAir --> UpdateConfig
  
  UpdateConfig --> Done([Air Effect Configured])

  style UserCall fill:#e1f5ff,color:#000000
  style EnableAir fill:#e1ffe1,color:#000000
  style DisableAir fill:#ffe1e1,color:#000000
  style Done fill:#e1ffe1,color:#000000
```

## 📈 Buffer Management (Ping-Pong DMA)

```mermaid
flowchart TD
  subgraph Buffer["DMA Buffer (2048 samples)"]
    FirstHalf["First Half<br/>(samples 0-1023)<br/>CHUNK_SZ"]
    SecondHalf["Second Half<br/>(samples 1024-2047)<br/>CHUNK_SZ"]
  end
  
  subgraph DMA_Transfer["DMA Transfer Cycle"]
    Transfer1["Transfer samples 0-1023<br/>to I2S"]
    Transfer2["Transfer samples 1024-2047<br/>to I2S"]
  end
  
  subgraph Processing["Background Processing"]
    Process1["Process & fill<br/>First Half<br/>while DMA reads Second Half"]
    Process2["Process & fill<br/>Second Half<br/>while DMA reads First Half"]
  end
  
  Start([DMA Started]) --> Transfer1
  Transfer1 -.->|Half Complete IRQ| Process2
  Transfer1 --> Transfer2
  Transfer2 -.->|Full Complete IRQ| Process1
  Transfer2 --> Transfer1
  
  Process1 -.->|Write new data| FirstHalf
  Process2 -.->|Write new data| SecondHalf
  
  FirstHalf -.->|Read by DMA| Transfer1
  SecondHalf -.->|Read by DMA| Transfer2

  style Start fill:#e1f5ff,color:#000000
  style FirstHalf fill:#ffe1f5,color:#000000
  style SecondHalf fill:#fff4e1,color:#000000
  style Process1 fill:#e1ffe1,color:#000000
  style Process2 fill:#e1ffe1,color:#000000
```

---

## 📖 How to View These Flowcharts

### In VS Code
1. Install the "Markdown Preview Mermaid Support" extension
2. Open this file and press `Ctrl+Shift+V` (or `Cmd+Shift+V` on Mac)
3. Flowcharts will render automatically

### On GitHub
GitHub natively renders Mermaid diagrams in markdown files. Just view this file on GitHub.

### In Other Tools
- **Mermaid Live Editor**: Copy diagram code to https://mermaid.live
- **Obsidian**: Renders Mermaid natively
- **Notion**: Supports Mermaid diagrams
- **GitLab**: Native Mermaid support
- **Docusaurus**: Native Mermaid support

## 🔗 Related Documentation

- [AUDIO_ENGINE_MANUAL.md](AUDIO_ENGINE_MANUAL.md) - Complete technical manual
- [API_REFERENCE.md](API_REFERENCE.md) - Function reference
- [QUICK_REFERENCE.md](QUICK_REFERENCE.md) - Common patterns
- [audio_engine.c](../Core/Libraries/audio_engine.c) - Implementation source code

---

*Last updated: 2026-02-01*
*Part of the Audio Engine Documentation Suite*
