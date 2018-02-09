# TimmehTimmeh's Arduino Board

This repository contains support for the following TimmehTimmeh Arduino-compatible reflow oven controller.  The reflow controller runs on an Atmega32u4 at 3.3V/8MHz.

# Attributions
This project is based on TMK's headless reflow oven (https://github.com/tmk/HeadlessReflowOven) which is in turn based on  work of 

* [ohararp ReflowOven]  (https://github.com/ohararp/ReflowOven)

* which is modification of [rocketscream Reflow-Oven-Controller]  (https://github.com/rocketscream/Reflow-Oven-Controller)

rocketscream released the work under Creative Commons Share Alike v3.0 license.

The bootloader and corresponding installation instructions are a modification of Sparkfun's bootloader code for their Arduino Pro Micro: (https://github.com/sparkfun/Arduino_Boards)

The directory structure is as follows:
* [ReflowBoard]	- The firmware, bootloader, and arduino IDE files that communicate with the board
* [ReflowOven]	- The software (arduino software project) that runs on the board
* [Schematics] - schematics and eagle files for the board
* [libraries] - Library files that the arduino project links against
