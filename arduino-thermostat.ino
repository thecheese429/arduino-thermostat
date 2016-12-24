//this code is distributed under a creative commons license
//written by thecheese429 - thecheese429@gmail.com
#define version 1.105


//inputs		
#define TEMPSENSE A6
#define PROXIMITYSENSE A7
#define KEYPAD	A2
		
//outputs		
#define BACKLIGHTPIN 11		
#define COMPRESSORPIN A1	
#define FANPIN A3
#define HEATPIN A2
		
//circuit variables		
#define TEMPRES1 1000				
#define TEMPRES2 2000		
#define THERMISTORSLOPE 1		//UPDATE


#define ROLLINGAVERAGECOUNT 10	//The number of past temperature values used to smooth the temperature
		
//Heating variables
#define MINHEATCYCLETIME 3000		//millisecond delay between compressor shutoff and startup
#define HEATSTARTDELAY 0
#define HEATSTOPDELAY 0
#define HEATRESTARTDELAY 0
#define HEATTEMPRANGE 1
#define DEFAULTHEAT 20

//Cooling variables
#define MINCOMPRESSORCYCLETIME 0
#define COMPRESSORSTARTDELAY 0
#define COMPRESSORSTOPDELAY 0
#define COOLRESTARTDELAY 10000
#define COOLTEMPRANGE 2	//temperature difference between turning on and turning off, during cooling mode
#define DEFAULTCOOL 30

//Fan variables
#define MINFANCYCLETIME 0
#define FANSTARTDELAY 0
#define FANSTOPDELAY 3000
#define FANRESTARTDELAY 0


//interface variables
#define LIGHTTIMEOUT 6000
#define RAMPTIME 1000
#define PROXIMITYBRIGHTNESS 60		//the brightness of the LCD screen when it illuminates for proximity
#define CYCLEDELAY 100
#define PROXIMITYTHRESHHOLD 100			//the threshhold for registering an object near proximity sensor
#define PROXIMITYROLLINGAVERAGE 5		//the number of samples used to determine the proximity of viewer

/*++++++++++++++++++++++++++++++++++++++++++++++
	
		NO VARIABLES BELOW THIS LINE
	
++++++++++++++++++++++++++++++++++++++++++++++*/	

#include <FastIO.h>
#include <I2CIO.h>
#include <LCD.h>
#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>
#include <Time.h>
#include <TimeLib.h>
#include <BitBool.h>
#include <avr/pgmspace.h>
#include <Keypad.h>
#include <PrintEx.h>


// template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; } 


//miscellaneous
#define TEMPCONVERSION 1	//UPDATE

double currTemp = 0;
unsigned long currTime;
unsigned long buttonTime = 0;
unsigned long lastCycle = 0;
byte runMode = 0;		// see notes below 
byte state = 0;		// The state contains the active on and off states of the outputs, based on temperature and mode. - | - | - | - | - | cool | heat | fan
byte settings;
double menu = 0;
double heatTarget = DEFAULTHEAT;
double coolTarget = DEFAULTCOOL;

const char *message[4][5]  = 
{
	{"status          ", "                ", "                " },
	{"state           ", "Set cooling     ", "Set heating     " },
	{"Current   Target", "target          ", "target          " },
	{""                , ""                , ""                 }
};

const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns
char keys[ROWS][COLS] = {
{'1','2','3'},
{'4','5','6'},
{'7','8','9'},
{'*','0','#'}
};
byte rowPins[ROWS] = {3, 4, 5, 6}; //connect to the row pinouts of the kpd
byte colPins[COLS] = {7, 8, 9}; //connect to the column pinouts of the kpd

unsigned long loopCount;
unsigned long startTime;
String msg;

byte degC[8] = {	  
  0b11100,
  0b10100,
  0b11100,
  0b00000,
  0b00110,
  0b01000,
  0b01000,
  0b00110
  };


/*

The modes are as follow:

0 - off
1 - fan only
2 - cooling only 
3 - heating only 
4 - automatically heat or cool 

*/




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
		unsigned long restartDelay;
		unsigned long minimumCycle;
		boolean isActive = 0;
		boolean isCalledActive = 0;
		boolean isCalledInactive = 0;
		byte pin;
		
	Output(unsigned long activationDelay, unsigned long deactivationDelay, unsigned long restartDelay, unsigned long minimumCycle, byte pin)
	{
		this->currTime = currTime;
		this->restartDelay = restartDelay;
		this->minimumCycle = minimumCycle;
		this->pin = pin;
		this->activationDelay = activationDelay;
		this->deactivationDelay = deactivationDelay;
		pinMode(pin, OUTPUT);
	}
	
	void setState(unsigned long currTime, boolean callState)
	{
		this->currTime = currTime;
		
		if(callState)
		{
			if(isCalledActive == 0)
			{
				isCalledActive = 1;
				isCalledInactive = 0;
				lastCalledActive = currTime;
			}
		}
		else
		{
			if(isCalledInactive == 0)
			{
				isCalledInactive = 1;
				isCalledActive = 0;
				lastCalledInactive = currTime;
			}
		}
	}
	
	void update(unsigned long currTime)
	{
		this->currTime = currTime;
		
		if(isActive == 0)
		{
			if(isCalledActive == 1)
			{
				if( currTime - lastDeactivated > restartDelay )
				{
					if(currTime - lastCalledActive > activationDelay )
					{
						isActive = 1;
						isCalledActive = 0;
						lastActivated = currTime;
						digitalWrite(pin, isActive);
					}
					
				}
			}
		}
		else
		{
			if(isCalledInactive == 1)
			{
				if( currTime - lastActivated > minimumCycle )
				{
					if( currTime - lastCalledInactive > deactivationDelay )
					{
						isActive = 0;
						isCalledInactive = 0;
						lastDeactivated = currTime;
						digitalWrite(pin, isActive);
					}
				}
			}
		}
	}
};

class Input
{
	private:
		double sumValue = 0;
		int history[];
	
	public:
		double avgValue = 0;
		double modeValue = 0;
		int smoothing;
		byte pin;
	
	Input(int smoothing, byte pin)
	{
		//delay(1000);
		this->smoothing = smoothing;
		this->pin = pin;
		// int history[smoothing];
		sumValue = 0;
		pinMode(pin, INPUT);
		// Serial << "Initializing history array of length " << smoothing << "\n";
		// for(int x = 0; x < smoothing; x++)
		// {
			// history[x] = analogRead(pin);
			// sumValue +=history[x];
			// Serial << "Value " << history[x] << " is in slot " << x << "\n";
		// }
		// avgValue = sumValue / smoothing;
		// modeValue = mode(history,smoothing);
		
		avgValue = analogRead(pin);
	}
	
	void update()
	{
		// sumValue = 0;
		// for(int x = 0; x < smoothing - 1; x++ )
		// {
			// history[x] = history [x + 1];
			// sumValue += history[x];
			// Serial << history[x] << " | ";
			// if( x == (smoothing - 1) )
				// Serial << " . \n";
		// }
		// history[smoothing] = analogRead(pin);
		
		// avgValue = sumValue / smoothing;
		//modeValue = mode(history,smoothing);
		avgValue = ( (smoothing-1)*avgValue + analogRead(pin) ) / smoothing;
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
	
Output compressor(COMPRESSORSTARTDELAY, COMPRESSORSTOPDELAY, COOLRESTARTDELAY, MINCOMPRESSORCYCLETIME, COMPRESSORPIN);
Output heater(HEATSTARTDELAY, HEATSTOPDELAY, HEATRESTARTDELAY, MINHEATCYCLETIME, HEATPIN);
Output fan(FANSTARTDELAY, FANSTOPDELAY, FANRESTARTDELAY, MINFANCYCLETIME, FANPIN);

Input temperature(ROLLINGAVERAGECOUNT, TEMPSENSE);
Input proximity(PROXIMITYROLLINGAVERAGE, PROXIMITYSENSE);

Backlight backlight(BACKLIGHTPIN, RAMPTIME);
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address and pin mapping
Keypad kpd = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

void setup()
{
	
	
	
	msg = "";
	runMode = 4;
	
	lcd.begin(16,4);
	lcd.createChar(0, degC);
	
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
	else if( proximity.avgValue > PROXIMITYTHRESHHOLD )
	{
		backlight.setTarget(currTime, PROXIMITYBRIGHTNESS);		
	}
	else if(backlight.target != 0)
	{
		backlight.setTarget(currTime, 0);
	}
}

void determineState( double temperature )
{
	
	if( runMode == 0 )
	{
		state = B00000000;
	}
	else if( runMode == 1 )
	{
		state = B00000001;
	}
	else if( runMode == 2 ) 
	{
		if( temperature - COOLTEMPRANGE > coolTarget )
		{
			state = B00000101;
		}
		else if( temperature < coolTarget )
		{
			state = B00000000;
		}
	}
	else if ( runMode == 3 )
	{
		if( temperature + HEATTEMPRANGE < heatTarget )
		{
			state = B00000011;
		}
		else if( temperature > heatTarget )
		{
			state = B00000000;
		}
	}
	else if( runMode == 4 )
	{
		if( temperature + HEATTEMPRANGE < heatTarget )
		{
			state = B00000011;
		}
		else if( temperature - COOLTEMPRANGE > coolTarget )
		{
			state = B00000101;
		}
		else if( temperature > heatTarget && temperature < coolTarget )
		{
			state = B00000000;
		}
	}
}

void setOutputs(unsigned long currTime)
{
	if( (state & B00000001) == B00000001 )
	{
		fan.setState(currTime, 1);
	}
	else
	{
		fan.setState(currTime, 0);
	}
	
	if( (state & B00000100) == B00000100 )
	{
		compressor.setState(currTime, 1);
	}
	else
	{
		compressor.setState(currTime, 0);
	}
	
	if( (state & B00000010) == B00000010 )
	{
		heater.setState(currTime, 1);
	}
	else
	{
		heater.setState(currTime, 0);
	}
	
}

void display()
{
	double a = 4561.123456789;
	char number1[6], number2[6];
	for(int y = 0; y<4; y++)
	{
		lcd.setCursor(0,y);
		lcd.print(message[y][int(menu)]);
		
	}
	if(true)
	{
		char numbers[17] = "||||old||data|||";
		
		sprintf( numbers, "%7.1f  %7.1f%s", temperature.avgValue, heatTarget, byte(0) );
		lcd.setCursor(0,3);
		lcd << numbers;
		

	}
		
	
}

void readKeys()
{
	if(kpd.getKeys())
    {
		buttonTime = currTime;
        
		for (int i=0; i<LIST_MAX; i++)   // Scan the whole key list.
        {
            if ( kpd.key[i].stateChanged )   // Only find keys that have changed state.
            {
				if( kpd.key[i].kstate == PRESSED )
				{
					char key = kpd.key[i].kchar;
					Serial << key;
					switch(key)
					{
						case '4':
						if(menu == 2)
						{
							menu = 0;
						}
						else
						{
							menu++;
						}
						break;
						
						case '6':
						if(menu == 0)
						{
							menu = 2;
						}
						else
						{
							menu--;
						}
						
						case '8':
						if(menu == 1)
						{
							coolTarget += 0.1;
						}
						if(menu == 2)
						{
							heatTarget += 0.1;
						}
						break;
						
						case '2':
						if(menu == 1)
						{
							coolTarget -= 0.1;
						}
						if(menu == 2)
						{
							heatTarget -= 0.1;
						}
						break;
					}
				}
            }
		}
    }
}

void loop()
{
	currTime = millis();
	
	
	
	if(currTime - lastCycle > CYCLEDELAY)
	{
		lastCycle = currTime;
		currTemp = temperature.avgValue * TEMPCONVERSION;
		
		compressor.update(currTime);
		heater.update(currTime);
		fan.update(currTime);
		
		temperature.update();
		//proximity.update();
		determineState( currTemp );
			
		setOutputs(currTime);	
	}
	
	
	
	setBacklight( currTime );
	backlight.update(currTime);
	readKeys();
	display();
	
	
	

	
	
}