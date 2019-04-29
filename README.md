# MayDel

A useless Switch system module who allow you to light on/off the (hidden) right JoyCon's LED.

Works on firmware higher than 7.x **ONLY**!

Currently it's only support the gamecard state event. The LED blink if you add or remove the gamecard. Maybe more will be added later... 

Blink only works when LED is already power on. As we can't know if the LED is already off or on after a sleep or a reboot. You have to light on it back to get the blink.

# Usage

- Put the release zip content inside `sdmc:/atmosphere/titles/`.
- Push Left Stick and ZL to light on.
- Push Left Stick and ZR to light off.
- Enjoy...

## Credits

- [Libnx and CTCaer for the IPC call and buffer struct](https://github.com/switchbrew/libnx/)
- [DarkMatterCore for gcdumptool Event code](https://github.com/DarkMatterCore/gcdumptool)
- [WerWolv for our own try of the IPC call](https://github.com/WerWolv/)

> Provide with the courtesy of the mob.
