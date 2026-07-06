# Proteox-Field-Display

This is the code for the LED field display that is connected to the Proteox.
There are three files:
- main.cpp is the firmware for the raspberry pi pico zero microcontroller.
- platformio.ini is the central config file for the project
- labrad_connection.py is the labrad code that is installed on the computer that controls the Proteox.

A note on the files:
- platformio.ini imports lib_deps = adafruit/Adafruit Protomatter, which is the library we used to write the firmware and is quite extensive (and can
  be used for future similar projects)
- labrad_connection.py connects to mercury_ips_server (the server for the Proteox) and connects to COM3. If modifying this code for another led field disply
  change these accordingly.
