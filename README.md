# RTOS Monitor on STM32H743I-EVAL

A live system-monitor dashboard for the **STM32H743I-EVAL** board, built on **FreeRTOS**. Seven tasks run at different priorities and share data through a queue, a mutex and a semaphore. The 640x480 LCD shows what the RTOS is doing in real time: uptime, CPU load, every task's state and stack use, heap use, inputs, an ADC reading, the LEDs and a system log.

## Features

- **CPU load** measured from FreeRTOS run-time statistics, using the Cortex-M7 DWT cycle counter as the timer (100 % minus the time spent in the IDLE task, averaged over 500 ms)
- **Task table** with each task's priority, state (RUN / RDY / BLK / SUS) and stack usage as a percentage, from the stack high-water mark
- **Heap monitor**: free, used and minimum-ever-free bytes of the 32 KB FreeRTOS heap
- **Uptime and tick counter** (1 kHz tick)
- **Joystick**: live direction pad, press count and last event
- **TAMPER button** (interrupt) and **WAKEUP button** (polled and debounced), each with a press counter
- **ADC**: the on-board potentiometer (PA0_C, ADC1 channel 0) shown as volts, raw value and a bar
- **LED mirror**: shows the state of the green, orange and blue LEDs
- **System log**: the last 4 events with timestamps (a ring buffer of 8)
- **Self-tests** that log a message if the DWT counter is not running or run-time statistics are switched off
- Flicker-free drawing: labels and frames are drawn once, and only changing values are redrawn

## Hardware

| Item | Details |
|---|---|
| Board | STM32H743I-EVAL (STM32H743XIH6 MCU) |
| Display | On-board LCD, 640x480, landscape |
| Inputs | Joystick, TAMPER button, WAKEUP button, potentiometer RV1 |
| Outputs | LED1 (green), LED2 (orange), LED3 (red), LED4 (blue) |
| Debugger | On-board ST-LINK (USB) |

## Software requirements

- STM32CubeIDE
- FreeRTOS kernel files (included in `STM32CubeIDE/FreeRTOS`: `tasks.c`, `queue.c`, `list.c`, `port.c`, `heap_4.c`)
- STM32H7 HAL and BSP drivers (included in the `Drivers` folder)

## Getting started

1. Clone or download this repository.
2. Open STM32CubeIDE and choose **File -> Import -> General -> Existing Projects into Workspace**.
3. Select the `RTOSMonitor_Multitask/STM32CubeIDE` folder and click **Finish**.
4. Build the project (hammer icon). You want 0 errors.
5. Connect the board to your PC with the ST-LINK USB cable.
6. Click **Debug** (bug icon), then **Resume (F8)**. A debug configuration, `RTOSMonitor_Multitask.launch`, is included.

## What you will see

The dashboard screen has these areas:

| Area | Shows |
|---|---|
| Header | "RTOS MONITOR" title, CPU clock in MHz, RUNNING badge |
| Top row | Uptime, tick count, CPU load with a coloured bar (green, amber above 70 %, red above 90 %) |
| Tasks card | One row per task: name, priority, state, stack-use bar |
| Heap card | Free bytes, used bar, minimum free, used bytes |
| Workload card | Count of workload cycles completed |
| Input card | Joystick pad, joystick value, press count, last event, TAMPER and WAKEUP counts |
| ADC card | Potentiometer voltage, bar and raw value |
| LEDs card | Heartbeat, workload and joystick LEDs |
| System log | Latest events with `[hh:mm:ss]` timestamps |

Try it: move the joystick, press TAMPER and WAKEUP, and turn the potentiometer. Each shows up on screen straight away.

## The tasks

FreeRTOS runs the highest-priority ready task first (a bigger number means higher priority).

| Task | Priority | Stack (words) | Job |
|---|---|---|---|
| Input | 4 | 384 | Polls the joystick every 20 ms and sends changes to a queue. Debounces the WAKEUP button. Reads the ADC every 100 ms. |
| Status | 3 | 384 | Draws a small "Status tick" counter every 50 ms. This proves the display is shared safely. |
| Button | 3 | 256 | Sleeps until the TAMPER interrupt gives a semaphore, then counts and logs the press. |
| Display | 2 | 640 | Draws the dashboard every 200 ms (`vTaskDelayUntil`) and logs joystick events from the queue. |
| Memory | 2 | 384 | Every 500 ms, reads heap figures and works out CPU load. Logs a warning above 80 % and a recovery below 60 %. |
| Work | 1 | 256 | A deliberate busy loop (200,000 additions) to give the CPU something to do, then sleeps 20 ms. |
| LedMon | 1 | 256 | Blinks the green LED, toggles orange per workload cycle, and lights blue while the joystick is pressed. |

### How the tasks talk to each other

| Mechanism | Used for |
|---|---|
| Queue (`joyQueue`, 8 items) | Input task -> Display task: joystick events |
| Binary semaphore (`tamperSem`) | TAMPER interrupt -> Button task: wake up on a press |
| Mutex (`lcdMutex`) | Display and Status tasks take turns using the LCD |
| Shared struct (`gStats`) | Tasks publish values (CPU load, heap, counters) that the Display task reads |

The TAMPER interrupt gives the semaphore with `xSemaphoreGiveFromISR()` and then calls `portYIELD_FROM_ISR()`, so the Button task runs immediately.

## Project structure

```
RTOSMonitor_Multitask/
├── Inc/                         # HAL and BSP configuration headers, main.h
├── Src/
│   ├── main.c                   # Clock, MPU, cache, LED, LCD and joystick setup; starts the scheduler
│   ├── stm32h7xx_it.c           # SysTick (feeds HAL and FreeRTOS) and EXTI handlers
│   └── system_stm32h7xx.c       # System init
└── STM32CubeIDE/
    ├── Example/User/
    │   ├── app_tasks.c          # The 7 tasks, queue, mutex, semaphore, ADC, run-time timer
    │   └── dashboard.c          # All LCD drawing and the system log
    ├── Inc/
    │   ├── FreeRTOSConfig.h     # FreeRTOS settings
    │   ├── app_tasks.h          # Task IDs, stack sizes, shared stats struct
    │   └── dashboard.h
    ├── FreeRTOS/                # Kernel source (tasks, queue, list, port, heap_4)
    ├── STM32H743XIHX_FLASH.ld   # Linker script
    └── RTOSMonitor_Multitask.launch   # Debug configuration
```

`Src/game.c` is left over from an earlier game project and is not called by this application.

## Key FreeRTOS settings (`FreeRTOSConfig.h`)

| Setting | Value |
|---|---|
| Scheduler | Preemptive |
| Tick rate | 1000 Hz |
| Heap | 32 KB (`heap_4`, dynamic allocation) |
| Priorities | 7 levels |
| Run-time statistics | On, using the DWT cycle counter |
| Trace facility | On (needed for `uxTaskGetSystemState`) |
| Max interrupt priority that may call FreeRTOS | 5 |
| Software timers | Off |

## How it works

1. `main()` sets up the MPU, caches, HAL and a 400 MHz system clock.
2. It initialises the LEDs, LCD and joystick.
3. `App_CreateTasks()` creates the queue, mutex, semaphore and 7 tasks, and initialises the ADC and the buttons.
4. `vTaskStartScheduler()` hands control to FreeRTOS. From here on, the tasks run and `main()` never continues.
5. `SysTick_Handler` calls both `HAL_IncTick()` and `xPortSysTickHandler()`, so the HAL delay functions and the RTOS tick share one timer.

**CPU load:** FreeRTOS counts how long each task runs, using the DWT cycle counter as its clock. The Memory task adds up all task times, finds the IDLE task's share over the last 500 ms, and reports `100 % - idle share`.

**Stack use:** `uxTaskGetStackHighWaterMark()` returns the smallest free stack the task has ever had, so the bar shows the worst-case usage so far. If a bar turns amber (over 80 %), increase that task's stack size in `app_tasks.h`.

## Troubleshooting

| Problem | Fix |
|---|---|
| Log shows "Run-time stats OFF" | Check `configGENERATE_RUN_TIME_STATS` in `FreeRTOSConfig.h`. |
| Log shows "DWT cycle counter NOT counting" | The debugger or core is blocking the DWT unit. Power-cycle the board and start a new debug session. |
| ADC shows `n/a` | ADC initialisation failed. The log shows "ADC init failed". Check the ADC clock settings in `Adc_Init()`. |
| Screen stays black | Check that `BSP_LCD_Init()` succeeds and that the heap is large enough (32 KB). |
| Board freezes in `configASSERT` | A FreeRTOS check failed. Common causes: wrong interrupt priority, or a stack that is too small. |
| `undefined reference to ...` | Make sure `app_tasks.c`, `dashboard.c` and the FreeRTOS source files are in the build and not excluded. |


## Author

Deniston
