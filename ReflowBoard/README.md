# TimmehTimmeh's Arduino Board

This repository contains support for the following TimmehTimmeh Arduino-compatible development boards:

#### AVR Boards

* [Tim's Reflow Board]



Each board will be added as an entry to the Arduino **Tools** > **Board** menu.

![Example image](boards_list.png)

### Installation Instructions

To add board support for our products, go to **File** > **Preferences**, and paste this URL into the 'Additional Boards Manager URLs' input field:

	https://raw.githubusercontent.com/timmehtimmeh/ReflowController/master/ReflowBoard/IDE_Board_Manager/package_TimmehTimmeh_index.json

![Adding a board manager list](Preferences.png)

This field can be found in 'Preferences...' under the Arduino File menu.

Now, under the **Tools** > **Board** > **Boards Manager...**, if you type in "TimmehTimmeh", you will see an option to install board files for the reflow controller board. Click "Install" to add these to your list.

**NOTE: If you are using Arduino 1.6.6 and the link isn't working for you, change "https" at the beginning of the link to "http" and try again. We're working to figure out why this is happening in version 1.6.6.**

![TimmehTimmeh Boards image](TimmehTimmehBoards.PNG)

Now, when you select the Boards list, you will see a collection of new boards for Timmeh.


### Flashing the bootloader

To flash the bootloader, power up the board with either the onboard USB or the 2.1mm jack, and plug the board into a AVRISP MKII via the icsp port

In Atmel Studio 
Set the fuses to the  following values:
* low_fuses = 0xFF
* high_fuses = 0xD8
* extended _fuses = 0xFE

### Notes

* **Please note: This will only work under Arduino IDE versions 1.5 and up.**
* Information on compiling and programming the bootloaders can be found in the bootloaders directory.
