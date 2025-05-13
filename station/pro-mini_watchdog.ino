//=========================================================================================================
// This acts as the external watchdog to reset the Electron/Boron/Argon if it hasn't pinged the Arduino in 
// 20 minutes.  It should ping the Arduino every 8 minutes
//=========================================================================================================

volatile int state = 0;
int counter = 0;
int loop_counter;



void setup()
   {
   Serial.begin(9600);  

   pinMode(2, INPUT);
   pinMode(3, OUTPUT);
   
   digitalWrite(3, HIGH);  // Start with pin-3 High.  We take it low for 5 msecs to reset the Electron)
   
   attachInterrupt(0, blink, FALLING);  // Call blink() when interrupt 0 (digital pin 2) falls
  
   loop_counter = 0;
   }




void loop()
   {
   delay(1000); // 1-second delay for each loop
  
   loop_counter += 1;

   if (state == 1)
      {
      Serial.println(counter);
      loop_counter = 0;  // We got pinged by the Electron.  Reset the watchdog timer
      state = 0;
      }
   
   if (loop_counter > 1200)
      {
      digitalWrite(3, LOW);  // We hit the timeout period without the Electron pinging us.  Reset the Electron
      delay(5);
      digitalWrite(3, HIGH);
      loop_counter = 0;
      }
   }




void blink()
  {
  state = 1;
  counter++;
  }

  
