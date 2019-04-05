# Endian Zephyr RTOS Hackathon @ FOSS North 2019

Hi and welcome to the FOSS North 2019 Zephyr hackathon hosted by Endian
Technologies AB!

We're really excited about Zephyr and hope you are too. Nordic Semiconductor has
graciously sent us a box full of nRF52840-DK:s to play around with. You'll get
to take yours home.

## Sample app

This repo contains a simple sample application built against `v1.14.0-rc3` of
Zephyr. Its intention is to quickly get participants up and running with a
working application.

Implemented is a GATT service with two characteristics: one that reads the
battery voltage, and one that controls the four LEDs on the nRF52840 DK.

To try it out, edit `prj.conf` and give your device a suitable alias by changing
the line

    CONFIG_BT_DEVICE_NAME="zephyr-example-device"

appropriately. Then, build and flash the sample to your nRF52840 DK
(instructions in next section). To enable reading off the battery voltage,
connect the ADC pin `P0.05` to the voltage pin `VDD`.

Next, install the `nRF Connect` app on your phone and connect to your device.

You'll notice an `Unknown Service` with a long UUID containing one READ and one
WRITE characteristic. To turn on LED #1, write `0x0001`. To turn on LED #2,
write `0x0101`, etc.

Alternatively, if you have Chrome and want to try out *WebBluetooth*, open
`gatt-sample.html` in this directory, or surf to

    https://gatt-sample.endian.se

Note: You may need to enable "experimental features" first, by surfing to

    chrome://flags/#enable-experimental-web-platform-features

## Building and flashing Zephyr

*Note:* Nowadays, upstream encourages using `west`, the brand new Zephyr
meta-tool for fetching, building AND flashing Zephyr. It slices, it dices, it
supposedly even acts as a substitute for Git Submodules/Google Repo. Supposedly.

This means that the process documented here isn't "upstream idiomatic", so to
speak. Feel free to try out West instead if you feel like it.

### Building using Docker

The easiest way to build is to copy the Dockerfile and build script in this repo
to the Zephyr source root directory and build everything containerized. Note
that we've only tested this on Linux and Mac.

    ZEPHYR_BASE=/path/to/zephyr  # change this
    SAMPLE_BASE=$PWD             # and possibly this
    cp {Dockerfile,build.sh} $ZEPHYR_BASE
    cd $ZEPHYR_BASE
    docker build . --tag=zephyr-builder
    docker run -it \
        -v $ZEPHYR_BASE:/zephyr                 \
        -v $SAMPLE_BASE:/hackathon-sample       \
        zephyr-builder

and inside the Docker container, run

    /zephyr/./build.sh /hackathon-sample

Note that this script will work for any sample, and will also work if you're
building natively. Quite convenient!

### Building without Docker

See https://docs.zephyrproject.org/latest/getting_started/getting_started.html
and raise your hand if/when you run into problems :)

### Flashing

Note that containerized flashing is a lot more annoying than containerized
building because you need access to USB. Full instructions can be found at

    https://docs.zephyrproject.org/latest/tools/nordic_segger.html

Get the Segger J-Link software and the Nordic tools, located at

    https://www.segger.com/jlink-software.html
    https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF5-Command-Line-Tools

*Note:* Depending on your Linux version, you may need a few packages backported.
On Ubuntu, at time of writing, `cmake` and `dtc` needs newer versions. See

    https://askubuntu.com/questions/1090223/how-to-upgrade-dtc-version-in-ubuntu-18-04
    https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line/865294

for instructions.

Once you've set up the Segger tools and `nrfjprog` is in your `$PATH`, you're
almost good to go. The Segger tools should have added some rules to
`/etc/udev/rules.d/` which will enable you to flash your device without sudo. To
enable these new rules, do

    udevadm control --reload-rules && udevadm trigger

and you should be ready to flash:

    nrfjprog --eraseall && nrfjprog --program /path/to/zephyr.hex

### Debugging by printing to console

You can absolutely hook up a UART cable to the device and debug using `screen`,
`minicom` or whatever. If so, the following snippet from the device's
device tree may come in handy:

    &uart0 {
            compatible = "nordic,nrf-uart";
            current-speed = <115200>;
            status = "ok";
            tx-pin = <6>;
            rx-pin = <8>;
            rts-pin = <5>;
            cts-pin = <7>;
    };

But there's a very convenient method which lets you get away with just a micro
USB cable. One of the Segger tools installed previously is `JLinkRTTLogger`,
which uses the J-Link as a console. Make sure your `prj.conf` contains

    CONFIG_LOG=y
    CONFIG_CONSOLE=y
    CONFIG_USE_SEGGER_RTT=y
    CONFIG_RTT_CONSOLE=y

*Note:* If you modify `prj.conf`, the `.dts` file or certain headers not listed
as dependencies, you need to remove the contents of `build/` and re-run `cmake`.
This is something that will frustrate you a lot since everyone forgets to do
this from time to time.

Once you've compiled and flashed your application, execute

    JLinkRTTLogger -device NRF52 -if SWD -speed 5000 -rttchannel 0 /tmp/jlink.cap

in a terminal and

    tail -f /tmp/jlink.cap

in another terminal. You can also log directly to `stdout` by specifying
`/dev/stdout` when you call the logger, but we like logging to a file because
you can open it in emacs, grep it, send it to a buddy, etc.

### Debugging with GDB

On Debian/Ubuntu, you'll need to install `gdb-multiarch`. Then, run

    JLinkGDBServer -select USB -device nrf52 -if swd -speed 1000 -port 2331

to start the J-Link GDB server. To start the actual client, cd to your project
directory and run

    gdb-multiarch build/zephyr/zephyr.elf

and note that `gdb` expects the `.elf` file and not the `.hex` file. The `.hex`
file doesn't contain the required symbol tables.

Inside gdb, do

    (gdb) target remote localhost:2331
    (gdb) b main
    (gdb) monitor reset
    (gdb) continue

to connect to the target, attach a breakpoint to the main function, reset the
device and start running until the breakpoint hits. A nice productivity hack is
to place these commands inside a `.gdbinit` file placed inside your project
directory. To get `gdb` to auto-source this file, `~/.gdbinit` should contain
the following line:

    set auto-load local-gdbinit

but sourcing the file from inside GDB also works. We've placed a sample
`.gdbinit` inside this repository for your convenience.


## General development tips

If you're familiar with embedded Linux development, you have almost surely
worked with device tree (`dts`) files. On Zephyr, they're syntactically
identical, but aren't compiled to a binary data structure as in Linux. Instead
the `dts` is parsed and used to generate a bunch of pre-processor `#define`s
that you can access from your code.

This may seem ugly, because it is ugly. But it is way more efficient because the
kernel doesn't have to include a run-time parser -- the hardware is all decided
upon compile-time.

The ugliness is something that we have to deal with. Debugging is GREATLY aided
by knowing where the header files generated are placed. Check out

        build/zephy/include/generated

and especially `autoconf.h` and `generated_dts_board*.h`. The device tree for
the nRF52840 DK is located at

    $ZEPHYR_BASE/boards/arm/nrf52840_pca10056/nrf52840_pca10056.dts

You will consult this file frequently, but DO NOT edit it. If you want to alter
the hardware description, by for example changing pins or baudrate on the UART,
create a device tree overlay file and set the CMake variable `DTS_OVERLAY_FILE`
appropriately (multiple, space-separated, overlay files are fine).

You'll also want to keep `$ZEPHYR_BASE/include/kernel.h` handy at all times, as
it contains definitions and documentation for most of the basic data structures
and API:s you'll need (semaphores, mutexes, message queues, work queues, timers,
threads, etc).
