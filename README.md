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
    - ref temp (default 20°C)
    - resistance at ref temp (default 200000)

I'm currently driving the thermistors by setting a GPIO high.  The voltage isn't particularly stable, so I'm about to tear a bunch of this down and try using some 3.3v voltage regulators, will feed the thermistors and the esp32 from that.  Should get a more stable reference voltage for the voltage divider.

I'm using 22k resistors as the fixed side of the divider, read that somewhere in a forum discussing this thermistor. Idea was to get the curve as steep as possible around the range you care most about, in this case ~200 (maybe 250) Didn't save the link :(

I am using this project as an excuse to get a PCB made. Will be posting/sharing the Fritzing files eventually.
