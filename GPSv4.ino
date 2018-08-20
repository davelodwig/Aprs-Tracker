#include <LibAPRS.h>
#include <SoftwareSerial.h>
#include <TinyGPS.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/*
 * Maintained By David J. Lodwig <david.lodwig@davelodwig.co.uk>
 * Forked from John Weers APRS Tracker https://github.com/jweers1/Aprs-Tracker
 */

 /*
  * 
  * The following variables control the operation of the beacon, you need
  * to set your callsign, ssid, and the intervals in which you want it to
  * beacon in seconds (speed increases the faster you are moving), and the time between
  * screen refreshes.
  * 
  * You don't want to set the beacon interval any lower than 60 seconds, and no longer
  * than about 10 minutes (600 seconds) for general mobile use.
  * 
  */

char    ConfigCallsign[8] ;
int8_t  ConfigSsid ;
int8_t  ConfigDelay ;
 
#define ADC_REFERENCE REF_5V
#define OPEN_SQUELCH false

TinyGPS gps;
SoftwareSerial ss(11, 12);
LiquidCrystal_I2C lcd(0x3F,16,2);

int8_t lastUpdate    = 0 ;
int8_t currentscreen = 0 ;
int8_t currentrow    = 0 ;

/* ************************************************************************* */
/* * FUNCTION DECLARATIONS                                                 * */
/* ************************************************************************* */

static char *ftoa(char *a, double f, int precision) ;

static void lcd_display_startup () ;
static void lcd_display_nosignal () ;
static void lcd_display_noconfig () ;
static void lcd_display_home ( float flat, float flon, char timeclock ) ;

/* ************************************************************************* */
/* * STARTUP                                                               * */
/* ************************************************************************* */

void setup() {
  
  // initialize serial output, 9600 is mandatory. the aprs stuff can't hack a faster rate
  // even though it doesn't directly use this.
  Serial.begin(9600);

  // initialize serial to talk with the GPS. 9600 is mandatory.
  ss.begin(9600);
  
  // LED display to tell me when we are transmitting.
  pinMode ( 2, OUTPUT ) ;

  // MENU BUTTONS
  pinMode ( 13, INPUT_PULLUP ) ;    // UP
  pinMode ( 15, INPUT_PULLUP ) ;    // DOWN
  pinMode ( 16, INPUT_PULLUP ) ;    // LEFT
  pinMode ( 17, INPUT_PULLUP ) ;    // RIGHT
  pinMode ( 9, INPUT_PULLUP ) ;     // OK BUTTON
  pinMode ( 8, INPUT_PULLUP ) ;    // CANCEL BUTTON

  // Intialise the lcd display driver.
  lcd.init() ;
  lcd.backlight() ;
  lcd_display_startup () ;
  
  // Initialise APRS library - This starts the mode
  APRS_init(ADC_REFERENCE, OPEN_SQUELCH);
  
  // You must at a minimum configure your callsign and SSID
  // you may also not use this software without a valid license
  APRS_setCallsign ( ConfigCallsign, ConfigSsid ) ;
}

/* ************************************************************************* */
/* * THE LOOP                                                              * */
/* ************************************************************************* */

void loop() {
  
  float flat, flon;
  unsigned long age, date, time, chars = 0;
  unsigned short sentences = 0, failed = 0;
  byte buttonState;
  unsigned long gps_speed;
  int8_t FlexibleDelay = ConfigDelay;

  // If we are not in the config menu then
  // behave in tracking mode.
  
  if ( currentscreen < 5 ) {
    
    // get where we are.
    gps.f_get_position(&flat, &flon, &age);
  
    // speed comes in 100th's of a knot.  30 knots = 34.5 mph
    // 100 for gps_speed = 1 knot.
    gps_speed = gps.speed();
  
    // 10 mph = no alteration
    // 20 mph = /2
    // 30 mph = /3
    // 40 mph = /4
  
    if ((gps_speed > 100) && (gps_speed <20000)) {
     
     FlexibleDelay = ConfigDelay /(gps_speed / 1000) ;
     
    } else {
      
      FlexibleDelay = ConfigDelay; 
      
    }

    if ((flat==TinyGPS::GPS_INVALID_F_ANGLE) || (flon==TinyGPS::GPS_INVALID_F_ANGLE)) {
  
      // No satellites found, display the error.
      lcd_display_nosignal () ;
  
    } else {

      if (((millis()-lastUpdate)/1000 > FlexibleDelay)||(ConfigDelay==0)) {
    
        locationUpdate(flat,flon);
        lastUpdate=millis();


        lcd_display_home ( flat, flon, gps ) ;
      }
      
     }
  
     // Has the user asked to see the menu
     buttonState = digitalRead ( 9 ) ;
     if ( buttonState == 0 ) {

      currentscreen = 5 ;
      currentrow = 0 ;
      lcd_display_menu () ;

     }
    
  } else if ( currentscreen > 4 ) {

    if ( currentscreen == 5 ) {

       // Read some buttons and decide what to do
       // only ones that matter here are up, down
       // OK and Cancel.   
       byte upState = button_debounce ( 13 ) ;
       byte dnState = button_debounce ( 15 ) ;
       byte okState = button_debounce ( 9 ) ;
       byte cnState = button_debounce ( 8 ) ;

       if ( cnState == 1 ) {

          // back to the home screen
          currentscreen = 0 ;
       
       } else if ( upState == 1 ) {

          if ( currentrow == 0 ) {
            
            currentrow == 3 ;
            
          } else {
            
            currentrow = currentrow - 1 ;
            
          }

          lcd_display_menu () ;
        
       } else if ( dnState == 1 ) {

         if ( currentrow == 3 ) {
            
            currentrow == 0 ;
            
          } else {
            currentrow = currentrow + 1 ;
          }

          lcd_display_menu () ;
        
       }
       
       
    }
    
  }
    
}

  byte button_debounce ( int8_t pin ) {

  if (digitalRead(pin) == 0){
    delay ( 250 ) ;
    if (digitalRead(pin) == 0) {
      return 1 ;        
    }
   }
  return 0 ;
}

/*
 * Converts float to char for display
 */
char *ftoa(char *a, double f, int precision) {
 
  long p[] = {0,10,100,1000,10000,100000,1000000,10000000,100000000};
 
  char *ret = a;
  long heiltal = (long)f;
  itoa(heiltal, a, 10);
  while (*a != '\0') a++;
  *a++ = '.';
  long desimal = abs((long)((f - heiltal) * p[precision]));
  itoa(desimal, a, 10);
  
  return ret;
}



void locationUpdate(float flat,float flon) {
  
  // Let's first set our latitude and longtitude.
  // These should be in NMEA format!
  // flat, negative == Southern hemisphere
  // flon, negative == western hemisphere
  //  for instance...  43.646808, -116.286437  
  //nmea then is:
  //  43 .. (.646808*60),N
  // 116 .. (.286437*60),W  (w since it was negative)
  // APRS chokes when you send a ,N though... 
  // it also chokes when you send more than 2 decimal places.

  int8_t temp;
  char y[13];
  char z[13];

  // CONVERT to NMEA.  
  if (flat<0) {
    
    temp=-(int)flat;
    flat=temp*100 - (flat+temp)*60;
    ftoa((char*)&y,flat,3);   
    //z[10]=',';
    
    if (flat > 10000) {
      y[8]='S';
      y[9]=0;
     
     } else {
      
      y[7]='S';
      y[8]=0;
      
     }
    
    } else {
      
      temp=(int)flat;
      flat=temp*100 + (flat-temp)*60;
      ftoa((char*)&y,flat,3);      
      //z[10]=',';
      
      if (flat > 10000) {
        
        y[8]='N';
        y[9]=0;
        
      } else {
      
        y[7]='N';
        y[8]=0;
     
      }  
    }
     
   APRS_setLat(y);
  
  if (flon<0){
    temp=-(int)flon;
    flon=temp*100 - (flon+temp)*60;
    ftoa((char*)&z,flon,3);   
     //z[10]=',';
     if (flon > 10000){
       z[8]='W';
       z[9]=0;
     } else {
      z[7]='W';
      z[8]=0; 
     }
  } else {
    temp=(int)flon;
    flon=temp*100 + (flon-temp)*60;
    ftoa((char*)&z,flon,3);   
     //z[10]=',';
     if (flon > 10000){
       z[8]='E';
       z[9]=0;
     } else {
      z[7]='E';
      z[8]=0; 
     }
     
}
    APRS_setLon(z);
 
 // done converting to NMEA.
  
  // And send the update
  APRS_sendLoc(F("Mobile Tracker"), 14);
  
}

void aprs_msg_callback(struct AX25Msg *msg) {
  
  // Don't do anything as we can't recieve packets.
  // Also not bothering here saves some memory.
  
}

/* ************************************************************************* */
/* * LCD DISPLAY FUNCTIONS                                                 * */
/* ************************************************************************* */

static lcd_display_menu () {

  // Tell the loop it's in the menu now, we'll use this function to change the
  // screen and accept input each time it goes around.
  
  if ( currentscreen == 5 ) {
    
    lcd.clear() ;

    lcd.setCursor ( 0, 0 ) ;
    lcd.print (F("Configuration")) ;
  
    if ( currentrow == 0 ) {

      lcd.setCursor ( 0, 1 ) ;
      lcd.print (F("Set Callsign")) ;
        
    } else if ( currentrow == 1 ) {

      lcd.setCursor ( 0, 1 ) ;
      lcd.print (F("Set SSID")) ;
    
    } else if ( currentrow == 2 ) {

      lcd.setCursor ( 0, 1 ) ;
      lcd.print (F("Set Comment")) ;
    
    } else if ( currentrow == 3 ) {

      lcd.setCursor ( 0, 1 ) ;
      lcd.print (F("Set Interval")) ;
      
    }

  }

  
  
}

/*
 * Displays the tracker startup screen.
 */
static void lcd_display_startup () {

  lcd.clear() ;
  lcd.setCursor ( 0, 0 ) ;
  lcd.print ("Raynet Tracker") ;
  lcd.setCursor ( 0, 1 ) ;
  lcd.print ("Version: 1.2") ;
  delay ( 5000 ) ;
}

static void lcd_display_nosignal () {

  if ( currentscreen != 1 ) {

    currentscreen = 1 ;
    
    lcd.clear() ;
    lcd.setCursor ( 0, 0 ) ;
    lcd.print ( ConfigCallsign ) ;
    lcd.setCursor ( 0, 1 ) ;
    lcd.print ( F("No Signal Found") ) ;
  }
}

static void lcd_display_noconfig () {
  lcd.clear() ;
  lcd.setCursor ( 0, 0 ) ;
  lcd.print (F("Unable to read configuration file")) ;
 }

static void lcd_display_home ( float flat, float flon, TinyGPS &gps ) {

  int8_t temp ;
  char y[13];
  char z[13];

  int year;
  byte month, day, hour, minute, second, hundredths;
  unsigned long age;
  
  if ( flat<0 ) {
    
    temp =- (int) flat ;
    flat = temp*100 - (flat+temp)*60 ;
    ftoa((char*)&y,flat,3);   
    
    if (flat > 10000) {
      
      y[8]='S';
      y[9]=0;
    
    } else {
      
      y[7]='S';
      y[8]=0;
      
    }
    
  } else {

    temp=(int)flat;
    flat=temp*100 + (flat-temp)*60;
    ftoa((char*)&y,flat,3);      
    
    if (flat > 10000) {

      y[8]='N';
      y[9]=0;
        
    } else {
      
      y[7]='N';
      y[8]=0;
     
    }  
  }

  if ( flon<0 ) {
    
    temp=-(int)flon;
    flon=temp*100 - (flon+temp)*60;
    ftoa((char*)&z,flon,3);
    
    if (flon > 10000) {
      
      z[8]='W';
      z[9]=0;
      
     } else {
      
      z[7]='W';
      z[8]=0; 
      
     }
     
  } else {
  
    temp=(int)flon;
    flon=temp*100 + (flon-temp)*60;
    ftoa((char*)&z,flon,3);   
    
    if (flon > 10000) {
      
      z[8]='E';
      z[9]=0;
      
    } else {
    
      z[7]='E';
      z[8]=0; 
     
    }
  }

  if ( currentscreen != 3 ) {

    currentscreen = 3 ;
    lcd.clear() ;
    lcd.setCursor ( 0, 0 ) ;
    lcd.print ( ConfigCallsign ) ;

  }
    
  lcd.setCursor ( 8, 0 ) ;
  lcd.print ( y ) ;
  lcd.setCursor ( 8, 1 ) ;
  lcd.print ( z ) ;

  lcd.setCursor ( 0, 1 ) ;
  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
  if (age == TinyGPS::GPS_INVALID_AGE) {
    lcd.print ( F("       ") ) ;
  } else {
    char sz[6];
    sprintf ( sz, "%02d:%02d", hour, minute ) ;
    lcd.print ( sz ) ;
  }
}

