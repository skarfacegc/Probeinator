# Probeinator

This is the code and eventually the Fritzing / stl files for a 4 probe wifi thermometer.  This is
very much a work in progress so the BOM is shifting, but at the moment the hardware is ...

Rough BOM

- 1x38 Pin ESP32 (these are what I had, not hard to mod things to use other sizes)
- 1xADS1115 (using the Adafruit breakout, there are a few vendors that are similar)
- 4x22kOhm resistors
- 4x2.5mm mono jack
- 4xET-73 replacement probe thermistors
  - [Data Sheet](https://drive.google.com/file/d/1ukcaFtORlLmLLrnIlCA0BvS1rEwbFoyd4ReqIFV8y3iL1sojljPAW8x8bYZW/view)
  - You can really use whatever you want here.  You need to know the following values
    - beta (default 3500)
    - ref temp (default 298.15K/25C)
    - resistance at ref temp (default 200000)
- 1xLD1117 3.3v regulator
- 1x100nf capacitor (for the LD1117 circuit)
- 1x10uf capacitor (for the LD1117 circuit)

Feeding the thermistors power from the 3.3v regulator to get a stable voltage, the esp32 GPIO pins were causing random 3ish degree drifts in the temp readings.

I'm using 22k resistors as the fixed side of the divider, read that somewhere in a forum discussing this thermistor. Idea was to get the curve as steep as possible around the range you care most about, in this case ~200 (maybe 250) Didn't save the link :(

I am using this project as an excuse to get a PCB made. Will be posting/sharing the Fritzing files eventually.
