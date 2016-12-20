#include <Wire.h> 
#include <LCD.h>
#include <LiquidCrystal_I2C.h>

template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; }

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address
#define BACKLIGHT_PIN     3

int letter = 0;

void setup()
{

  // Switch on the backlight
  pinMode ( BACKLIGHT_PIN, OUTPUT );
  analogWrite ( BACKLIGHT_PIN, 100 );
  
  lcd.begin(16,4);

}

void printletter()
{
  
  lcd << letter << ": " << char(letter) << "  ";
  letter++;
  delay(150);
}

void loop()
{
  for(int x = 0; x < 9; x+=8)
  {
    for(int y = 0; y < 4; y++)
    {
      lcd.setCursor(x,y);
      printletter();
      
    }
  }
}
