#### Wiring to the wind instrument: 
(Davis Instruments Anemometer and weather vane #6410; about $120 from Amazon and even less on ebay):
  - Anemometer GREEN wire to Boron A4 (Green goes to the wiper on the pot that measures wind direction)
  - Anemometer YELLOW wire to 3.3V  (Red and Yellow are the two ends of the pot that measures direction)
  - Anemometer RED wire to GND  (Red and black are the cup switch - but polarity is IMPORTANT)
  - Anemometer BLACK wire to Pin 1 of the Schmitt trigger (74HC14)
  - Schmitt trigger pin 2 to Boron D3 (interrupt 0)
  - Schmitt trigger pin 14 to 5V and pin 7 to GND
  - Schmitt trigger pin 1 is pulled up to Vcc with a 10K resistor
  - Ceramic "104" cap between Gnd and Schmitt trigger pin 1

#### Anemometer:
  - Red/Black: cup switch (Red to GND)
  - Red/Yellow: The two ends of the direction pot (Yellow to Vcc; 3.3V)
  - Green: The wiper on the direction pot (Green to A4)
  - I currently have the Mussel Rock wind vane set up so that it reads 175.5 when it's exactly aligned with the arm
    (i.e. arm pointing to the West and wind coming from the west.)
  - Davis 6410 Anemometer:  Speed in mph = 2.250 * counts/sec


#### Wiring to the BMP/BME280
This may differ depending on your sensor

|BMP280 | Boron|
|-------|------|
|1. Vcc | 3.3V |
|2. GND | GND  |
|3. SCL | D1 (Don't forget 4.7K pull up resistors on both A4 and A5)|
|4. SDA | D0 (Don't forget 4.7K pull up resistors on both A4 and A5)|
|5. CSB | 3.3V to select I2C mode (Not sure this is required as I2C should be default mode)|
|6. SDO | 3.3V to select slave address 0x77|

#### Wiring the Watchdog/Arduino Pro-Mini
|Boron | Arduino |
|------|---------|
|A5    | held high (3.3V) with a 20K resistor |
|D2    | Arduino D2 (This is the line that pings the Arduino to let it know all is well still.)|
|Rst   | Arduino D3 (This resets the Boron if a ping isn't received for 20 minutes.)|

#### Solar charge controller:
 - Connect the battery, solar panel, then load.  Dicsonnect them in the reverse order
 - On the solar charge controller there's a low-voltage cut-off which defaults to 10.7V.  There's also a "reconnect" threshold
   which defaults to 12.6V.  I reprogrammed the reconnect to occur at 12.4 volts.
 - The system draws just under 40mA from the 12V battery once it's up and running.
   So that should run 175 hours (about one week) on a 7AH gel cell.
 - I measured the current from the panel and found it was only giving me 14mA on an overcast day through the window (with the battery at 12.2V).
   A bit of research suggests that solar panels produce only 10%-25% of their normal output on overcast days, and Low-E windows can reduce
   their output by more than 50%.  I then opened the window (still overcast day) and got it up to 33mA.  By holding it outside the window
   a bit it went up to 40mA (maybe it was in the shadow of the roof overhang).
   When the sun peeked out for a few minutes I took additional readings: 98mA through the window and 160mA not looking through window.
   More solar panel results: bright sunny winter day - 305mA through bedroom window; 430mA without window (with battery already at 3.8V).
   ExpertPower recommended settings for EXP1233 battery:  Type B01 (14.4V boost, absorption, equalization); Float 13.8V; Discharge stop 11.0V.

#### Battery voltage monitoring:
 - Use 22K and 4.7K resistors to make a voltage divider (12V will map to 2.11V);  22K on top (to +side of gel cell) and 4.7K on bottom (to Gnd).
   Center of divider connects to input A5 of the Boron.
   
**IMPORTANT NOTE:** the particular charge controller I got for our wind station has positive common between the solar panel input, battery input, and load.
     This would normally make it impossible to read battery voltage from an analog input on the Boron.  But as luck would have it it the
     battery and USB output (that I use to power the Boron) share a common ground.  This makes it possible.  DO NOT do this if the
     battery and Boron don't share a common ground.

#### Solar Notes 
(Electron -legacy) looks like this settles into about 43mA at 5V; but frequently goes up to 160mA. Apparently it can run more than 1A during a publish.
