# teensypipe
An electronic bagpipe chanter based on a Teensy LC (Low Cost) board.

## Build
Connect the pins to the copper contacts corresponding to each finger.
I used thin enameled copper wire and copper tape. 
To mount the finger contacts you can use cut a PVC pipe or a 3d printed chanter.

## Install

Install python and platformio
```
pip install platformio
```

Compile and upload
```
pio run -t upload
```

Play using a midi synthesizer, I used Universal Piper running on an iPad (with a lightning to usb cable).
