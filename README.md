# DeSmuME-wasm

WebAssembly port of the DeSmuME. 

Designed for iPhone/iPad, also workable on other devices with a modern browser.

https://ds.44670.org/

Please read this guide before creating a new issue, thanks!

# Frequently Asked Questions

**Q: The performance is too slow/laggy.**

A: Please make sure iOS's system-wide "Low Power Mode" is disabled. The battery icon is yellow if it is enabled.

**Q: How do I save my progress?**

A: Just save in the game and wait a few seconds, an auto-saving banner will appear, your save data will be stored in the web app's local storage automatically. On iOS you have to add the site to Home Screen first, and your data will be deleted when the Home Screen icon is removed.

**Q: Do I have to export the save data?**

A: You DON't have to export save data manually since auto-saving is present. To prevent the data loss caused by accidential deletion of the Home Screen icon or damaged device, you may export the save data to a safe place manually.

**Q: After importing the save data, it takes me back to the main menu.**

A: It is an expected behavior, the save data was imported and you can continue playing by loading the game file again.

**Q: It can't load any files.**

A: Only iOS >= 14.7 is supported, please update your OS first.

**Q: There is no sound.**

A: The "Silent Switch" of your device should be "Off". If sound is still not working, please try to restart the app/reboot the device.

**Q: How to blow on the microphone?**

A: Pressing 'R' button will emulate a blow on microphone. It *may* be useful for playing some games.


# Performance

Most 2D games could be run at 60fps on A14-based devices. 

However, the performance of 3D games varies for each game, an A15-based device could run most 3D games at nearly full speed.

By default, power-saving mode is enabled, which will limit the frame rate to 30fps. It is strongly recommended to enable this mode on A14-based devices to protect battery life and keep the temperature comfortable for playing. 

On A15-based devices, this mode could be disabled if you want to have a smoother experience.

# Control

Gamepads are supported if your OS supports it. Please note that iOS does not support most kind of gamepads, DualShock 4 is an officially supported one.

You may also want to control the game with a keyboard:
```
z: A
x: B
a: Y
s: X
q: L
w: R
enter: Start
space: Select
```
