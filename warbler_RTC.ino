#include <RTClib.h>

#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#include <Wire.h>         // this #include still required because the RTClib depends on it
#include <RTClib.h>
#include <Adafruit_SleepyDog.h>

// Setup: Attach MP3 shield to Arduino per MP3 directions. 
//    RTC:  Attach GND to Arduino GND
//          Attach +5V to Arduino A3
//          Attach SDA to Arduino A4
//          Attach SCL to Arduino A5
// These are the pins used for the music maker shield
#define SHIELD_RESET  -1     // VS1053 reset pin (unused in shield mode)
#define SHIELD_CS     10      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)
// These are common pins between breakout and shield
#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin
// create shield-example object!
Adafruit_VS1053_FilePlayer musicPlayer = 
    Adafruit_VS1053_FilePlayer(11,12,13,SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);
    
// Setup: The following setup is for the RTC chip...
#define RTC_POWER A3
RTC_DS1307 rtc;    
#define SECS_PER_HOUR 60*60
#define SECS_PER_DAY SECS_PER_HOUR*24


#define MONITOR 1
// If MONITOR is 0 (normal operation), start time must be hard-coded
#define STARTHOUR 8
#define STOPHOUR 12

// Global Variables
DateTime next_state_change;
enum state_e { PASSIVE, ACTIVE_SING, ACTIVE_REST } current_state,next_state;
char *tracks[6];
int track=0;
int trackMax=5;

void setup() {
  // setSyncInterval(SECS_PER_WEEK);
  pinMode(RTC_POWER,OUTPUT);
  pinMode(A2,OUTPUT);
  digitalWrite(RTC_POWER,LOW); 
  digitalWrite(A2,LOW);  
  Wire.begin();
  digitalWrite(RTC_POWER,HIGH);
  rtc.begin();
  digitalWrite(RTC_POWER,LOW);
  tracks[0]=(char *)"TRACK001.MP3";
  tracks[1]=(char *)"TRACK002.MP3";
  tracks[2]=(char *)"TRACK003.MP3";
  tracks[3]=(char *)"TRACK0004.MP3";
  tracks[4]=(char *)"TRACK005.MP3";
  tracks[5]=(char *)"TRACK006.MP3";
  
  if (MONITOR==1) {
    Serial.begin(9600);
    Serial.flush();
    digitalWrite(RTC_POWER,HIGH);
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Set time/date based on last compile time
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0)); // Set time/date to January 21, 2014 at 3am 
    digitalWrite(RTC_POWER,LOW);
    DateTime adj=getTime();
    Serial.print("Adjusted time to:");
    printTimeStamp(adj);
    Serial.println();
  }

  DateTime clk=getTime();
  DateTime next_active=DateTime(clk.year(),clk.month(),clk.day(),STARTHOUR,0,0); // Start at 10 AM
  DateTime next_passive=DateTime(clk.year(),clk.month(),clk.day(),STOPHOUR,0,0);; // Stop at 2 PM
  if (clk.secondstime()<next_active.secondstime()) {
    current_state=PASSIVE; 
    next_state=ACTIVE_REST;
    next_state_change=next_active;
  } else if (clk.secondstime() < next_passive.secondstime()) { 
    current_state=ACTIVE_REST; 
    int interval=random(6); // set to a number between 0 and 5
    if (clk.secondstime()+interval+5 < next_passive.secondstime()) {
      next_state=ACTIVE_SING;
      next_state_change=clk+(interval+5)*60;
    } else {
      current_state=PASSIVE;
      next_state=ACTIVE_REST;
      next_state_change=DateTime(clk.year(),clk.month(),clk.day()+1,STARTHOUR,0,0);;     
    }
  }

  digitalWrite(CARDCS,HIGH); // Turn on power to MP3 player  
  pinMode(A0,OUTPUT);
  digitalWrite(A0,HIGH);
  musicPlayer.dumpRegs();
  if (! musicPlayer.begin()) { // initialise the music player
    musicPlayer.dumpRegs();
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }
  SD.begin(CARDCS);    // initialise the SD card
  musicPlayer.setVolume(3,3); // Set volume for left, right channels. lower numbers == louder volume!
  // musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT); // DREQ int
  digitalWrite(CARDCS,LOW); // Turn off power to MP3 player by default
  playSong();

}

void loop() {

  DateTime clk=getTime();

  if (clk.minute()==0) { // Also play in the first minute of the hour just to show we are alive
    playSong();
  }

  if (clk.secondstime() >= next_state_change.secondstime()) {
    DateTime next_passive=DateTime(clk.year(),clk.month(),clk.day(),STOPHOUR,0,0);
    switch(next_state) {
      case PASSIVE:
        current_state=PASSIVE;
        next_state=ACTIVE_REST;
        next_state_change=DateTime(clk.year(),clk.month(),clk.day()+1,STARTHOUR,0,0);
        break;
      case ACTIVE_REST:
        current_state=ACTIVE_REST;
        next_state=ACTIVE_SING;
        next_state_change=clk+(5+random(6))*60;
        if (next_state_change.secondstime()>next_passive.secondstime()) {
          next_state=PASSIVE;
          next_state_change=next_passive;
        }
        break;
      case ACTIVE_SING:
        current_state=ACTIVE_SING;
        next_state=ACTIVE_REST;
        next_state_change=clk+(3+random(3))*60;
        if (next_state_change.secondstime()>next_passive.secondstime()) {
          next_state=PASSIVE;
          next_state_change=next_passive;
        }
    }
        
  }

  if (MONITOR==1) {
    // digital clock display of the time
    printTimeStamp(clk);
    switch(current_state) {
      case PASSIVE: Serial.print(" Passive "); break;
      case ACTIVE_REST: Serial.print(" Active Resting "); break;
      case ACTIVE_SING: Serial.print(" Active Singing "); break;
    }
    Serial.print(" At ");
    printTimeStamp(next_state_change);
    switch(next_state) {
      case PASSIVE: Serial.print(" -> Passive "); break;
      case ACTIVE_REST: Serial.print(" -> Active Resting "); break;
      case ACTIVE_SING: Serial.print(" -> Active Singing "); break;
    }
    Serial.println(); 
  }

  if (current_state==ACTIVE_SING) {
    playSong();
    // Wait a full 10 seconds before playing more
    DateTime after=getTime();
    while(after.secondstime() < clk.secondstime() + 10) {
      int wait = 10 - after.secondstime() + clk.secondstime();
      if (wait<=0) wait=1;
      Watchdog.sleep(wait*1000);
      after=getTime();
    }
      
  }

  if (MONITOR==1) {
    //Serial.print("About to delay for ");
    //Serial.print(interval);
    //Serial.println(" microseconds");
    Serial.flush();
  }
  
  // delay(interval); // wait until next interrupt
  Watchdog.sleep();
}

void printDigits(int digits,char prefix){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(prefix);
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

DateTime getTime() {
  digitalWrite(RTC_POWER,HIGH);
  if (!rtc.isrunning()) {
    Serial.println("RTC IS STOPPED!!!");
    while(1);
  }
  DateTime clk=rtc.now();
  digitalWrite(RTC_POWER,LOW);
  return clk;
}

void printTimeStamp(DateTime clk) {
  printDigits(clk.month(),' ');
  printDigits(clk.day(),'/');
  Serial.print("/");
  Serial.print(clk.year());
  Serial.print(" @ ");
  Serial.print(clk.hour());
  printDigits(clk.minute(),':');
  printDigits(clk.second(),':');
}

void playSong() {
  // Play music here!
    if (MONITOR==1) {
      Serial.println("Playing Music ...");
    }
    digitalWrite(CARDCS,HIGH); // Turn on power to MP3 player
    musicPlayer.playFullFile(tracks[track]);
    track++;
    if (track>trackMax) track=0;
    digitalWrite(CARDCS,LOW); // Turn off power to MP3 player for now
}

