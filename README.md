BluetoothRCLampRGB
==================
This is a bluetooth controlled RGB lamp based on the
Adafruit neo pixel shield for Arduino.

The project falls into two main pieces.

Lamp: Any code that runs on the lamp device itself. 
[Joe Freeman](http://joe.blog.freemansoft.com/2013/08/the-simple-but-awesome-neopixel-shield.html) has a great writeup of the shield itself.
and a nice sample code to get started with. I've converted his code to accept bluetooth controls.

Remote: An implementation of the remote for android devices.

If you don't want to use my remote implementation, or want to port it
to other devices feel free. The lamp code provides a very nice interface
for controlling the lights on the board, and exposes it to bluetooth.

