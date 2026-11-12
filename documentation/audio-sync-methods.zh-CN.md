# DeSmuME 音频同步方式说明

[English](audio-sync-methods.en.md)

本文说明 DeSmuME 核心支持的音频同步方式，包括为什么需要同步、`N` / `Z` / `P`
三种方法的区别，以及如何选择。

## 1. 这些方式在哪里

音频同步由 **metaspu** 实现。该子系统移植自 PCSX2 的 SPU2-X 插件。相关配置分为两层：

| 层级 | 枚举 | 取值 | 含义 |
| --- | --- | --- | --- |
| 同步模式 | `ESynchMode` | `ESynchMode_DualSynchAsynch`、`ESynchMode_Synchronous` | 音频是否经过 metaspu 的样本队列 |
| 同步方式 | `ESynchMethod` | `ESynchMethod_N`、`ESynchMethod_Z`、`ESynchMethod_P` | `Synchronous` 模式下具体使用的算法 |

因此，`Dual Asynchronous`（双同步/异步）是一种独立的**模式**，不属于本文所说的三种
**方式**。选择 `Dual Asynchronous` 时会绕过同步器：音频按输出缓冲区需求即时混音，
不做采样率校正。下面三种方式都是 `Synchronous` 模式下的选项。

默认值（`src/NDSSystem.cpp`）：

```cpp
SPU_sync_mode = ESynchMode_Synchronous;
SPU_sync_method = ESynchMethod_N;
```

各前端的选择入口：

- GTK：*Audio Synchronization* 菜单
- Windows：*Config > Sound Settings* 中的 "Synchronizer Method" 分组
- Cocoa：*Preferences > Sound* 中的 "SPU Sound Synchronization Method"

只有在构建中包含 SoundTouch 时，才会提供 `P` 选项。

## 2. 为什么需要音频同步

DS 的 SPU 按模拟器的时间节奏产生 PCM，而宿主声卡按固定速率消费 PCM。两个时钟不可能
完全一致：模拟速度会在 100% 附近波动，声卡也有自己的时钟。

如果不做校正，生产者与消费者之间的缓冲区会慢慢漂移：

- **欠载（Underrun）**：声卡需要样本时，模拟器还没产生出来。表现为断续、爆音或卡顿。
- **溢出（Overrun）**：模拟器产生的新样本无处可放。表现为丢音和一阵杂音。

同步器位于 SPU 与声卡之间，用来补偿这种漂移。基本策略有两类：

1. **重采样/跳样本**：不改变生产速度，让消费者以略快或略慢的速度读取队列。
2. **时间伸缩（Time-stretch）**：直接改变音频的播放速度，同时尽量保持音高。

`N` 和 `Z` 属于第一类，`P` 属于第二类。

使用同步方式时的数据流：

```
SPU 混音器
   │  PCM 帧
   ▼
ISynchronizingAudioBuffer  (N / Z / P)
   │  经过速率校正的 PCM
   ▼
宿主音频设备
```

## 3. 三种方式一览

| | **N** | **Z** | **P** |
| --- | --- | --- | --- |
| 界面名称 | N: Crossfade resampler (low CPU) | Z: Micro-resampler (low latency) | P: Time-stretch (keeps pitch) |
| 实现类 | `NitsujaSynchronizer` | `ZeromusSynchronizer` | `PCSX2Synchronizer` + `SndBuffer` |
| 策略 | 样本队列上的交叉淡化 / 往返扫描 | 连续的微幅重采样 | SoundTouch 时间伸缩 |
| 外部依赖 | 无 | 无 | SoundTouch（可选） |
| 标称延迟 | 低 | 很低 | 较高 |
| CPU 开销 | 低 | 很低 | 最高 |
| 校正时是否保持音高 | 重采样时不保持 | 有轻微音高漂移 | 保持 |
| 典型伪影 | 交叉淡化使瞬态变糊；严重变慢时有短暂倒放 | 持续音上可能有轻微混叠/咔哒声 | 时间伸缩的糊音/抖动 |
| 可用性 | 所有构建 | 所有构建 | 含 `HAVE_LIBSOUNDTOUCH` 或 `DESMUME_COCOA` / MSVC 的构建 |

## 4. 方式 N —— Nitsuja

**实现：** `src/metaspu/metaspu.cpp` 中的 `NitsujaSynchronizer`

### 工作原理

`N` 用 `std::vector` 维护一个立体声帧队列。每次输出请求时，它根据队列长度和请求帧数
选择处理方式：

- 数据太少（`queued <= 0x200`，即 512 帧）时不输出，等待积累。这是欠载保护。
- 队列长度与请求量接近时，直接按原样拷贝输出。这是正常速度路径。
- 当队列明显长于请求量（`queued > 900` 或 `audiosize > queued * 2`）时进行重采样：
  - **快进**（`audiosize <= queued`）：对队列开头和结尾的样本做交叉淡化，
    在减少输出帧数的同时避免硬切。
  - **慢放**（`audiosize > queued`）：用 ping-pong（往返）索引选择样本，
    让读取位置先向前再短暂向后扫过，而不是直接跳变。它还会在队列两端寻找
    斜率较小的位置作为起止点，以进一步减少咔哒声。

这里没有 DSP、没有外部库，也不存在持续性的采样率转换；只有队列偏离正常窗口时，
才会成块地进行校正。

### 特点

- **优点：** 无依赖、开销极低、所有构建都可用，也是历史默认值。
- **缺点：** 快进时交叉淡化会让瞬态变糊；慢放时会有意短时间倒放（只有较大校正时
  才明显）。它更像"尽力而为"的重采样器，而不是连续的时钟校正器。
- **适合：** 模拟器基本跑在 100% 速度附近，且希望使用简单、无依赖方案的普通场景。

### 源码

```
src/metaspu/metaspu.cpp   NitsujaSynchronizer（类约在第 191 行起）
src/metaspu/metaspu.h     ESynchMethod_N
```

## 5. 方式 Z —— Zeromus

**实现：** `src/metaspu/metaspu.cpp` 中的 `ZeromusSynchronizer`

### 工作原理

`Z` 把音频放进 `Adjustobuf` 队列，并施加非常小、连续的播放速率校正。与 `N` 不同，
它不会等到误差很大才动作，而是每消费一帧都微调一次速率。

关键参数：

```cpp
// Release 构建
adjustobuf(200, 1000)      // minLatency, maxLatency（单位：帧）
// Debug 构建
adjustobuf(22000, 44000)   // 调试时使用大得多的窗口

kAverageSize = 80000;      // 滑动平均窗口（单位：输出帧）
targetLatency = (minLatency + maxLatency) / 2;
```

每次出队时都会记录当前队列长度，并对最近 `kAverageSize` 帧（80,000 帧，44.1 kHz
下约 1.8 秒）求滑动平均。目标速率为：

```cpp
if (averageSize < targetLatency)
    targetRate = 1.0f - (targetLatency - averageSize) / kAverageSize;
else if (averageSize > targetLatency)
    targetRate = 1.0f + (averageSize - targetLatency) / kAverageSize;
else
    targetRate = 1.0f;
```

由于除数是 `kAverageSize`，即使延迟误差很大，速率变化也只有不到百分之一。例如在
Release 窗口下，速率大致保持在 `0.99–1.01`。校正被摊到数秒内完成，通常听不出来。

样本消费使用小数游标：

```cpp
cursor += rate;
while (cursor > 1.0f) {
    cursor -= 1.0f;
    curr = buffer.pop();   // 前进一个源帧
}
输出 curr;                 // 否则保持当前帧
```

这本质上是最邻近/采样保持式重采样：偶尔重复或跳过一帧，但发生频率非常低。

`mixqueue_go` 还提供一个小型启动门限：队列超过 200 帧才开始播放，队列为空时停止。

### 特点

- **优点：** 延迟极低（标称目标约 600 帧，44.1 kHz 下约 14 ms；窗口 200–1000 帧），
  CPU 开销极低，无依赖，校正平滑，通常很少产生可听见的咔哒声。
- **缺点：** 不做保音高的时间伸缩，因此微小的速率变化也会带来轻微音高偏移
  （几个音分级别）；在持续的音调素材上，重复/跳过样本可能产生很轻微的伪影。
- **适合：** 追求低延迟，或希望在不引入库依赖的情况下获得平滑漂移校正的用户。

### 源码

```
src/metaspu/metaspu.cpp   ZeromusSynchronizer（类约在第 49 行起）
src/metaspu/metaspu.h     ESynchMethod_Z
```

## 6. 方式 P —— PCSX2 / SPU2-X

**实现：** `src/metaspu/metaspu.cpp` 中的 `PCSX2Synchronizer`，底层由
`SndBuffer`（`src/metaspu/SndOut.cpp`）和 SoundTouch 库
（`src/metaspu/Timestretcher.cpp`）支撑。

### 工作原理

`P` 采用 SPU2-X 的方案。PCM 帧被写入 `SndBuffer` 环形缓冲区，目标水位约为 50%。
另一层逻辑把帧转换成 float，送入 SoundTouch，再取回经过时间伸缩的帧。

SoundTouch 改变的是音频的 **tempo（速度）**，同时保持音高。tempo 由环形缓冲区的
填充程度驱动：

```cpp
// GetStatusPct(): 0.0 = 50% 满，+1.0 = 满，-1.0 = 空
pSoundTouch->setTempo(newTempo);
```

tempo 被限制在 `0.10–10.0` 之间。大约 `0.97–1.045` 的小偏差会被视为"正常速度"，
tempo 会平滑滑回 `1.0`；只有更大的偏差才真正触发时间伸缩。

当缓冲区欠载或溢出时，该方法会做紧急 tempo 调整
（`timeStretchUnderrun()` / `timeStretchOverrun()`），并在溢出时丢弃一部分已缓存帧
来腾出空间。

默认 `SndOutLatencyMS = 160` 时，环形缓冲区大小为 `160 ms × 2.0 = 320 ms`，
目标缓存约 160 ms 的音频。这比 `N` / `Z` 的延迟明显更高，但给了时间伸缩器足够的
工作空间。

### 特点

- **优点：** 保持音高；对持续漂移和速度偏差处理得比较自然；在速度比例变化时，
  三种方法中听感通常最"音乐化"。
- **缺点：** CPU 开销最高、延迟最高，并且依赖 SoundTouch。较大或快速变化的校正
  可能产生时间伸缩伪影（糊音或抖动）。没有 SoundTouch 的构建无法使用。
- **适合：** 重视音高保持、可以接受额外延迟的用户；也适合模拟速度明显偏离 100% 的
  场景。

### 可用性

只有满足以下条件之一时才会编译 `P`：

- 定义了 `HAVE_LIBSOUNDTOUCH`（Meson 始终定义；Autotools 找到系统 SoundTouch 时定义）
- 定义了 `DESMUME_COCOA`（macOS 构建）
- 定义了 `_MSC_VER`（Windows 构建，直接编译自带源码）

`src/metaspu/SoundTouch/` 中自带的 SoundTouch 版本为 **2.4.1**，采用
**LGPL 2.1** 许可证。Meson 优先使用系统库，未找到时编译自带源码；Autotools
需要系统 SoundTouch。Windows 和 macOS 则直接编译自带源码。自带库支持 x86、
x64 和 ARM64；在 ARM64 上使用通用 C++ 路径，因为 MMX/SSE 优化仅适用于 x86。

### 源码

```
src/metaspu/metaspu.cpp          PCSX2Synchronizer
src/metaspu/SndOut.cpp           SndBuffer：环形缓冲、欠载/溢出处理
src/metaspu/Timestretcher.cpp    SoundTouch 集成、tempo 控制
src/metaspu/SoundTouch/          自带 SoundTouch 2.4.1（LGPL 2.1）
```

## 7. 如何选择

| 场景 | 建议 |
| --- | --- |
| 默认使用 / 只想让它正常工作 | **N** |
| 追求最低延迟，且不想引入额外依赖 | **Z** |
| 必须保持音高，可以接受延迟 | **P** |
| 模拟速度接近 100%，且 CPU 性能有限 | **N** 或 **Z** |
| 速度频繁变化（快进、硬件较慢） | **P** |
| 构建中没有 SoundTouch | **N** 或 **Z** |
| 用 N/Z 时音调素材听起来轻微走音 | **P** |

没有绝对"最好"的方式。`N` 和 `Z` 用音高精度换取低延迟和低 CPU 开销；`P` 用延迟和
CPU 开销换取音高精度。模拟器全速运行时，三者的听感差异通常不大。

## 8. 与其他音频设置的关系

- **Audio Synchronization（音频同步）** 选择本文描述的模式/方式。
- **Audio Interpolation（音频插值）**（`None`、`Linear`、`Cosine`、`Cubic` 等）是
  独立的 DS SPU 重采样质量设置，发生在同步之前。
- **声音缓冲区大小 / 延迟**（由各前端提供）改变缓冲量，从而影响同步器需要做多少
  校正；它不负责选择算法。

## 9. 局限性

- 这些方式校正的是**时钟漂移**；如果宿主音频设备本身有问题，或前端完全跟不上，
  它们无法修复。
- `N` 和 `Z` 在校正时不保持音高。
- `P` 无法消除延迟；它会有意多缓存一些音频。
- 具体延迟和伪影表现取决于前端、缓冲区大小以及宿主音频后端。
