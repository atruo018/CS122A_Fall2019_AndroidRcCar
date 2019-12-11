import serial
import time
import RPi.GPIO as GPIO
from bluedot import BlueDot

GPIO.setmode(GPIO.BCM)
GPIO.setup(17, GPIO.OUT)
GPIO.setup(27, GPIO.OUT)

GPIO.output(17, GPIO.LOW)
GPIO.output(27, GPIO.LOW)

def move(pos):
    data = int(pos.distance * 255)
    if pos.top:
        GPIO.output(17, GPIO.LOW)
        GPIO.output(27, GPIO.LOW)
        print ("Moving Forward (%d)" %data)
    elif pos.bottom:
        GPIO.output(17, GPIO.HIGH)
        GPIO.output(27, GPIO.LOW)
        print ("Moving Backwards (%d)" %data)
    elif pos.left:
        GPIO.output(17, GPIO.LOW)
        GPIO.output(27, GPIO.HIGH)
        print ("Turning Left (%d)" %data)
    elif pos.right:
        GPIO.output(17, GPIO.HIGH)
        GPIO.output(27, GPIO.HIGH)
        print ("Turning Right (%d)" %data)
    ser.flush()
    ser.write(bytes([data]))

def stop(pos):
    ser.flush()
    ser.write(bytes([0]))
    print ("Stopping")

ser = serial.Serial(
        port = '/dev/ttyS0', 
        baudrate = 9600,
        parity = serial.PARITY_NONE,
        bytesize = serial.EIGHTBITS
        )

print ("Initializing UART..")

time.sleep(.2)

bd = BlueDot()
ser.write(bytes([0]))
try:
    while True:
        bd.when_pressed = move
        bd.when_moved = move
        bd.when_released = stop
except KeyboardInterrupt:
    ser.close()
    GPIO.cleanup()
    print("exiting")

