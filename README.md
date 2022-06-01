# DeSmuME-wasm

WebAssembly port of the DeSmuME. 

Designed for iPhone/iPad, also workable on other devices with a modern browser.

https://ds.44670.org/

Please read this guide before creating a new issue, thanks！

# Frequently Asked Questions
**Q: It can't load any files.**

A: Only iOS >= 14.7 is supported, please update your OS first.

**Q: The performance is very slow.**

A: Please make sure iOS's "Low Power Mode" is disabled.

**Q: There is no sound.**

A: The "Silent Switch" of your device should be "Off".

**Q: How do I save my progress?**

A: After you added the web page to the Home Screen, just save in the game and wait a few seconds, an 'auto-saving' banner will appear.

**Q: Do I need to export the save data?**

A: You don't have to export the save data in normal usecases. However to prevent the data loss caused by the accidental deletion of the Home Screen icon or the damage of your device, you can export the save data to a file. These feature should not be confused with "save states", you have to save in the game before exporting the save data.

**Q: After importing the save data, it takes me back to the main menu.**

A: It is an expected behavior, the save data was imported and you can continue playing by loading the game file again.


# About Save Data
You can save your game progress in the game, and it will be stored in browser's local storage automatically. An "auto saving" banner will appear while the saving is in progress.

On iOS(iPhone/iPad), you have to add the page to your **Home Screen** in order to save your game progress. Your save data will be stored on your device until you remove the icon from Home Screen. 

To reduce the risk of accidental deletion of your Home Screen icon, you can import/export your save data from the in-game menu regularly.

On macOS Safari, the game progress will be deleted after **7 days of inactivity** due to software limitation. This behavior of Safari cannot be changed, so it is strongly recommended to use other browsers like Chrome on macOS.

For other platforms, Chrome/Edge may be a good choice.

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

# Microphone

Press 'R' will send a waveform to microphone. It *may* be useful for playing some games.
