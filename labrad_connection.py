import labrad
import serial
import time
import sys
import signal

cxn = labrad.connect()
mag = cxn.mercury_ips_server
mag.select_device()


signal.signal(signal.SIGINT, signal.default_int_handler)
led = serial.Serial('COM3', 9600, timeout=0.2)
led.flush()
time.sleep(2) 


state = mag.get_ramp_status()
isramping = True
if state == 'HOLD':
    isramping = False


if isramping == 1:
    led.write(b"m1\n")
else:
    led.write(b"m0\n")


led.write(b'c0,2,1\n')

try:
    while True:
        raw_field_str = str(mag.get_field())[0:5]
        payload = (raw_field_str + "\n").encode()
        led.write(payload)

        current_state = True
        if state == 'HOLD':
            current_state = False


        if isramping != current_state:
            isramping = current_state
            if isramping == 1:
                led.write(b"m1\n")
            else:
                led.write(b"m0\n")
        time.sleep(2)

except KeyboardInterrupt:
    led.write(b"f(-_-)\n")
    led.close()
    sys.exit()
