/*
Temperature Sensor
Displayed on I2C LCD Display
*/

/*-----( Import needed libraries )-----*/
#include <Wire.h>
#include "RTClib.h"
#include "LiquidCrystal.h"
#include <SD.h>
#include <EEPROM.h>

//const char *dayName[] =
// { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}; // not used
const char *monthName[12] =
{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

byte thermometer[8] = //icon for thermometer
{
	B01110,
	B01010,
	B01010,
	B01110,
	B01110,
	B01110,
	B01110,
	B01110
};

byte waterDroplet[8] = //icon for water droplet
{
	B00100,
	B00100,
	B01010,
	B01010,
	B10001,
	B10001,
	B10001,
	B01110,
};

/*-----( Declare objects )-----*/
// Connect via i2c, default address #0 (A0-A2 not jumpered)
LiquidCrystal lcd(0);  // Set the LCD I2C address

/*-----( Declare Constants, Pin Numbers )-----*/

DateTime tm;

File myFile;

#define NUMBUTTONS 3
byte buttons[NUMBUTTONS] = {2, 3, 4};
byte buttonState[NUMBUTTONS];             // the current reading from the input pin
byte lastButtonState[NUMBUTTONS] = {LOW, LOW, LOW};   // the previous reading from the input pin
// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long lastDebounceTime[NUMBUTTONS] = {0, 0, 0};  // the last time the output pin was toggled
long debounceDelay = 50;    // the debounce time; increase if the output flickers
boolean button1down = false;
long button1counter = 0;

boolean logging = false;
int LoggingInterval = 5;

const int numReadings = 10;
int temperature1[numReadings];
int temperature2[numReadings];
int sunlight[numReadings];
int temperature1Total = 0;
int temperature2Total = 0;
int sunlightTotal = 0;
int index = 0;
int temperature1Average;
int temperature2Average;
int sunlightAverage;
int P1On;
int P1Off;
int P2On;
int P2Off;
unsigned long readingTime = 0;

#define minSetpoint 20
#define maxSetpoint 70
int setpointTemperature = 0;

boolean elementOn = false;
boolean cooling = false;
boolean override = false;
boolean overrideDetected = false;
long overrideTime = 1600;

// EEPROM addresses:
#define schema 101
const int schemaAddress = 3;
const int spTempAddress = 0;
const int bLightAddress = 1;
const int P1OnAddressRight = 4;
const int P1OnAddressLeft = 5;
const int P1OffAddressRight = 6;
const int P1OffAddressLeft = 7;
const int P2OnAddressRight = 8;
const int P2OnAddressLeft = 9;
const int P2OffAddressRight = 10;
const int P2OffAddressLeft = 11;

// BackLight control:
#define bLightTimeout 20000
unsigned long bLightTime = 0;
boolean bLightAutoOff = true;

// Menu:
const int menuTimeout = 8000;
unsigned long menuTime = 0;
int menuState = 0;
boolean changeMenu = false;
int changeSetting = 0;
// Put the menu items here
char* menuItems[] =
{
	"Max temperature:",
	"BLight auto off:",
	"Date and time:  "
};

RTC_DS1307 rtc;

void setup()   /*----( SETUP: RUNS ONCE )----*/
{
	lcd.begin(16, 2);        // initialize the lcd for 16 chars 2 lines, turn on backlight
	lcd.setBacklight(HIGH);
	lcd.clear();
	lcd.createChar(1, thermometer);
	lcd.createChar(2, waterDroplet);

	//buttons:
	pinMode(buttons[0], INPUT);
	pinMode(buttons[1], INPUT);
	pinMode(buttons[2], INPUT);

	//output pin
	pinMode(5, OUTPUT);

	for(int i = 0; i < 3; i++)
	{
		buttonState[i] = 0;
	}

	// initialise EEPROM on first-time startup
	if (EEPROM.read(schemaAddress) == schema)
	{
		// Temperature setpoint:
		setpointTemperature = EEPROM.read(spTempAddress);
		// Backlight control:
		bLightAutoOff = EEPROM.read(bLightAddress) == 1;
		
		int leftBits = EEPROM.read(P1OnAddressLeft);
		P1On = (leftBits << 8) | EEPROM.read(P1OnAddressRight);

                if ((int(P1On) > 1440) || (int(P1On) < 0))
                  P1On = 0;
		
		leftBits = EEPROM.read(P1OffAddressLeft);
		P1Off = (leftBits << 8) | EEPROM.read(P1OffAddressRight);
		
                if ((int(P1Off) > 1440) || (int(P1Off) < 0))
                  P1Off = 0;
		
		leftBits = EEPROM.read(P2OnAddressLeft);
		P2On = (leftBits << 8) | EEPROM.read(P2OnAddressRight);

                if ((int(P2On) > 1440) || (int(P2On) < 0))
                  P2On = 0;
		
		leftBits = EEPROM.read(P2OffAddressLeft);
		P2Off = (leftBits << 8) | EEPROM.read(P2OffAddressRight);

                if ((int(P2Off) > 1440) || (int(P2Off) < 0))
                  P2Off = 0;
		
	}
	else
	{
		setpointTemperature = 50;
		bLightAutoOff = true;
		EEPROM.write(schemaAddress, schema);
		EEPROM.write(spTempAddress, setpointTemperature);
		EEPROM.write(bLightAddress, bLightAutoOff);
		
		EEPROM.write(P1OnAddressLeft, byte(P1On >> 8));
		EEPROM.write(P1OnAddressRight, byte(P1On));

		EEPROM.write(P1OffAddressLeft, byte(P1Off >> 8));
		EEPROM.write(P1OffAddressRight, byte(P1Off));

		EEPROM.write(P2OnAddressLeft, byte(P2On >> 8));
		EEPROM.write(P2OnAddressRight, byte(P2On));

		EEPROM.write(P2OffAddressLeft, byte(P2Off >> 8));
		EEPROM.write(P2OffAddressRight, byte(P2Off));
	}

	InitialiseSdCard();

	bLightTime = millis();
}/*--(end setup )---*/

void InitialiseSdCard()
{
	pinMode(10, OUTPUT);
	lcd.setCursor(0, 0);
	if (!SD.begin(10))
	{
		lcd.print("SD Ini failed!  ");
		delay(1500);
	}
	else
	{
		myFile = SD.open("log.csv", FILE_WRITE);

		if (myFile)
		{
			myFile.println("Date & Time,Geyser,Ambient,Power,Sunlight");
			lcd.print("SD Ini success! ");
			delay(1500);
		}
		else
		{
			lcd.print("SD Ini failed!  ");
			delay(1500);
		}
		myFile.close();
	}
}

void DisplayTemperature()
{
	lcd.noBlink();
	lcd.setCursor(9, 0);
	lcd.print(" ");
	lcd.print(" ");
	lcd.write(2);
	lcd.setCursor(12, 0);

	if (temperature1Average < 10)
	{
		lcd.print(" ");
	}
	lcd.print(temperature1Average);

	lcd.print((char)223); //degree sign
	lcd.print("C ");


	lcd.setCursor(4, 1);
	if (sunlightAverage < 10)
	{
		lcd.print("   ");
	}
	else if (sunlightAverage < 100)
	{
		lcd.print("  ");
	}
	else if (sunlightAverage < 1000)
	{
		lcd.print(" ");
	}
	lcd.print(sunlightAverage);
	lcd.print("mW ");

	lcd.setCursor(11, 1);
	lcd.write(1);
	lcd.setCursor(12, 1);

	if (temperature1Average < 10)
	{
		lcd.print(" ");
	}
	//lcd.print(temperature2Average);
	lcd.print(setpointTemperature);

	lcd.print((char)223); //degree sign
	lcd.print("C ");
}

void DisplayDateTime(byte line, DateTime time)
{

	lcd.setCursor(0, line - 1);
	

	if (menuState < 7)
	{
		if (time.hour() < 10)
		lcd.print("0");
		lcd.print(time.hour());
	}
	lcd.print(":");
	if (time.minute() < 10)
	lcd.print("0");
	lcd.print(time.minute());
	if (line == 1)
	{
		lcd.print("    ");
	}
	else
	{
		lcd.print(" ");
		if (time.day() < 10)
		lcd.print("0");
		lcd.print(time.day());
		lcd.print(" ");
		lcd.print(monthName[time.month() - 1]);
		lcd.print(" ");
		lcd.print(time.year());
		lcd.print(" ");
	}
}

void DisplayMenuItem()
{
	lcd.setCursor(0, 0);
	if (menuState <= 3)
	{
		lcd.print(menuItems[menuState - 1]);
	}
	// For setting P1On, P1Off, P2On and P2Off I don't use the menu index
}

void DisplaySetting()
{
	lcd.setCursor(0, 1);
	lcd.print(" ");
	if (menuState == 1)
	{
		String temp = String(setpointTemperature);
		lcd.print(temp);
		lcd.print(" ");
		lcd.print((char)223); //degree sign
		lcd.print("C          ");
	}
	else if (menuState == 2)
	{
		if (bLightAutoOff)
		{
			lcd.print("on              ");
		}
		else
		{
			lcd.print("off             ");
		}
	}
	else if ((menuState >= 3) && (menuState < 8))
	{
		DateTime t = rtc.now();
		DisplayDateTime(2, t);
		if (menuState == 3)
		lcd.setCursor(1, 1);
		else if (menuState == 4)
		lcd.setCursor(4, 1);
		else if (menuState == 5)
		lcd.setCursor(7, 1);
		else if (menuState == 6)
		lcd.setCursor(9, 1);
		else if (menuState == 7)
		lcd.setCursor(14, 1);
		lcd.blink();
	}
	else if ((menuState >= 8) && (menuState < 12))
	{
		lcd.setCursor(0, 0);
		lcd.print("P1 on: ");
		lcd.print(GetTime(P1On));
		lcd.print("    ");
		lcd.setCursor(0, 1);
		lcd.print("P1 off: ");
		lcd.print(GetTime(P1Off));
		lcd.print("   ");
		if (menuState == 8)
		lcd.setCursor(8, 0);
		else if (menuState == 9)
		lcd.setCursor(11, 0);
		else if (menuState == 10)
		lcd.setCursor(9, 1);
		else if (menuState == 11)
		lcd.setCursor(12, 1);
		lcd.blink();
	}
	else
	{
		lcd.setCursor(0, 0);
		lcd.print("P2 on: ");
		lcd.print(GetTime(P2On));
		lcd.print("    ");
		lcd.setCursor(0, 1);
		lcd.print("P2 off: ");
		lcd.print(GetTime(P2Off));
		lcd.print("   ");
		if (menuState == 12)
		lcd.setCursor(8, 0);
		else if (menuState == 13)
		lcd.setCursor(11, 0);
		else if (menuState == 14)
		lcd.setCursor(9, 1);
		else if (menuState == 15)
		lcd.setCursor(12, 1);
		lcd.blink();
	}
}

void DisplayStatus(int temperature, int setpoint)
{
	String txt;
	if (override)
	{
		txt = "OVR ";
	}
	else if (elementOn)
	{
		txt = "ON  ";
	}
	else
	{
		txt = "OFF ";
	}

	static int blinkInterval = millis();
	static boolean blinkOn = false;
	
	boolean blink = (cooling || (temperature > setpoint));
	
	lcd.setCursor(0, 1);
	
	if (blink)
	{
		if ((millis() - blinkInterval) > 800)
		{
			if (blinkOn)
			{
				lcd.print("    ");
				blinkOn = false;
			}
			else
			{
				lcd.print(txt);
                                blinkOn = true;
			}
		
			blinkInterval = millis();
		}
	}
	else
	{
		lcd.print(txt);
	}
}

void ChangeSetting()
{
	if (menuState == 1)
		ChangeSetPointTemperature(changeSetting);
	else if (menuState == 2)
		bLightAutoOff = !bLightAutoOff;
	else if ((menuState >= 3) && (menuState < 8))
		ChangeTime(changeSetting);
	else if (menuState == 8)
		P1On = ChangePeriodTime(P1On, changeSetting*60); // changing hours
	else if (menuState == 9)
		P1On = ChangePeriodTime(P1On, changeSetting);    // changing minutes
	else if (menuState == 10)
		P1Off = ChangePeriodTime(P1Off, changeSetting*60);
	else if (menuState == 11)
		P1Off = ChangePeriodTime(P1Off, changeSetting);
	else if (menuState == 12)
		P2On = ChangePeriodTime(P2On, changeSetting*60);
	else if (menuState == 13)
		P2On = ChangePeriodTime(P2On, changeSetting);
	else if (menuState == 14)
		P2Off = ChangePeriodTime(P2Off, changeSetting*60);
	else if (menuState == 15)
		P2Off = ChangePeriodTime(P2Off, changeSetting);
}

void SaveSettings()
{
	EEPROM.write(spTempAddress, setpointTemperature);
	EEPROM.write(bLightAddress, (int)bLightAutoOff);

	EEPROM.write(P1OnAddressLeft, byte(P1On >> 8));
	EEPROM.write(P1OnAddressRight, byte(P1On));

	EEPROM.write(P1OffAddressLeft, byte(P1Off >> 8));
	EEPROM.write(P1OffAddressRight, byte(P1Off));

	EEPROM.write(P2OnAddressLeft, byte(P2On >> 8));
	EEPROM.write(P2OnAddressRight, byte(P2On));

	EEPROM.write(P2OffAddressLeft, byte(P2Off >> 8));
	EEPROM.write(P2OffAddressRight, byte(P2Off));
}

void ChangeSetPointTemperature(int changeValue)
{
	cooling = false;

	setpointTemperature = setpointTemperature + changeValue;
	if (setpointTemperature <= minSetpoint)
	{
		setpointTemperature = minSetpoint;
	}
	else if (setpointTemperature >= maxSetpoint)
	{
		setpointTemperature = maxSetpoint;
	}
}

void ChangeTime(int changeValue)
{
	tm = rtc.now();

	if (menuState == 3)
	{
		if ((tm.hour() == 0) && (changeValue < 0))
		rtc.adjust(DateTime(tm.year(), tm.month(), tm.day(), 23, tm.minute(), tm.second()));
		else if ((tm.hour() == 23) && (changeValue > 0))
		rtc.adjust(DateTime(tm.year(), tm.month(), tm.day(), 0, tm.minute(), tm.second()));
		else
		rtc.adjust(DateTime(tm.year(), tm.month(), tm.day(), tm.hour() + changeValue, tm.minute(), tm.second()));
	}
	else if (menuState == 4)
	{
		if ((tm.minute() == 0) && (changeValue < 0))
		rtc.adjust(DateTime(tm.year(), tm.month(), tm.day(), tm.hour(), 59, tm.second()));
		else if ((tm.minute() == 59) && (changeValue > 0))
		rtc.adjust(DateTime(tm.year(), tm.month(), tm.day(), tm.hour(), 0, tm.second()));
		else
		rtc.adjust(DateTime(tm.year(), tm.month(), tm.day(), tm.hour(), tm.minute() + changeValue, tm.second()));
	}
	else if (menuState == 5)
	{
		if ((tm.day() == 1) && (changeValue < 0))
		rtc.adjust(DateTime(tm.year(), tm.month(), 31, tm.hour(), tm.minute(), tm.second()));
		else if ((tm.day() == 31) && (changeValue > 0))
		rtc.adjust(DateTime(tm.year(), tm.month(), 1, tm.hour(), tm.minute(), tm.second()));
		else
		rtc.adjust(DateTime(tm.year(), tm.month(), tm.day() + changeValue, tm.hour(), tm.minute(), tm.second()));
	}
	else if (menuState == 6)
	{
		if ((tm.month() == 1) && (changeValue < 0))
		rtc.adjust(DateTime(tm.year(), 12, tm.day(), tm.hour(), tm.minute(), tm.second()));
		else if ((tm.month() == 12) && (changeValue > 0))
		rtc.adjust(DateTime(tm.year(), 1, tm.day(), tm.hour(), tm.minute(), tm.second()));
		else
		rtc.adjust(DateTime(tm.year(), tm.month() + changeValue, tm.day(), tm.hour(), tm.minute(), tm.second()));
	}
	else if (menuState == 7)
	{
		rtc.adjust(DateTime(tm.year() + changeValue, tm.month(), tm.day(), tm.hour(), tm.minute(), tm.second()));
	}
}

int ChangePeriodTime(int oldTime, int changeValue)
{
	int newValue = oldTime + changeValue;

	if (newValue >= 1440)
		return newValue - 1440;
	else if (newValue <= -1)
		return 1440 + newValue;
	else
		return newValue;  
}

String GetTime(int time)
{
	int hours = time/60;
	String result = String(hours);

	if (hours < 10)
	result = "0" + result;
	
	int minutes = time%60;

	if (minutes < 10)
	result = result + ":0" + String(minutes);
	else
	result = result + ":" + String(minutes); 
	
	return result;
}

void ElementOn(boolean on)
{
	if (on)
	{
		elementOn = true;
		cooling = false;
		digitalWrite(5, HIGH);
	}
	else
	{
		elementOn = false;
		digitalWrite(5, LOW);
	}
}

boolean SwitchOnIfUnderTemp(int temperature, int setpoint)
{
	if (temperature < setpoint)
	{
		if (cooling)
		{
			if (temperature < (setpoint - 6))
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return true;
		}
	}
	else
	{
		if (temperature > (setpoint - 6))
		{
			cooling = true;
		}
		
		return false;
	}
}

int GetTemperature(int channel)
{
	//getting the voltage reading from the temperature sensor
	int reading = analogRead(channel);
	// converting that reading to voltage, for 5v arduino
	float voltage = reading * 5 / 1.024;
	// convert mV to temperature
	return voltage / 10 + 2;
}

void Log(DateTime time, int geyserTemp, int ambientTemp, int solarRadiation)
{
	pinMode(10, OUTPUT);
	myFile = SD.open("log.csv", FILE_WRITE);

	lcd.setCursor(0, 0);

	if (myFile)
	{
		myFile.print(time.year());
		myFile.print('/');
		if (time.month() < 10)
		myFile.print("0");
		myFile.print(time.month());
		myFile.print('/');
		if (time.day() < 10)
		myFile.print("0");
		myFile.print(time.day());
		myFile.print(' ');
		if (time.hour() < 10)
		myFile.print("0");
		myFile.print(time.hour());
		myFile.print(':');
		if (time.minute() < 10)
		myFile.print("0");
		myFile.print(time.minute());
		myFile.print(':');
		if (time.second() < 10)
		myFile.print("0");
		myFile.print(time.second());
		myFile.print(",");
		myFile.print(geyserTemp);
		myFile.print(",");
		myFile.print(ambientTemp);
		myFile.print(",");
		if (elementOn)
		{
			myFile.print("on");
		}
		else
		{
			myFile.print("off");
		}
		myFile.print(",");
		myFile.println(solarRadiation);

		lcd.print("logging");
	}
	else
	{
		lcd.setCursor(0, 0);
		lcd.print("logging error");
	}
	myFile.close();
	delay(1500);
}

boolean checkButtons()
{
	boolean result = false;
	
	int  reading[NUMBUTTONS];

	for (byte i = 0; i < NUMBUTTONS; i++)
	{
		reading[i] = digitalRead(buttons[i]);
	}

	// check to see if you just pressed the button
	// (i.e. the input went from LOW to HIGH),  and you've waited
	// long enough since the last press to ignore any noise:

	for (byte i = 0; i < NUMBUTTONS; i++)
	{
		// If the switch changed, due to noise or pressing:
		if (reading[i] != lastButtonState[i])
		{
			// reset the debouncing timer
			lastDebounceTime[i] = millis();
		}
	}

	for (byte i = 0; i < NUMBUTTONS; i++)
	{
		if ((millis() - lastDebounceTime[i]) > debounceDelay)
		{
			// whatever the reading is at, it's been there for longer
			// than the debounce delay, so take it as the actual current state:

			// if the button state has changed:
			if (reading[i] != buttonState[i])
			{
				
				result = true;
				
				buttonState[i] = reading[i];

				menuTime = millis();

				if (buttonState[0] == HIGH)
				{
					button1down = true;
				}
				else if ((buttonState[0] == LOW) && button1down)
				{
					button1down = false;
					if (overrideDetected)
					{
						overrideDetected = false;
					}
					else
					{
						changeMenu = true;
					}
				}


				if (buttonState[1] == HIGH)
				{
					changeSetting = 1;
				}

				if (buttonState[2] == HIGH)
				{
					changeSetting = -1;
				}

				bLightTime = millis();
			}
		}
	}

	for (byte i = 0; i < NUMBUTTONS; i++)
	{
		// save the reading.  Next time through the loop,
		// it'll be the lastButtonState:
		lastButtonState[i] = reading[i];
	}
	
	return result;
}

void loop()   /*----( LOOP: RUNS CONSTANTLY )----*/
{
	static boolean onIfUnderTemp = false;
	static boolean backlight = true;
	
	DateTime tm = rtc.now();
	
	if (((millis() - readingTime) > 1000) || (readingTime > millis()))
	{
		// subtract the last reading:
		temperature1Total = temperature1Total - temperature1[index];
		temperature2Total = temperature2Total - temperature2[index];
		sunlightTotal = sunlightTotal - sunlight[index];
		// read from the sensor
		temperature1[index] = GetTemperature(A0);
		temperature2[index] = GetTemperature(A1);
		sunlight[index] = analogRead(A2);
		// add the reading to the total:
		temperature1Total = temperature1Total + temperature1[index];
		temperature2Total = temperature2Total + temperature2[index];
		sunlightTotal = sunlightTotal + sunlight[index];
		// advance to the next position in the array:
		index++;
		// if we're at the end of the array...
		if (index >= numReadings)
		// ...wrap around to the beginning:
		index = 0;

		readingTime = millis();

		temperature1Average = temperature1Total / numReadings;
		temperature2Average = temperature2Total / numReadings;
		sunlightAverage = sunlightTotal / numReadings;

		if (menuState == 0)
		{
			DisplayDateTime(1, tm);
			DisplayTemperature();
			DisplayStatus(temperature1Average, setpointTemperature);
		}
	}
	
	if (((millis() - menuTime) > menuTimeout) || (menuTime > millis()))
	{
		menuState = 0;
	}

	if (bLightAutoOff && (((millis() - bLightTime) > bLightTimeout) || (bLightTime > millis())))
	{
		lcd.setBacklight(LOW);
		backlight = false;
	}

	if (changeMenu)
	{
		changeMenu = false;
		
                if (!backlight)
                {
		  backlight = true;
                }
                else
                {
		  menuState++;
		  DisplayMenuItem();
		  DisplaySetting();
		  if (menuState > 15)
		  {
			menuState = 0;
		  }
                }
	}
	else if ((menuState > 0) && (changeSetting != 0))
	{
		ChangeSetting();
		DisplaySetting();
		SaveSettings();

		changeSetting = 0;
	}

	if (checkButtons())
	{
		lcd.setBacklight(HIGH);
	}

	if (override)
	{
		ElementOn(SwitchOnIfUnderTemp(temperature1Average, setpointTemperature));
	}
	else
	{
		int timeNow = tm.hour()*60 + tm.minute();

		if (((P1On <= timeNow) && (P1Off > timeNow)) || ((P2On <= timeNow) && (P2Off > timeNow)))
		{
			ElementOn(SwitchOnIfUnderTemp(temperature1Average, setpointTemperature));
		}
		else
		{
			if (elementOn && (temperature1Average > (setpointTemperature - 6)))
			{
				cooling = true;
			}
			
			ElementOn(false);
			
			onIfUnderTemp = false;
		}
	}

	if ((tm.minute() % LoggingInterval == 0) && !logging)
	{
		if (menuState < 3) // not setting the time
		{
			logging = true;
			Log(tm, temperature1Average, temperature2Average, sunlightAverage);
		}
	}
	if (!(tm.minute() % LoggingInterval == 0) && logging)
	{
		logging = false;
	}

	// override control:
	if (!button1down)
	{
		button1counter = millis();
	}

	if (((millis() - button1counter) > overrideTime) && !overrideDetected)
	{
		override = !override;
		button1counter = millis();
		overrideDetected = true;
	}
}
/* ( THE END ) */

