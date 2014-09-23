/*
   Temperature Sensor
   Displayed on I2C LCD Display
*/

/*-----( Import needed libraries )-----*/
#include <Wire.h>
#include <Time.h>
#include <DS1307RTC.h>
#include <LiquidCrystal.h>
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

tmElements_t tm;

File myFile;

#define NUMBUTTONS 3
byte buttons[NUMBUTTONS] = {2, 3, 4};
byte buttonState[NUMBUTTONS];             // the current reading from the input pin
byte lastButtonState[NUMBUTTONS] = {LOW, LOW, LOW};   // the previous reading from the input pin
// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long lastDebounceTime[NUMBUTTONS] = {0, 0, 0};  // the last time the output pin was toggled
long debounceDelay = 50;    // the debounce time; increase if the output flickers
boolean button1down = true;
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

// BackLight control:
#define bLightTimeout 20000
unsigned long bLightTime = 0;
boolean bLightAutoOff = true;
boolean backlightOn = false;

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
	"Date and time:  ",
	""
};

void setup()   /*----( SETUP: RUNS ONCE )----*/
{
	lcd.begin(16, 2);        // initialize the lcd for 16 chars 2 lines, turn on backlight
	SetBacklight(true);
//SetBacklight(false);
	lcd.clear();
	lcd.createChar(1, thermometer);
	lcd.createChar(2, waterDroplet);

	if (!RTC.read(tm))
	{
		if (!RTC.chipPresent())
		{
			lcd.setCursor(0, 0);
			lcd.print("Clock read error");
			delay(3000);
		}
		else
		{
			setDefaultTime();
			lcd.setCursor(0, 0);
			lcd.print("                ");
			lcd.setCursor(0, 0);
			lcd.print("Default time");
			lcd.print(tm.Year);
			delay(2000);
		}
	}
	else if ((tm.Year > 50) || (tm.Year < 0))
	{
		setDefaultTime();
		lcd.setCursor(0, 0);
		lcd.print("                ");
		lcd.setCursor(0, 0);
		lcd.print("Default time");
		lcd.print(tm.Year);
		delay(2000);
	}

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
	}
	else
	{
		setpointTemperature = 50;
		bLightAutoOff = true;
		EEPROM.write(schemaAddress, schema);
		EEPROM.write(spTempAddress, setpointTemperature);
		EEPROM.write(bLightAddress, bLightAutoOff);
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

void SetBacklight(boolean on)
{
	if (on)
	{
		lcd.setBacklight(HIGH);
	}
	else
	{
		lcd.setBacklight(LOW);
	}

	backlightOn = on;
}

void setDefaultTime()
{
	tm.Hour = 0;
	tm.Minute = 0;
	tm.Second = 0;
	tm.Day = 1;
	tm.Month = 1;
	tm.Year = 2000;
	if (!RTC.write(tm))
	{
		lcd.setCursor(0, 0);
		lcd.print("Clk write error!");
		delay(3000);
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
	lcd.print(temperature2Average);

	lcd.print((char)223); //degree sign
	lcd.print("C ");
}

void DisplayDateTime(byte line)
{

	lcd.setCursor(0, line - 1);

	if(RTC.read(tm))
	{
		if (tm.Hour < 10)
			lcd.print("0");
		lcd.print(tm.Hour);
		lcd.print(":");
		if (tm.Minute < 10)
			lcd.print("0");
		lcd.print(tm.Minute);
		if (line == 1)
		{
			lcd.print("    ");
		}
		else
		{
			lcd.print(" ");
			if (tm.Day < 10)
				lcd.print("0");
			lcd.print(tm.Day);
			lcd.print(" ");
			lcd.print(monthName[tm.Month - 1]);
			lcd.print(" ");
			lcd.print(tmYearToCalendar(tm.Year) - 2000);
		}

// lcd.setCursor(12,1); // this code is used for displaying day of the week
//  lcd.print(tm.Wday[zile-2]); //it's disabled because for some reason it doesn't work on i2c display
	}
	else
	{
		if (!RTC.chipPresent())
		{
			lcd.print("Clock read error!");
		}
	}
}

void DisplayMenuItem()
{
	if (menuState <= 3)
	{
		lcd.setCursor(0, 0);
		lcd.print(menuItems[menuState - 1]);
	}
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
	else if (menuState >= 3)
	{
		DisplayDateTime(2);
		if (menuState == 3)
		{
			lcd.setCursor(1, 1);
		}
		else if (menuState == 4)
		{
			lcd.setCursor(4, 1);
		}
		else if (menuState == 5)
		{
			lcd.setCursor(7, 1);
		}
		else if (menuState == 6)
		{
			lcd.setCursor(9, 1);
		}
		else if (menuState == 7)
		{
			lcd.setCursor(14, 1);
		}
		lcd.blink();
	}
}

void DisplayStatus()
{
	lcd.setCursor(0, 1);
	if (elementOn)
	{
		lcd.print("ON  ");
	}
	else
	{
		lcd.print("OFF ");
	}
}

void ChangeSetting()
{
	if (menuState == 1)
		ChangeSetPointTemperature(changeSetting);
	else if (menuState == 2)
		ChangeAutoBacklightOff(!bLightAutoOff);
	else if (menuState >= 3)
		ChangeTime(changeSetting);

	DisplaySetting();
}

void ChangeSetPointTemperature(int changeValue)
{
	setpointTemperature = setpointTemperature + changeValue;
	cooling = false;
	if (setpointTemperature <= minSetpoint)
	{
		setpointTemperature = minSetpoint;
	}
	else if (setpointTemperature >= maxSetpoint)
	{
		setpointTemperature = maxSetpoint;
	}
	EEPROM.write(spTempAddress, setpointTemperature);
}

void ChangeAutoBacklightOff(boolean value)
{
	bLightAutoOff = value;
	if (bLightAutoOff)
	{
		EEPROM.write(bLightAddress, 1);
	}
	else
	{
		EEPROM.write(bLightAddress, 0);
	}
}

void ChangeTime(int changeValue)
{
	RTC.read(tm);
	if (menuState == 3)
	{
		if ((tm.Hour == 0) && (changeValue < 0))
			tm.Hour = 23;
		else if ((tm.Hour == 23) && (changeValue > 0))
			tm.Hour = 0;
		else
			tm.Hour = tm.Hour + changeValue;
	}
	else if (menuState == 4)
	{
		if ((tm.Minute == 0) && (changeValue < 0))
			tm.Minute = 59;
		else if ((tm.Minute == 59) && (changeValue > 0))
			tm.Minute = 0;
		else
			tm.Minute = tm.Minute + changeValue;
	}
	else if (menuState == 5)
	{
		if ((tm.Day == 1) && (changeValue < 0))
			tm.Day = 31;
		else if ((tm.Day == 31) && (changeValue > 0))
			tm.Day = 1;
		else
			tm.Day = tm.Day + changeValue;
	}
	else if (menuState == 6)
	{
		if ((tm.Month == 1) && (changeValue < 0))
			tm.Month = 12;
		else if ((tm.Month == 12) && (changeValue > 0))
			tm.Month = 1;
		else
			tm.Month = tm.Month + changeValue;
	}
	else if (menuState == 7)
	{
		tm.Year = tm.Year + changeValue;
	}
	RTC.write(tm);
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

void Log(int geyserTemp, int ambientTemp, int solarRadiation)
{
	pinMode(10, OUTPUT);
	myFile = SD.open("log.csv", FILE_WRITE);

	lcd.setCursor(0, 0);

	if (myFile)
	{
		myFile.print(tmYearToCalendar(tm.Year));
		myFile.print('/');
		if (tm.Month < 10)
			myFile.print("0");
		myFile.print(tm.Month);
		myFile.print('/');
		if (tm.Day < 10)
			myFile.print("0");
		myFile.print(tm.Day);
		myFile.print(' ');
		if (tm.Hour < 10)
			myFile.print("0");
		myFile.print(tm.Hour);
		myFile.print(':');
		if (tm.Minute < 10)
			myFile.print("0");
		myFile.print(tm.Minute);
		myFile.print(':');
		if (tm.Second < 10)
			myFile.print("0");
		myFile.print(tm.Second);
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

void myDelay(unsigned long duration)
{
	unsigned long start = millis();

	while (millis() - start <= duration)
	{
		checkButtons();  // check the buttons
	}
}

void checkButtons()
{
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

				buttonState[i] = reading[i];

				menuTime = millis();

				if ((!backlightOn) && (buttonState[0] == HIGH))
				{
					SetBacklight(true);
				}
				else
				{
					if (buttonState[0] == HIGH)
					{
						button1down = true;
					}
					else if ((buttonState[0] == LOW) && button1down)
					{
						button1down = false;
						changeMenu = !overrideDetected;
						overrideDetected = false;
						SetBacklight(true);
					}
				}


				if (buttonState[1] == HIGH)
				{
					changeSetting = 1;
					SetBacklight(true);
				}

				if (buttonState[2] == HIGH)
				{
					changeSetting = -1;
					SetBacklight(true);
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
}

void loop()   /*----( LOOP: RUNS CONSTANTLY )----*/
{
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
			DisplayDateTime(1);
			DisplayTemperature();
			DisplayStatus();
		}
	}

	if (((millis() - menuTime) > menuTimeout) || (menuTime > millis()))
	{
		menuState = 0;
	}

	if (bLightAutoOff && (((millis() - bLightTime) > bLightTimeout) || (bLightTime > millis())))
	{
		SetBacklight(false);
	}

	if (changeMenu)
	{
		changeMenu = false;

		if (backlightOn)
		{
			menuState++;
			DisplayMenuItem();
			DisplaySetting();
			if (menuState > 7)
			{
				menuState = 0;
			}
		}
	}
	else if ((menuState > 0) && (changeSetting != 0))
	{
		ChangeSetting();
		changeSetting = 0;
	}

	checkButtons();

	if (override || (cooling && (temperature1Average < (setpointTemperature - 6))))
	{
		elementOn = true;
		cooling = false;
		digitalWrite(5, HIGH);
	}
	else if (override || (!cooling && (temperature1Average < setpointTemperature)))
	{
		elementOn = true;
		cooling = false;
		digitalWrite(5, HIGH);
	}
	else if (temperature1Average >= setpointTemperature)
	{
		elementOn = false;
		cooling = true;
		digitalWrite(5, LOW);
	}
	else
	{
		elementOn = false;
		digitalWrite(5, LOW);
	}


	if ((tm.Minute % LoggingInterval == 0) && !logging)
	{
		if (menuState < 3) // not setting the time
		{
			logging = true;
			Log(temperature1Average, temperature2Average, sunlightAverage);
		}
	}
	if (!(tm.Minute % LoggingInterval == 0) && logging)
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
