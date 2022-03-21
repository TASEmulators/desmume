# DeSmuME-wasm

WebAssembly port of the DeSmuME. 

Designed for iPhone/iPad, also workable on other devices with a modern browser.

https://ds.44670.org/

# Please Read This Guide Before Creating a New Issue, Thank You!

# Please, Do Not Create a Duplicated Issue for Feature Request

# How to save your game progress

You can save your game progress in the game, and it will be stored in browser's local storage automatically. An "auto saving" banner will appear while the saving is in progress.

On iOS(iPhone/iPad), you have to add the page to your **Home Screen** in order to save your game progress. Your save data will be stored on your device until you remove the icon from Home Screen. 

To reduce the risk of accidental deletion of your Home Screen icon, you can import/export your save data from the in-game menu regularly.

On macOS Safari, the game progress will be deleted after **7 days of inactivity** due to the browser's limitation. This behavior of Safari cannot be changed, so it is strongly recommended to use other browsers like Chrome on macOS.

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
