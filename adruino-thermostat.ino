#include <Time.h>
#include <TimeLib.h>

#include <Wire.h>	
#include <BitBool.h>		
#include <OnewireKeypad.h>	

//template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; }

//inputs		
#define TEMPSENSE A0
#define PROXIMITYSENSE A1
#define KEYPAD	A2
		
//outputs		
#define BACKLIGHTPIN 3		
#define COMPRESSORPIN 4		
#define FANPIN 5		
#define HEATPIN 6		
#define SDA 7		
#define SCL 8		
		
//circuit variables		
#define TEMPRES1 1000				
#define TEMPRES2 2000		
#define THERMISTORSLOPE 1		//UPDATE
		
//HVAC variables
#define COMPRESSORDELAY 5000		//millisecond delay between compressor shutoff and startup
#define COOLDELTATEMP 0.5			//temperature difference between turning on and turning off, during cooling mode
#define HEATDELTATEMP 0.1			//temperature difference between turning on and off, during heating mode
#define FANSTARTDELAY 10000			//delay between compressor or furnace start and fan starting
#define FANSTOPDELAY 100000			//delay between compressor or furnace shutting off and fan shutting off
#define ROLLINGAVERAGECOUNT 30

//interface variables
#define LIGHTTIMEOUT 6000
#define RAMPTIME 1000
#define INPUTDELAY 200
#define PROXIMITYLEVEL 100

//various calculations
#define TEMPCONVERSION 1	//UPDATE

byte tempRaw;
double tempRollingAverage = 0;
byte mode = B00000000;				//The bits represent: fan, cool, heat,-,-,-,-,-
unsigned long lastButtonTime = 0;
unsigned long lastMeasureTime = 0;
unsigned long lastTimeUpdate = 0;
unsigned long compressorOffTime = COMPRESSORDELAY + 1;
byte proximity = 0;
byte backlightLevel = 0;



void setup() 
{
	Serial.begin(9600);
	pinMode(TEMPSENSE, INPUT);
	pinMode(KEYPAD, INPUT);
	pinMode(PROXIMITYSENSE, INPUT);
	pinMode(BACKLIGHTPIN, OUTPUT);
	pinMode(COMPRESSORPIN, OUTPUT);
	pinMode(FANPIN, OUTPUT);
	pinMode(HEATPIN, OUTPUT);
}

void updateInOut()
{
	//read inputs
	lastTimeUpdate = millis();
	if(lastTimeUpdate - lastMeasureTime > INPUTDELAY)	//these tasks don't need to happen every loop
	{
		Serial << "reading temperature";
		tempRaw = analogRead(TEMPSENSE);
		lastMeasureTime = lastTimeUpdate;
		tempRollingAverage = (tempRollingAverage * (ROLLINGAVERAGECOUNT - 1) + tempRaw*TEMPCONVERSION)/ROLLINGAVERAGECOUNT;
		
		digitalWrite(FANPIN, (B10000000 & mode) >> 7);
		digitalWrite(HEATPIN, (B00100000 & mode) >> 5);
		digitalWrite(COMPRESSORPIN, compressorOn());
	}
	proximity = analogRead(PROXIMITYSENSE);
	
	//set outputs

	analogWrite(BACKLIGHTPIN, backlightLevel);
}

void setBacklightLevel()
{
	int increment;
	
	if(lastTimeUpdate - lastButtonTime < LIGHTTIMEOUT)
	{
		backlightLevel = 255;
	}
	else
	{
		increment = (millis() - lastTimeUpdate)*255/RAMPTIME;
		
		if(proximity > PROXIMITYLEVEL)
		{
			if(255 - backlightLevel < increment)
			{
				backlightLevel = 255;
			}
			else
			{
				backlightLevel += increment;
			}
		}
		else
		{
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
}

boolean compressorOn()
{
	if( !((B01000000 & mode) >> 6) )	//if the mode dictates that the compressor be turned off, do this
	{
		if( digitalRead(COMPRESSORPIN) )	//if the compressor is on when the mode turns it off, record the time it was deactivated
		{
			compressorOffTime = lastTimeUpdate;	
		}
		return 0;
	}
	if( (B01000000 & mode) >> 6 && (lastTimeUpdate - compressorOffTime > COMPRESSORDELAY) )	//don't short cycle the compressor
	{
		return 1;
	}
	
}

void updateDisplay()
{
	lcd.clear();
	if(mode == 0)
	{
		lcd << "System off  " << hour(lastTimeUpdate) << ":" << minute(lastTimeUpdate) << "Temperature: ";
	}
	
}

void loop() 
{
	updateInOut();
	setBacklightLevel();
	updateDisplay();
}
