<!--
Template source: awesome-readme-template
This README has been adapted for an embedded STM32 firmware repo.
-->

<div align="center">

  <img src="assets/logo.png" alt="logo" width="200" height="auto" />
  <h1>373FP (Glove + Board firmware)</h1>
  
  <p>
    Two STM32CubeIDE projects for a glove-based input device and a companion board.
  </p>

<!-- Badges -->
<p>
  <!-- TODO: replace with your repo badges (or delete this block) -->
  <a href="">
    <img src="https://img.shields.io/badge/MCU-STM32L432KCUx-blue" alt="mcu" />
  </a>
  <a href="">
    <img src="https://img.shields.io/badge/IDE-STM32CubeIDE-lightgrey" alt="ide" />
  </a>
  <a href="">
    <img src="https://img.shields.io/badge/Board-NUCLEO--L432KC-lightgrey" alt="board" />
  </a>
</p>
   
<h4>
    <a href="#getting-started">Getting started</a>
  <span> · </span>
    <a href="#usage">Usage</a>
  <span> · </span>
    <a href="#roadmap">Roadmap</a>
  </h4>
</div>

<br />

<!-- Table of Contents -->
# Table of Contents

- [About the Project](#about-the-project)
  * [Tech Stack](#tech-stack)
  * [Features](#features)
  * [Hardware](#hardware)
  * [Data formats](#data-formats)
- [Getting Started](#getting-started)
  * [Prerequisites](#prerequisites)
  * [Installation](#installation)
  * [Run Locally](#run-locally)
- [Usage](#usage)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
  * [Code of Conduct](#code-of-conduct)
- [License](#license)
- [Contact](#contact)
- [Acknowledgements](#acknowledgements)
  

<!-- About the Project -->
## About the Project

<div align="center"> 
  <img src="https://placehold.co/600x400?text=Project+photo+placeholder" alt="screenshot" />
</div>

This repo contains two STM32CubeIDE projects:

- `Glove/`: reads 4 analog inputs (flex/tap sensors) via ADC+DMA, reads an I2C accelerometer for hand orientation, builds a small command packet, and transmits it over UART.
- `Board/`: reads 4 analog inputs via ADC+DMA, threshold-converts them to 0/1, and streams the 4-byte sample out over the ST-Link Virtual COM Port (UART).

> The current `main.c` implementations already define packet formats and UARTs used; see [Data formats](#data-formats).


<!-- TechStack -->
### Tech Stack

<details>
  <summary>Firmware</summary>
  <ul>
    <li><a href="https://www.st.com/en/development-tools/stm32cubeide.html">STM32CubeIDE</a> (CubeMX-generated HAL projects)</li>
    <li>STM32 HAL + CMSIS</li>
    <li>C (GCC toolchain)</li>
  </ul>
</details>

<details>
  <summary>Peripherals used</summary>
  <ul>
    <li>ADC1 + DMA (circular sampling)</li>
    <li>USART1 + DMA (command TX)</li>
    <li>USART2 (ST-Link VCP debug/streaming)</li>
    <li>I2C1 (accelerometer)</li>
  </ul>
</details>

<!-- Features -->
### Features

- **Glove**: ADC+DMA averaging + baseline calibration for 4 analog channels.
- **Glove**: I2C accelerometer readout → coarse hand pose (fingers up/down/horizontal, palm/back/edge up).
- **Glove**: roll around X-axis mapped to a 1..10 value.
- **Glove**: 7-byte command packet sent over `USART1` using DMA.
- **Board**: ADC+DMA continuous scan of 4 channels → threshold to 4 single-byte bits → streamed over `USART2` (ST-Link VCP).

<!-- Hardware -->
### Hardware

- **MCU**: STM32L432KCUx (per both `.ioc` files)
- **Dev board**: NUCLEO-L432KC

#### ADC pins (both projects)

| ADC channel | MCU pin |
|---|---|
| ADC1_IN5 | `PA0` |
| ADC1_IN6 | `PA1` |
| ADC1_IN9 | `PA4` |
| ADC1_IN12 | `PA7` |

#### UART pins (both projects)

- **USART1** (115200 8N1)
  - TX `PA9`
  - RX `PA10`
- **USART2** (115200 8N1, ST-Link VCP on NUCLEO)
  - TX `PA2`
  - RX `PA15`

#### I2C pins (Glove project)

- **I2C1**
  - SCL `PB6`
  - SDA `PB7`

<!-- Data formats -->
### Data formats

#### Glove → UART command packet (7 bytes)

Sent from `Glove` over `USART1` (DMA TX), at ~2 Hz (500 ms loop delay):

| Byte | Name | Meaning |
|---:|---|---|
| 0 | `mode` | Mode index \(0..3\), advanced via “tap” threshold on ADC channel 0 |
| 1 | `flex1` | 1 if \(\Delta\) ADC ch1 exceeds threshold, else 0 |
| 2 | `flex2` | 1 if \(\Delta\) ADC ch2 exceeds threshold, else 0 |
| 3 | `flex3` | 1 if \(\Delta\) ADC ch3 exceeds threshold, else 0 |
| 4 | `fingers_up` | 1 if accelerometer-derived pose says fingers up, else 0 |
| 5 | `palm_up` | 1 if accelerometer-derived pose says palm up, else 0 |
| 6 | `roll_x_1to10` | roll around X axis mapped to integer 1..10 |

Notes:

- The Glove code uses accelerometer addresses `0x32` (write) / `0x33` (read) as provided in the firmware.
- The Glove also prints human-readable debug lines over `USART2` (ST-Link VCP).

#### Board → UART bitstream (4 bytes)

The `Board` project continuously samples 4 ADC channels via DMA and converts each to a single byte (0/1) using a fixed threshold:

- **Threshold**: `1700` (12-bit ADC raw)
- **Payload**: 4 bytes: `[b0, b1, b2, b3]` where `bi ∈ {0,1}`
- **Output**: transmitted over `USART2` (ST-Link VCP) when ADC DMA half/full callbacks indicate new data is ready.


<!-- Getting Started -->
## Getting Started

<!-- Prerequisites -->
### Prerequisites

- STM32CubeIDE (recommended)
- A NUCLEO-L432KC (or compatible STM32L432KCUx target) for each project you want to run
- USB cable(s) for ST-Link programming and Virtual COM Port

<!-- Installation -->
### Installation

Clone this repo (or download as ZIP) and open the project(s) in STM32CubeIDE.

<!-- Run Locally -->
### Run Locally

In STM32CubeIDE:

- Import `Glove/` as an existing STM32CubeIDE project.
- Import `Board/` as an existing STM32CubeIDE project.
- Build and flash each project to its target board.

If you modify peripherals/pins, prefer doing it via the `.ioc` file and re-generating code (keeping user-code blocks).


<!-- Usage -->
## Usage

### Glove project (`Glove/`)

- Flash `Glove` to a NUCLEO-L432KC.
- Open a serial monitor on the ST-Link VCP port at `115200 8N1` to view debug output.
- The firmware:
  - starts ADC+DMA sampling of 4 channels and captures a baseline after ~500 ms
  - reads an I2C accelerometer
  - sends a 7-byte command packet over `USART1`

### Board project (`Board/`)

- Flash `Board` to a NUCLEO-L432KC.
- Open a serial monitor on the ST-Link VCP port at `115200 8N1`.
- You should see a repeating stream of 4 raw bytes, each either `0x00` or `0x01`, representing the 4 ADC channels after thresholding.

### Wiring (if you are linking Glove → another device over USART1)

- Connect `Glove PA9 (USART1_TX)` → receiver `USART1_RX`
- Connect grounds together
- Ensure both sides use **115200 8N1**

> The current `Board` firmware does not yet parse the 7-byte Glove packet; it focuses on ADC→VCP streaming.


<!-- Roadmap -->
## Roadmap

* [x] Glove: 7-byte command packet over `USART1` DMA TX
* [x] Glove: basic accelerometer-derived pose + roll value
* [x] Board: ADC threshold streaming over ST-Link VCP
* [ ] Board: implement `USART1` RX parser for Glove command packet
* [ ] Document sensor wiring / BOM and add real photos to `assets/`

<!-- Contributing -->
## Contributing

Contributions are always welcome!

If you plan to change pinouts/peripherals, please update the `.ioc` file(s) and mention the changed wiring in the PR/commit description.

<!-- Code of Conduct -->
### Code of Conduct

Please be respectful and constructive in issues and PRs.

<!-- License -->
## License

TBD. (Add a `LICENSE` file and update this section.)

<!-- Contact -->
## Contact

TBD. (Add your name + preferred contact and update this section.)

<!-- Acknowledgements -->
## Acknowledgements

 - [Shields.io](https://shields.io/)
 - [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html)
