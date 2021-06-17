##To-do:
- [Feature] Perhaps we should register a function using Particle.function() that allows you to change the update rate and start/stop times.
- [Electron bug] Figure out why we get a big jump in wind gust when I have a connection outage.
- [Feature] Encode mph with alpha-numerics and do fractions of MPH. Using upper-case letters and numbers we can encode 0-129 mph in 0.1 mph increments (using 2 chars).
   If we used the printable ASCII characters (32-126) we could encode speed in 1/2 MPH increments with a single character (from 0 to 47 mph).
   Two characters would allow us to describe direction to about 1/25th of a degree.
- [Feature] Maybe we should send previous data with each event.  This could allow us to fill in gaps, or to get greater granularity even if
   we don't increase the update rate.
- [Electron bug] Figure out whether the failed updates are happening on the Electron or Photon side (by watching the logs).
- [Feature] Consider having it sleep at night (lower current draw and potentially some data savings)
    https://docs.particle.io/reference/firmware/electron/#sleep-sleep-
    Probably should have sleep from sunset to sun-up.  Figure out what those hours are each month.
- [Electron bug] Check to see if it's stuck trying to connect and maybe do a reset:
      Look at SYSTEM_MODE(SEMI_AUTOMATIC) and SYSTEM_THREAD(ENABLED) to stay ontop of the background task
      and handle reconnects myself rather than letting some "obscure" magic happen.
      User can call reset() to force a full reset.
      *** With SYSTEM_THREAD(ENABLED) or Software Timers you can have your own code run to check
          Particle.connected() and do whatever you want when this returns false.
      Also look at: ApplicationWatchdog()
      Discussion here: https://community.particle.io/t/using-lots-of-data-to-connect/31292/8
- [Cleanup] Share the PCB design on a site like pcbway where they can be ordered
- [Boron Update] On Gen3 devices (and since DeviceOS 0.5.3) it is no longer necessary to set pinMode().  Pine Mtn station had an issue with bad readings on A4. Particle Support believes that setting pinmode() could be the culprit.  "You do not need to set the pinMode() to read an analog value using analogRead as the pin will automatically be set to the correct mode when analogRead is called." https://docs.particle.io/cards/firmware/input-output/analogread-adc/
