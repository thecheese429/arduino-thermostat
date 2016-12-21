//this code is distributed under a creative commons license
//written by thecheese429 - thecheese429@gmail.com
#define version 1.103



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
#define COMPRESSORDELAY 1500		//millisecond delay between compressor shutoff and startup
#define COOLTEMPRANGE 0.5			//temperature difference between turning on and turning off, during cooling mode
#define HEATTEMPRANGE 0.1			//temperature difference between turning on and off, during heating mode
#define FANSTARTDELAY 1000			//delay between compressor or furnace start and fan starting
#define FANSTOPDELAY 4500			//delay between compressor or furnace shutting off and fan shutting off
#define ROLLINGAVERAGECOUNT 100	//The number of past temperature values used to smooth the temperature

#define DEFAULTHEAT 24
#define DEFAULTCOOL 25

//interface variables
#define LIGHTTIMEOUT 4000
#define RAMPTIME 400
#define PROXIMITYBRIGHTNESS 60		//the brightness of the LCD screen when it illuminates for proximity
#define CYCLEDELAY 200
#define PROXIMITYTHRESHHOLD 100			//the threshhold for registering an object near proximity sensor

/*++++++++++++++++++++++++++++++++++++++++++++++
	
	NO VARIABLES BELOW THIS LINE
	
*/	

#include <FastIO.h>
#include <I2CIO.h>
#include <LCD.h>
#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>
#include <Time.h>
#include <TimeLib.h>
#include <BitBool.h>
#include <OnewireKeypad.h>


//miscellaneous
#define TEMPCONVERSION 1	//UPDATE

double currTemp = 0;
unsigned long currTime;
unsigned long buttonTime = 0;
byte mode = 0;		// The mode determines if the heat or cool is regulated, or if the fan should stay on. 
byte state = 0;		// The state contains the active on and off states of the outputs, based on temperature and mode. - | - | - | - | - | cool | heat | fan
double heatTarget = DEFAULTHEAT;
double coolTarget = DEFAULTCOOL;


LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address and pin mapping

class Output 
{
	public:
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
		byte pin;
		
	Output(unsigned long activationDelay, unsigned long deactivationDelay, byte pin)
	{
		this->currTime = currTime;
		this->pin = pin;
		this->activationDelay = activationDelay;
		this->deactivationDelay = deactivationDelay;
		pinMode(pin, OUTPUT);
	}
	
	void setState(unsigned long currTime, boolean callState)
	{
		this->currTime = currTime;
		
		if(isActive == 1)
		{
			if(callState == 0)
			{
				if(isCalledInactive == 0)
				{
					lastCalledInactive = currTime;
					isCalledInactive = 1;
					Serial << "setState calling pin " << pin << " to Inactive\n";
				}
			}
		}
		else
		{
			if(callState == 1)
			{
				if(isCalledActive == 0)
				{
					lastCalledActive = currTime;
					isCalledActive = 1;
					Serial << "setState calling pin " << pin << " to Active\n";
				}
			}
		}
	}
	
	void update(unsigned long currTime)
	{
		this->currTime = currTime;
		
		if(isCalledActive == 1)
		{
			if(isActive == 0)
			{
				if(currTime - lastCalledActive > activationDelay)
				{
					isCalledActive = 0;
					isActive = 1;
					lastActivated = currTime;
					digitalWrite(pin, isActive);
					
					Serial << "Class Output setting pin " << pin << " to " << isActive << "\n";
				}
			}
		}
		else if(isCalledInactive)
		{
			if(isActive == 1)
			{
				if(currTime - lastCalledInactive > deactivationDelay)
				{
					isCalledInactive = 0;
					isActive = 0;
					lastDeactivated = currTime;
					digitalWrite(pin, isActive);
					Serial << "Class Output setting pin " << pin << " to " << isActive << "\n";
				}
			}
		}
	}
};

class Input
{
	private:
		int smoothing;
		byte pin;
	
	public:
		double value;
	
	Input(int smoothing, byte pin)
	{
		this->smoothing = smoothing;
		this->pin = pin;
		value = 0;
		pinMode(pin, INPUT);
		for(int x = 0; x < 10; x++)
		{
			value += analogRead(pin);
		}
		value /= 10;
	}
	
	void update()
	{
		value *= (smoothing - 1);
		value += analogRead(pin);
		value /= smoothing;
	}
};
		
class Backlight
{
	public:
		byte pin;
		int level = 0;
		int lastLevel = 0;
		int target = 0;
		int rampTime;
		double slope;
		unsigned long currTime = 0;
		unsigned long targetSetTime = 0;
		boolean isDecreasing = 0;
		boolean isIncreasing = 0;
		
	
	Backlight( byte pin, int rampTime )
	{
		this->pin = pin;
		this->rampTime = rampTime;
		
		pinMode(pin, OUTPUT);
	}
	
	void setTarget(unsigned long currTime, int target)
	{
		if(this->target != target)
		{
			this->target = target;
			this->targetSetTime = currTime;
			lastLevel = level;
			slope = double(target - level) / rampTime;
		}
		
	}
	
	void update(unsigned long currTime)
	{
		this->currTime = currTime;
		
		if(level != target)
		{
			if(currTime - targetSetTime > rampTime)
			{
				level = target;
			}
			else
			{
				level = lastLevel + (currTime - targetSetTime) * slope;
			}
			analogWrite(pin, level);
		}			
	}		
};	
	
Output compressor(COMPRESSORDELAY, 0, COMPRESSORPIN);
Output heater(0,0, HEATPIN);
Output fan(FANSTARTDELAY, FANSTOPDELAY, FANPIN);

Input temperature(100, TEMPSENSE);
Input proximity(5, PROXIMITYSENSE);

Backlight backlight(BACKLIGHTPIN, RAMPTIME);	

void setup()
{
	
	

	mode = B00000101;
	
	lcd.begin(16,4);
	Serial.begin(9600);
	while(!Serial);
	
	lcd.home();
	lcd << "Arduino";
	lcd.setCursor(0,1);
	lcd << "Thermostat";
	lcd.setCursor(0,3);
	lcd << "S/W Ver. ";
	lcd.print(version, 3);
	delay(500);
	
	for(int x = 15; x >=0; x--)
	{
		for (int y = 3; y >=0; y--)
		{
			lcd.setCursor(x,y);
			lcd.print(" ");
		}
		delay(10);
	}

}

void setBacklight(unsigned long currTime)
{
	if(currTime - buttonTime < LIGHTTIMEOUT )
	{
		backlight.setTarget(currTime, 255);
	}
	else if( proximity.value > PROXIMITYTHRESHHOLD )
	{
		backlight.setTarget(currTime, PROXIMITYBRIGHTNESS);		
	}
	else if(backlight.target != 0)
	{
		backlight.setTarget(currTime, 0);
	}
}

void determineState(double temperature)
{
	
	if( mode == B00000000 )	//system is off
	{
		state = mode;
	}
	else if( mode & B00000110 == B00000110 )	//system is AUTO
	{
		if(temperature > coolTarget + COOLTEMPRANGE)	//Temp is warm emough to start cooling
		{
			state = B00000101;
		}
		else if( temperature < heatTarget + HEATTEMPRANGE)
		{
			state = B00000011;
		}
		else if(temperature > heatTarget && temperature < coolTarget)
		{
			state = B00000000;
		}
	}
	else if( mode & B00000110 == B00000100 )	//system is set to cool only
	{
		if(temperature > coolTarget + COOLTEMPRANGE)	//Temp is warm emough to start cooling
		{
			state = B00000101;
		}		
		else if(temperature < coolTarget )
		{
			state = B00000000;
		}
		
	}
	else if(mode & B00000110 == B00000010 ) //set to heat only
	{
		if(temperature < heatTarget - HEATTEMPRANGE )
		{
			state = B00000011;
		}
		else if( temperature > heatTarget )
		{
			state = B00000000;
		}
	}
	
	
	if( mode & B00000001 == B00000001 ) //fan is on constant
	{
		state = state | B00000001;
	}
	
	
	/*if(mode & B00000111 == B00000001)	//mode is set to fan on constantly, no heat or cool.
	{
		if(state != B00000001)
			Serial << "determineState set state to B00000001 from fan only\n";
		state = B00000001;	//turn on the fan only
		
	}
	else if(state == B00000000)
	{
		if(state != B00000000)
			Serial << "determineState set state to B00000000 from fan only\n";
		state = B00000000;
	}
	
	if(mode & B0000110 == B00000110)	//mode is set to heat and cool automatically
	{
		if(temperature < heatTarget - HEATTEMPRANGE)
		{
			if(state != B00000101)
				Serial << "determineState set state to B00000101 by auto\n";
			state = B00000101;	//turn on the fan and compressor
			
		}
		else if(temperature > coolTarget + COOLTEMPRANGE)
		{
			if(state != B00000011)
				Serial << "determineState set state to B00000011 by auto\n";
			state = B00000011;	//turn on the fan and heat
		}
		else if(temperature > heatTarget && temperature < coolTarget) 	//if the temperature is within range
		{
			if(state != B00000000)
				Serial << "determineState set state to B00000000 from auto\n";
			state = B00000000;		//turn the outputs off
		}
	}
	if(mode & B00000110 == B00000010 )	//mode is set to heat only
	{
		if(temperature > coolTarget + COOLTEMPRANGE)
		{
			if(state != B0000011)
				Serial << "determineState set state to B00000011 for heating only \n";
			state = B00000011;		//turn on the fan and heat
		} 
		else if(temperature < coolTarget) //if the temperature has dropped below the cooling target
		{
			if(state != B00000000)
				Serial << "determineState set state to B00000000 from heating only\n";
			state = B00000000;		//turn everything off
		}
	}
	if(mode & B0000110 == B0000100 )	//mode is set to cool only
	{
		if(temperature < heatTarget - HEATTEMPRANGE)	//the temperature is below the heat target by at least the maximum delta
		{
			
			if(state != B00000101)
				Serial << "determineState set state to B00000101 for cool only\n";
			state = B00000101;		//turn on the heat and fan
		}
		else if( temperature > heatTarget)	//if the heat is above the target
		{
			if(state != B00000000)
				Serial << "determineState set state to B00000000 from cool only\n";
			state = B00000000;		//turn everything off
		}
	}*/
		
}

void setState(unsigned long currTime)
{
	if( state & B00000001 > 0)
	{
		fan.setState(currTime, 1);
	}
	else
	{
		fan.setState(currTime, 0);
	}
	
	if( state & B00000100 > 0)
	{
		compressor.setState(currTime, 1);
	}
	else
	{
		compressor.setState(currTime, 0);
	}
	
	if(state & B00000010 > 0)
	{
		heater.setState(currTime, 1);
	}
	else
	{
		heater.setState(currTime, 0);
	}
	
}

void loop()
{
	currTime = millis();
	
	compressor.update(currTime);
	heater.update(currTime);
	fan.update(currTime);
	
	temperature.update();
	proximity.update();
	
	backlight.update(currTime);
	
	currTemp = temperature.value * TEMPCONVERSION;
	
	setBacklight(currTime);
	determineState(currTemp);
	setState(currTime);
	
	lcd.home();
	lcd << "Temp = " << temperature.value << "  ";
	lcd.setCursor(0,1);
	lcd << "Prox = " << proximity.value << "  ";
	lcd.setCursor(0,2);
	lcd << "mode = ";
	lcd.print(mode, BIN);
	lcd << "        ";
	lcd.setCursor(0,3);
	lcd << "state= ";
	lcd.print(state, BIN);
	lcd << "        ";
	
}