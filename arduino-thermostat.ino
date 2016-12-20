#include <FastIO.h>
#include <I2CIO.h>
#include <LCD.h>
#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>
#include <Time.h>
#include <TimeLib.h>
#include <BitBool.h>
#include <OnewireKeypad.h>


//template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; }

//inputs		
#define TEMPSENSE A0
#define PROXIMITYSENSE A1
#define KEYPAD	A2
		
//outputs		
#define BACKLIGHTPIN 3		
#define COMPRESSORPIN 13	
#define FANPIN 12	
#define HEATPIN 11
		
//circuit variables		
#define TEMPRES1 1000				
#define TEMPRES2 2000		
#define THERMISTORSLOPE 1		//UPDATE
		
//HVAC variables
#define COMPRESSORDELAY 15000		//millisecond delay between compressor shutoff and startup
#define COOLTEMPRANGE 0.5			//temperature difference between turning on and turning off, during cooling mode
#define HEATTEMPRANGE 0.1			//temperature difference between turning on and off, during heating mode
#define FANSTARTDELAY 10000			//delay between compressor or furnace start and fan starting
#define FANSTOPDELAY 45000			//delay between compressor or furnace shutting off and fan shutting off
#define ROLLINGAVERAGECOUNT 300		//The number of past temperature values used to smooth the temperature

//interface variables
#define LIGHTTIMEOUT 5000
#define RAMPTIME 1600
#define PROXIMITYBRIGHTNESS 60		//the brightness of the LCD screen when it illuminates for proximity
#define INPUTDELAY 200
#define PROXIMITYTHRESHHOLD 100			//the threshhold for registering an object near proximity sensor

//miscellaneous
#define TEMPCONVERSION 1	//UPDATE



byte tempRaw;
double tempRollingAverage;
byte mode = B00000000;
byte state = B00000000;
unsigned long lastButtonTime = 0;
unsigned long lastMeasureTime = 0;
unsigned long lastTimeUpdate = 0;
unsigned long lastBacklightUpdate = 0;
unsigned long fanOffTime = 0;
unsigned long fanCallOnTime = 0;
unsigned long compressorOffTime = COMPRESSORDELAY;
byte proximity = 0;
byte backlightLevel = 0;

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

byte degC[8] = {	  
  0b11100,
  0b10100,
  0b11100,
  0b00000,
  0b00111,
  0b01000,
  0b01000,
  0b00111
  };
  
  byte degF[8] = {	  
  0b11100,
  0b10100,
  0b11100,
  0b00000,
  0b01111,
  0b01000,
  0b01110,
  0b01000
  };
  



void setup() 
{
	
	Serial.begin(9600);
	lcd.begin(16,4);
	lcd.home();
	lcd << "Arduino";
	lcd.setCursor(0,1);
	lcd << "Thermostat";
	lcd.setCursor(0,3);
	delay(500);
	lcd.clear();
	
	lcd.createChar(0, degC);
	lcd.createChar(1, degF);
	
	pinMode(TEMPSENSE, INPUT);
	pinMode(KEYPAD, INPUT);
	pinMode(PROXIMITYSENSE, INPUT);
	pinMode(BACKLIGHTPIN, OUTPUT);
	pinMode(COMPRESSORPIN, OUTPUT);
	pinMode(FANPIN, OUTPUT);
	pinMode(HEATPIN, OUTPUT);
	
	tempRollingAverage = analogRead(TEMPSENSE) * TEMPCONVERSION;
	
	
}

void updateInOut()
{
	//read inputs
	lastTimeUpdate = millis();
	if(lastTimeUpdate - lastMeasureTime > INPUTDELAY)	//these tasks will only occur after INPUTDELAY has elapsed
	{
		//Serial << "reading temperature";
		tempRaw = analogRead(TEMPSENSE);
		lastMeasureTime = lastTimeUpdate;
		tempRollingAverage = (tempRollingAverage * (ROLLINGAVERAGECOUNT - 1) + tempRaw*TEMPCONVERSION)/ROLLINGAVERAGECOUNT;
		
		digitalWrite(FANPIN, fanOn() );
		digitalWrite(HEATPIN, (B00000010 & state) >> 1);
		digitalWrite(COMPRESSORPIN, compressorOn() );
	}
	
}

void setBacklightLevel()
{
	int increment = 1;
	
	if(lastTimeUpdate - lastButtonTime < LIGHTTIMEOUT)
	{
		lcd.setCursor(0,1);
		lcd << "Waiting for T/O    ";
		backlightLevel = 255;
	}
	else
	{
		increment = ( (lastTimeUpdate - lastBacklightUpdate)*255/RAMPTIME );
		
		if(proximity > PROXIMITYTHRESHHOLD)
		{
			lcd.setCursor(0,1);
			lcd << "prox. control  " ;
			lcd.setCursor(0,2);
			lcd << "+ " << increment << "   " ;
			
			if(PROXIMITYBRIGHTNESS - backlightLevel < increment)
			{
				backlightLevel = PROXIMITYBRIGHTNESS;
			}
			else
			{
				backlightLevel += increment;
			}
		}
		else
		{
			lcd.setCursor(0,1);
			lcd << "prox. control  " ;
			lcd.setCursor(0,2);
			lcd << "- " << increment ;
			
			if(backlightLevel < increment)
			{
				backlightLevel = 0;
			}
			else
			{
				backlightLevel -= increment;
			}
		}
			
	}
	if(increment > 0)
	{
		analogWrite(BACKLIGHTPIN, backlightLevel);
		lastBacklightUpdate = lastTimeUpdate;
		
	}
}

boolean fanOn()
{
	if( B00000001 & state > 0 )	//if the fan is supposed to be on constantly
	{
		return 1;
	}
	else if( (B00000110 & state > 0) && (lastTimeUpdate - fanOffTime > FANSTARTDELAY) ) //If the fan is supposed to be on for heat or cool and the start delay has elapsed
	{
		return 1;
	}
	else if ((B00000110 & state > 0) && (lastTimeUpdate - fanOffTime < FANSTARTDELAY)	//if the fan is supposed to be on for heat or cool but the start delay has not elapsed
	{
		if( !(digitalRead(FANPIN)) )
		{
			fanCallOnTime = lastTimeUpdate;
		}
		if(lastTimeUpdate - fanCallOnTime > FANSTARTDELAY)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
}

boolean compressorOn()
{
	if(  ((B00000100 & state) >> 2) && (lastTimeUpdate - compressorOffTime > COMPRESSORDELAY) )	//if the compressor is supposed to be on and the cycle delay HAS elapsed
	{
		return 1;
	}
	else if( ((B00000100 & state) >> 2) && (lastTimeUpdate - compressorOffTime < COMPRESSORDELAY) ) //if the compressor is supposed to be on, but the cycle timeout HAS NOT elapsed
	{
		return 0;
		lcd.setCursor(0,3);
		lcd << "Cycle Delay " << int( (COMPRESSORDELAY - (lastTimeUpdate - compressorOffTime) )/1000 ) << "  "; //notify of delay to avoid a short cycle and diplay the countdown
	}
	else if( digitalRead(COMPRESSORPIN) )
	{
		compressorOffTime = lastTimeUpdate;
		return 0;
	}
	else
	{
		return 0;
	}
	

}

void updateDisplay()
{
	lcd.setCursor(0,0);
	lcd << int(tempRollingAverage);
	lcd.write(byte(1));
	lcd.setCursor(0,3);
	lcd << "backlight = " << backlightLevel << "  ";
	//lcd.clear();
	
/*
The modes are a superposition of the bits below:
00000000 - system off (fan auto)
00000001 - fan on (zero = auto)
00000010 - A/C on
00000100 - heat on
00001000 - display degrees F (zero = C)

Some interesting combinations are:
00000110 - automatic heat or cool (two separate target temperatures)
00000111 - automatic heat or cool, fan stays on

*/
	if(mode == 0)
	{
		//lcd << "System off  " << hour(lastTimeUpdate) << ":" << minute(lastTimeUpdate) << "Temperature: ";
	}
	
}

void loop() 
{
	updateInOut();
	setBacklightLevel();
	updateDisplay();
	
	if(lastTimeUpdate/4000 % 2 == 0)
	{
		proximity = 99;
	}
	else
	{
		proximity = 101;
	}
}
