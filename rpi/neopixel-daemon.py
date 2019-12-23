#!/usr/bin/python3

import json
import serial
import time

SERIAL_PORT = '/dev/cu.usbmodem14201'
SERIAL_BAUDRATE = 115200
SERIAL_WAIT = 0.2

ANIMATIONS = {
    "OFF": 0,
    "COLOR": 1,
    "RAINBOW": 2,
    "TWINKLE": 3
}

class Color():
    def __init__(self, red, green, blue, white = 0):
        self.red = red
        self.green = green
        self.blue = blue
        self.white = white

    def __int__(self):
        return self.white << 24 | self.red << 16 | self.green << 8 | self.blue

    def __str__(self):
        return "#%08x" %int(self)

class AnimationConfig():
    def __init__(self, animation, color1 = Color(0, 0, 0), color2 = Color(0, 0, 0), speed = 16, width = 16):
        self.animation = animation
        self.color1 = color1
        self.color2 = color2
        self.speed = speed
        self.width = width

    def toJson(self):
        return {
            "anim": ANIMATIONS[self.animation],
            "col1": int(self.color1),
            "col2": int(self.color2),
            "speed": self.speed,
            "width": self.width
        }
    

class NeopixelDaemon():

    def __init__(self, serialPort, serialBaud):
        self.serialPort = serialPort
        self.serialBaud = serialBaud
        self._initSerial()

    def _initSerial(self):
        self.serial = serial.Serial(self.serialPort, self.serialBaud, timeout=0.1)
        init = False
        while not init:
            if self.serial.readline():
                init = True

    def applyConfig(self, config):
        configStr = json.dumps(list(map(lambda anim: anim.toJson(), config)))
        self._sendConfig(configStr)

    def _sendConfig(self, config):
        self.serial.write(b' ')
        time.sleep(SERIAL_WAIT)
        self.serial.write(config.encode('utf-8'))
        print(self.serial.read(128))

if __name__ == '__main__':
    animation = AnimationConfig(
        "TWINKLE",
        Color(255, 16, 0),
        Color(255, 255, 128),
        1,
        16)
    daemon = NeopixelDaemon(SERIAL_PORT, SERIAL_BAUDRATE)
    daemon.applyConfig([animation, animation, animation])