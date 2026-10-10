# STM32 Servo Manager

Bare-metal servo motor controller for the **STM32F103C8T6** microcontroller, implemented in C using CMSIS register-level access.

The firmware controls a servo motor via PWM and accepts angle commands over UART. The project is designed for STM32 Blue Pill hardware and can also be used with a compatible Proteus simulation setup.

## Features

- Register-level peripheral configuration using CMSIS.
- No STM32 HAL, LL, or Standard Peripheral Library.
- PWM generation using TIM2 Channel 1.
- UART command interface using USART2.
- Command validation and error reporting.
- Support for servo angles from `0°` to `180°`.
- Fixed-length receive buffer with overflow detection.
- CMake-based build system.

## Hardware

| Component | Configuration |
|---|---|
| Microcontroller | STM32F103C8T6 |
| Core | ARM Cortex-M3 |
| System clock | 64 MHz |
| PWM output | PA0 / TIM2_CH1 |
| UART TX | PA2 / USART2_TX |
| UART RX | PA3 / USART2_RX |
| UART configuration | 9600 baud, 8 data bits, no parity, 1 stop bit |
| PWM frequency | 50 Hz |
| PWM pulse width | 1000–2000 µs |

## PWM Configuration

The servo control signal is generated using TIM2 Channel 1.

- Timer clock: 64 MHz
- Prescaler: 63
- Timer counter frequency: 1 MHz
- Auto-reload value: 19999
- PWM period: 20 ms (50 Hz)

The pulse width is mapped linearly to the requested angle.

| Angle | Pulse width |
|---:|---:|
| 0° | 1000 µs |
| 90° | 1500 µs |
| 180° | 2000 µs |

These values are nominal control parameters. The actual mechanical angle depends on the servo model and its calibration.

## UART Command Protocol

Commands are transmitted over USART2 using the following format:

```text
SETVO ddd\r\n
```

Where:
- `SETVO` is the command identifier.
- A single space separates the command from its argument.
- `ddd` is a three-digit decimal angle in the range `000`–`180`.
- `\r\n` represents the CRLF line terminator (`0x0D 0x0A`).

### Examples

Set the servo to 90 degrees:

```text
SETVO 090
```

Set the servo to 0 degrees:

```text
SETVO 000
```

Set the servo to 180 degrees:

```text
SETVO 180
```

### Responses

Successful commands return a confirmation:

```text
OK 090
```

Invalid commands return an error message. The firmware validates command syntax, angle range, and receive-buffer length.

## Project Structure

```text
servo_manager/
├── Sources/
│   ├── main.c
│   ├── syscalls.c
│   ├── sysmem.c
│   └── system_stm32f1xx.c
├── Startup/
│   └── startup_stm32f103c8tx.s
├── CMakeLists.txt
├── CMakePresets.json
├── cubeide-gcc.cmake
└── STM32F103C8TX_FLASH.ld
```

The project uses CMSIS device headers for STM32F1 and a custom startup and linker configuration.

## Requirements

- CMake 3.20 or newer.
- GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`).
- CMSIS Core headers.
- STM32F1 CMSIS device headers.
- STM32F103C8T6-compatible startup and linker files.

The CMSIS headers are expected under the following STM32CubeF1 directory structure:

```text
STM32CubeF1/
└── Drivers/
    └── CMSIS/
        ├── Include/
        └── Device/
            └── ST/
                └── STM32F1xx/
                    └── Include/
```

## Build

Configure the project using the provided CMake toolchain file:

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=cubeide-gcc.cmake
```

Build the firmware:

```bash
cmake --build build
```

The exact output location depends on the selected CMake configuration and build presets.

## Connecting the Servo

| Servo signal | Connection |
|---|---|
| PWM signal | PA0 / TIM2_CH1 |
| Power | According to the servo's specifications |
| Ground | Common ground with the microcontroller |

Use an appropriate external power supply for the servo when required. Ensure that the servo signal input is compatible with the STM32's 3.3 V GPIO logic.

## Design Notes

The firmware configures the system clock, GPIO, USART2, and TIM2 directly through peripheral registers. UART input is processed using polling, and the command parser validates incoming commands before updating the PWM compare register.

The implementation is intended for educational purposes and as a foundation for further bare-metal embedded development.

## License

No license has been specified yet. Add a `LICENSE` file before distributing or reusing the project publicly. If you want others to be able to use, modify, and redistribute the code, consider an open-source license such as MIT.

