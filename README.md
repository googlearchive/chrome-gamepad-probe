chrome-gamepad-probe
====================

This is a tool for users to report gamepad button and axis mappings for DirectInput on Windows.
Today, Chrome only supports DirectInput controllers it can identify. This is because buttons and axes are mapped
differently on different controllers, but the Gamepad API expects consistent button and axis numbers.

Ports to Linux and OS X are currently in development.

Quick start
-----------
Download gamepad_probe.exe from the win/bin directory. Then run the program, follow the directions, and open a bug
at crbug.com, pasting the program output inline.
