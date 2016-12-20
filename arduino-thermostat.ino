//this code is distributed under a creative commons license
//written by thecheese429 - thecheese429@gmail.com
#define version 1.101

#include <FastIO.h>
#include <I2CIO.h>
#include <LCD.h>
#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>
#include <Time.h>
#include <TimeLib.h>
#include <BitBool.h>
#include <OnewireKeypad.h>

//inputs		
#define TEMPSENSE A0
#define PROXIMITYSENSE A1
#define KEYPAD	A2
		
//outputs		
#define BACKLIGHTPIN 3		
#define COMPRESSORPIN 11	
#define FANPIN 13	
#define HEATPIN 12
		
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
#define ROLLINGAVERAGECOUNT 1000	//The number of past temperature values used to smooth the temperature

//interface variables
#define LIGHTTIMEOUT 5000
#define RAMPTIME 1600
#define PROXIMITYBRIGHTNESS 60		//the brightness of the LCD screen when it illuminates for proximity
#define CYCLEDELAY 200
#define PROXIMITYTHRESHHOLD 100			//the threshhold for registering an object near proximity sensor

//miscellaneous
#define TEMPCONVERSION 1	//UPDATE

double currTemp = 0;
unsigned long currTime;
byte mode = 0;


class Output 
{
	private:
		unsigned long currTime;
		unsigned long lastActivated = 0;
		unsigned long lastDeactivated = 0;
		unsigned long lastCalledActive = 0;
		unsigned long lastCalledInactive = 0;
		unsigned long activationDelay;
		unsigned long deactivationDelay;
		boolean isActive = 0;
		boolean isCalledActive = 0;
		boolean isCalledInactive = 0;
		int pin;
		
	public:
		
		
		
	Output(unsigned long activationDelay, unsigned long deactivationDelay, int pin)
	{
		this->currTime = currTime;
		this->pin = pin;
		this->activationDelay = activationDelay;
		this->deactivationDelay = deactivationDelay;
		pinMode(pin, OUTPUT);
	}
	
	unsigned long activate(unsigned long currTime)
	{
		if(isActive)
		{
			return 0;
		}
		
		isCalledInactive = 0;
		if( !isCalledActive )
		{
			lastCalledActive = currTime;
			isCalledActive = 1;
		}
		if(  lastCalledActive - lastDeactivated > activationDelay )
		{
			isActive = 1;
			digitalWrite(pin, 1);
			return 0;
		}
		return ( activationDelay - lastCalledActive + lastDeactivated );
	}
	
	unsigned long deactivate(unsigned long currTime)
	{
		if( !isActive )
		{
			return 0;
		}
		
		isCalledActive = 0;
		if( !isCalledInactive )
		{
			lastCalledInactive = currTime;
			isCalledInactive = 1;
			
		}
		if( lastCalledInactive - lastActivated > deactivationDelay )
		{
			isActive = 0;
			digitalWrite(pin, 0);
			return 0;
		}
		return ( deactivationDelay - lastCalledInactive + lastActivated );
	}
};

class Input
{
	private:
		int smoothing;
		int pin;
	
	public:
		double value;
	
	Input(int smoothing, int pin)
	{
		this->smoothing = smoothing;
		this->pin = pin;
		value = 0;
		pinMode(pin, INPUT);
		for(int x = 0; x < 10; x++)
		{
			value += analogRead(pin) * TEMPCONVERSION;
		}
		value /= 10;
	}
	
	void update()
	{
		value *= (smoothing - 1);
		value += analogRead(pin) * TEMPCONVERSION;
		value /= smoothing;
	}
};
		
class Backlight
{
	private:
		byte level;
		int increment;
		unsigned long currTime;
		
	public:
	
	Backlight(unsigned long currTime)
	{
		this->currTime = currTime;
		
		//increment = (level - target)*255/RAMPTIME;
	}
};	
	
void setup()
{
	
	Output compressor(COMPRESSORDELAY, 0, COMPRESSORPIN);
	Output heater(0,0, HEATPIN);
	Output fan(FANSTARTDELAY, FANSTOPDELAY, FANPIN);
	
	Input temperature(100, TEMPSENSE);
	Input proximity(1, PROXIMITYSENSE);
	
	LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address and pin mapping
	lcd.begin(16,4);
	
	lcd.home();
	lcd << "         Arduino";
	lcd.setCursor(0,1);
	lcd << "      Thermostat";
	lcd.setCursor(0,3);
	lcd << "S/W Ver. ";
	lcd.print(version, 3);
	delay(1000);
	
	for(int x = 15; x >=0; x--)
	{
		for (int y = 3; y >=0; y--)
		{
			lcd.setCursor(x,y);
			lcd.print(" ");
		}
		delay(60);
	}
}

void loop()
{
	currTime = millis();
/*	if(currTime - lastUpdated > CYCLEDELAY)
	{
		;
	}*/
}