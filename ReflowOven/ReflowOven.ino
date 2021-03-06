/*********************************************************************
  Headless ReflowController - just with button, buzzer and LED

  based on ReflowOven
    https://github.com/ohararp/ReflowOven

  - To start/abort reflow process push button.
  - Buzzer beeps when cooling stage starts and ends. You may open door oven to cool quickly.
  - LED and buzzer are optional but buzzer is recommended.
  - Using with KOIZUMI Wide Oven Toaster 1200W KOS-1202
**********************************************************************/
// Includes
#include <Adafruit_MAX31855.h>  // Thermocouple Library - https://github.com/rocketscream/MAX31855
#include <PID_v1.h>             // PID Library - http://playground.arduino.cc/Code/PIDLibrary
//             - https://github.com/br3ttb/Arduino-PID-Library/
#include <LiquidCrystal.h>


typedef enum REFLOW_STATE
{
  REFLOW_STATE_IDLE,
  REFLOW_STATE_PREHEAT,
  REFLOW_STATE_SOAK,
  REFLOW_STATE_REFLOW,
  REFLOW_STATE_COOL,
  REFLOW_STATE_COMPLETE,
  REFLOW_STATE_TOO_HOT,
  REFLOW_STATE_ERROR,
  REFLOW_STATE_ABORT
} reflowState_t;

typedef enum REFLOW_STATUS
{
  REFLOW_STATUS_OFF,
  REFLOW_STATUS_ON
} reflowStatus_t;

typedef enum MENU_STATE
{
  MENU_START_SELECTED,
  MENU_PREHEAT_PARAMS_SELECTED,
  MENU_SET_PREHEAT_PARAMS,
  MENU_SOAK_PARAMS_SELECTED,
  MENU_SET_SOAK_PARAMS,
  MENU_REFLOW_PARAMS_SELECTED,
  MENU_SET_REFLOW_PARAMS,
  MENU_COOL_PARAMS_SELECTED,
  MENU_SET_COOL_PARAMS,
  MENU_STATE_REFLOWING
} menuState_t;


/*  Reflow temperature profile

    Tamura leaded SOLDER PASTE(Sn62.8 Pb36.8 Ag0.4) Reflow Profile
        |
    220-|                                                  x  x
        |                                                x       x
    200-|                                               x           x
        |                                              x|           |
        |                                            x  |           | x
    170-|                              x x x  x x  x    |           |   x
        |                  x  x  x  x           |       |<- 20-60s->|     x
    150-|               x                       |                   |
        |             x |                       |                   |
        |           x   |                       |                   |
        |         x     |                       |                   |
        |       x       |                       |                   |
        |     x         |                       |                   |
        |   x   <3C/s   |        20C/60s        |         20/20s    |
    30 -| x             |                       |                   |
        |     <60s      |<-        60s        ->|                   |
        |     Preheat   |        Soaking        |       Reflow      |   Cool
     0  |_ _ _ _ _ _ _ _|_ _ _ _ _ _ _ _ _ _ _ _|_ _ _ _ _ _ _ _ _ _|_ _ _ _
*/
float TEMPERATURE_COOL       = 100;
float TEMPERATURE_SOAK_MIN   = 150;
float TEMPERATURE_SOAK_MAX   = 170;
float TEMPERATURE_REFLOW_MAX = 230;

// Preheat: 1.5C/s
#define PREHEAT_TEMPERATURE_STEP    15
#define PREHEAT_MS_STEP             10000

// Soak: 20C/60s
#define SOAK_TEMPERATURE_STEP       2
#define SOAK_MS_STEP                6000

// Reflow: 20C/20s
#define REFLOW_TEMPERATURE_STEP     5
#define REFLOW_MS_STEP              5000


// ***** PID PARAMETERS *****
// ***** PRE-HEAT STAGE *****
float PID_KP_PREHEAT         = 300;
float PID_KI_PREHEAT         = 0.05;
float PID_KD_PREHEAT         = 350;
// ***** SOAKING STAGE *****
float PID_KP_SOAK            = 300;
float PID_KI_SOAK            = 0.05;
float PID_KD_SOAK            = 350;
// ***** REFLOW STAGE *****
float PID_KP_REFLOW          = 300;
float PID_KI_REFLOW          = 0.05;
float PID_KD_REFLOW          = 350;

#define PID_SAMPLE_TIME        1000
#define SENSOR_SAMPLING_TIME   1000


// Reflow status strings
const char* ssdMessagesReflowStatus[] = {
  "  Ready  ",
  " Preheat ",
  "   Soak  ",
  "  Reflow ",
  "   Cool  ",
  " Complete",
  "  !HOT!  ",
  "  Error  "
};


//
// Pin Definitions
//

int noPin = -1;
int upPin = A2;
int downPin = A4;
int leftPin = A0;
int rightPin = A3;
int selectPin = A1;

int HeaterPin = 13;
int SSRPin1 = A5;

int thermocoupleCLKPin = 10;
int thermocoupleSOPin  = 8;
int thermocoupleCSPin  = 9;

LiquidCrystal lcd(5, 4, 1, 0, 2, 3);

// ***** PID CONTROL VARIABLES *****
double setpoint;
double input;
double inputOld; //Store old Temperature

double output;
double kp = PID_KP_PREHEAT;
double ki = PID_KI_PREHEAT;
double kd = PID_KD_PREHEAT;
int windowSize;
unsigned long windowStartTime;
unsigned long nextCheck;
unsigned long nextRead;
unsigned long timerSoak;
unsigned long buzzerPeriod;
boolean StartTest;

// Reflow oven controller state machine state variable
reflowState_t reflowState;
// Reflow oven controller status
reflowStatus_t reflowStatus;

menuState_t menuState;

//Button Variables
byte ButtCount  = 0;

// tc Error Counter
int tcErrorCtr = 0;

// Seconds timer
int timerSeconds;

// Library Setup
// Specify PID control interface
PID reflowOvenPID(&input, &output, &setpoint, kp, ki, kd, DIRECT);
Adafruit_MAX31855 thermocouple(thermocoupleCLKPin, thermocoupleCSPin, thermocoupleSOPin);

// Setup
void setup()   {
  // Setup Serial Baudrate
  Serial.begin(57600);

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.print("Hello World");

  // initialize the Led pin as an output.
  //pinMode(Led, OUTPUT);

  // Init Buttons
  // pinMode(selectPin, INPUT_PULLUP);

  // Init Elements
  pinMode(HeaterPin, OUTPUT);
  //pinMode(BuzPin, OUTPUT);

  digitalWrite(HeaterPin, LOW);
  //digitalWrite(BuzPin, LOW);

  //button pin setup
  pinMode(selectPin, INPUT);
  pinMode(upPin, INPUT);
  pinMode(downPin, INPUT);
  pinMode(leftPin, INPUT);
  pinMode(rightPin, INPUT);

  //output pins to control relay
  //pinMode(SSRPin0, OUTPUT);
  pinMode(SSRPin1, OUTPUT);

  // Set window size
  windowSize = 5000;
  // Initialize time keeping variable
  nextCheck = millis();
  // Initialize thermocouple reading variable
  nextRead = millis();
  // Set Relow Status = OFF
  reflowStatus = REFLOW_STATUS_OFF;
  menuState = MENU_START_SELECTED;

  StartTest = false;
}

void PrintMenuOptions(bool leftPin, bool rightPin, bool upPin, bool downPin, bool selectPin) {

  Serial.print(leftPin);
  Serial.print(rightPin);
  Serial.print(upPin);
  Serial.print(downPin);
  Serial.println(selectPin);

  //  MENU_START_SELECTED,
  //  MENU_PREHEAT_PARAMS_SELECTED,
  //  MENU_SET_PREHEAT_PARAMS,
  //  MENU_SOAK_PARAMS_SELECTED,
  //  MENU_SET_SOAK_PARAMS,
  //  MENU_REFLOW_PARAMS_SELECTED,
  //  MENU_SET_REFLOW_PARAMS,
  //  MENU_COOL_PARAMS_SELECTED,
  //  MENU_SET_COOL_PARAMS,
  //  MENU_STATE_REFLOWING

  if (millis() > nextRead)
  {
    // Read thermocouple next sampling period
    nextRead += SENSOR_SAMPLING_TIME;
    input = thermocouple.readCelsius();
    lcd.setCursor(11, 1);
    lcd.print(input);
  }
  switch (menuState)
  {

    case MENU_START_SELECTED:
      lcd.setCursor(0, 0);
      lcd.print("Cool >Start  Preheat");

      if (leftPin) {
        menuState = MENU_COOL_PARAMS_SELECTED;
      } else if (rightPin) {
        menuState = MENU_PREHEAT_PARAMS_SELECTED;
      }
      else if (selectPin) {
        menuState = MENU_STATE_REFLOWING;
        StartTest = true;
      }
      break;
    case MENU_PREHEAT_PARAMS_SELECTED:
      lcd.setCursor(0, 0);
      lcd.print("Start >Preheat  Soak");

      if (leftPin) {
        menuState = MENU_START_SELECTED;
      } else if (rightPin) {
        menuState = MENU_SOAK_PARAMS_SELECTED;
      } else if (selectPin || upPin || downPin) {
        menuState = MENU_SET_PREHEAT_PARAMS;
      }
      //      else if (upPin || downPin) {
      //        menuState = MENU_START_SELECTED;
      //      }

      break;
    case MENU_SOAK_PARAMS_SELECTED:
      lcd.setCursor(0, 0);
      lcd.print("Preheat >Soak Reflow");

      if (leftPin) {
        menuState = MENU_PREHEAT_PARAMS_SELECTED;
      } else if (rightPin) {
        menuState = MENU_REFLOW_PARAMS_SELECTED;
      } else if (selectPin || upPin || downPin) {
        menuState = MENU_SET_SOAK_PARAMS;
      }
      //      else if (upPin || downPin) {
      //        menuState = MENU_START_SELECTED;
      //      }
      break;
    case MENU_REFLOW_PARAMS_SELECTED:
      lcd.setCursor(0, 0);
      lcd.print("Soak >Reflow Cool");

      if (leftPin) {
        menuState = MENU_SOAK_PARAMS_SELECTED;
      } else if (rightPin) {
        menuState = MENU_COOL_PARAMS_SELECTED;
      } else if (selectPin || upPin || downPin) {
        menuState = MENU_SET_REFLOW_PARAMS;
      }
      //      else if (upPin || downPin) {
      //        menuState = MENU_START_SELECTED;
      //      }
      break;
    case MENU_COOL_PARAMS_SELECTED:
      lcd.setCursor(0, 0);
      lcd.print("Reflow >Cool  Start");

      if (leftPin) {
        menuState = MENU_REFLOW_PARAMS_SELECTED;
      } else if (rightPin) {
        menuState = MENU_START_SELECTED;
      } else if (selectPin || upPin || downPin) {
        menuState = MENU_SET_COOL_PARAMS;
      }
      //      else if (upPin || downPin) {
      //        menuState = MENU_START_SELECTED;
      //      }
      break;

    //TEMP SETTINGS
    //        float TEMPERATURE_COOL       = 100;
    //        float TEMPERATURE_SOAK_MIN   = 150;
    //        float TEMPERATURE_SOAK_MAX   = 170;
    //        float TEMPERATURE_REFLOW_MAX = 230;

    case MENU_SET_PREHEAT_PARAMS:
      lcd.setCursor(0, 0);
      lcd.print("Preheat Max     Curr");
      lcd.setCursor(0, 1);
      lcd.print(TEMPERATURE_SOAK_MIN);

      if (leftPin || downPin) {
        TEMPERATURE_SOAK_MIN -= 5;
      } else if (rightPin || upPin) {
        TEMPERATURE_SOAK_MIN += 5;
      } else if (selectPin  ) {
        menuState = MENU_PREHEAT_PARAMS_SELECTED;
      }
      break;
    case MENU_SET_SOAK_PARAMS:
      lcd.setCursor(0, 0);
      lcd.print("Soak Max     Curr");
      lcd.setCursor(0, 1);
      lcd.print(TEMPERATURE_SOAK_MAX);

      if (leftPin || downPin) {
        TEMPERATURE_SOAK_MAX -= 5;
      } else if (rightPin || upPin) {
        TEMPERATURE_SOAK_MAX += 5;
      } else if (selectPin || upPin || downPin) {
        menuState = MENU_SOAK_PARAMS_SELECTED;
      }
      break;
    case MENU_SET_REFLOW_PARAMS:
      lcd.setCursor(0, 0);
      lcd.print("Reflow Max     Curr");
      lcd.setCursor(0, 1);
      lcd.print(TEMPERATURE_REFLOW_MAX);

      if (leftPin || downPin) {
        TEMPERATURE_REFLOW_MAX -= 5;
      } else if (rightPin || upPin) {
        TEMPERATURE_REFLOW_MAX += 5;
      } else if (selectPin || upPin || downPin) {
        menuState = MENU_REFLOW_PARAMS_SELECTED;
      }
      break;
    case MENU_SET_COOL_PARAMS:
      lcd.setCursor(0, 0);
      lcd.print("Cool To     Curr");
      lcd.setCursor(0, 1);
      lcd.print(TEMPERATURE_COOL);

      if (leftPin || downPin) {
        TEMPERATURE_COOL -= 5;
      } else if (rightPin || upPin) {
        TEMPERATURE_COOL += 5;
      } else if (selectPin || upPin || downPin) {
        menuState = MENU_COOL_PARAMS_SELECTED;
      }
      break;
  }


}


//returns true if pin is held high for longer than 50ms
bool debounceButton(int pin) {

  bool ret = false;
  int count = 0;

  for (int i = 0; i < 5; i++) {
    if (digitalRead(pin) == HIGH)
    {
      count++;
      delay(20);
    }
  }

  if (count >= 4)
  {
    ret = true;
  } else
  {
    ret = false;
  }

  return ret;
}


// Begin Main Loop
void loop()
{
  // Current time
  unsigned long now;

  if (menuState != MENU_STATE_REFLOWING) {

    //now read from the joystick
    if (digitalRead(leftPin) == HIGH)
    {
      if (debounceButton(leftPin)) {
        //left, right, up, down, select
        PrintMenuOptions(true, false, false, false, false);
      }
    }

    if (digitalRead(rightPin) == HIGH)
    {
      if (debounceButton(rightPin)) {
        //left, right, up, down, select
        PrintMenuOptions(false, true, false, false, false);
      }
    }
    else if (digitalRead(upPin) == HIGH)
    {
      if (debounceButton(upPin)) {
        //left, right, up, down, select
        PrintMenuOptions(false, false, true, false, false);
      }
    }
    else if (digitalRead(downPin) == HIGH)
    {
      if (debounceButton(downPin)) {
        //left, right, up, down, select
        PrintMenuOptions(false, false, false, true, false);
      }
    }
    else if (digitalRead(selectPin) == HIGH)
    {
      //left, right, up, down, select
      if (debounceButton(selectPin)) {
        PrintMenuOptions(false, false, false, false, true);
      }
    } else {
      PrintMenuOptions(false, false, false, false, false);
      return;
    }
  } else { //menu state is reflowing
    // Test for Abort Button Input
    if (digitalRead(selectPin) == HIGH)
    {
      if (debounceButton(selectPin)) {
        //tone(BuzPin,4100,500); //Buzz the Buzzer
        while (digitalRead(selectPin) != LOW) ;
        reflowState = REFLOW_STATE_IDLE;


        if (reflowStatus == REFLOW_STATUS_ON) {
          reflowState = REFLOW_STATE_ABORT;
          menuState = MENU_START_SELECTED;
          StartTest = false;
        } else {

        }
      }
      else
      {
        ButtCount = 0;
        StartTest = false;
        menuState = MENU_START_SELECTED;
      }
    }
  }

  //********************************************************************************************************
  // Time to read thermocouple?
  if (millis() > nextRead)
  {
    // Read thermocouple next sampling period
    nextRead += SENSOR_SAMPLING_TIME;
    // Read current temperature
    inputOld = input; //Store Old Temperature
    input = thermocouple.readCelsius();

    if (isnan(input))
    {
      input = inputOld;
      tcErrorCtr++;

      if (tcErrorCtr >= 5)
      {
        // Illegal operation
        reflowState  = REFLOW_STATE_ERROR;
        reflowStatus = REFLOW_STATUS_OFF;
      }
    }
    else
    {
      tcErrorCtr = 0;
    }
  }

  if (millis() > nextCheck)
  {
    lcd.clear();
    // Check input in the next seconds
    nextCheck += 1000;
    // If reflow process is on going
    if (reflowStatus == REFLOW_STATUS_ON)
    {
      // Toggle LED as system heart beat
      //digitalWrite(Led, !(digitalRead(Led)));
      // Increase seconds timer for reflow curve analysis
      timerSeconds++;
      // Send temperature and time stamp to serial
      Serial.print(timerSeconds);
      Serial.print(",");
      //Serial.print(thermocouple.readInternal());
      //Serial.print(",");
      Serial.print(setpoint);
      Serial.print(",");
      Serial.print(input);
      Serial.print(",");
      Serial.print(output);
      Serial.print(",");
      Serial.println(ssdMessagesReflowStatus[reflowState]);
      //lcd.clear();
      //      lcd.setCursor(0, 0);
      //      lcd.print("Reflow Status On");
      //      lcd.setCursor(11,1);
      //      lcd.print(input);
    }
    else
    {
      Serial.print(input);
      Serial.print(" ");
      Serial.print(thermocouple.readInternal());
      Serial.print(" ");
      Serial.println(thermocouple.readCelsius());
      // Turn off LED
      //digitalWrite(Led, HIGH);
      //lcd.clear();
      //lcd.setCursor(0, 0);
      //lcd.print("Reflow Off");
      //lcd.setCursor(11, 1);
      //lcd.print(input);
    }

    // If currently in error state
    if (reflowState == REFLOW_STATE_ERROR)
    {
      // No thermocouple wire conLednected
      Serial.print("TC Error!\n");

      lcd.setCursor(0, 0);
      lcd.print("TC Error");
      lcd.setCursor(11, 1);
      lcd.print(input);
    }
  }






  // Reflow oven controller state machine
  switch (reflowState)
  {
    case REFLOW_STATE_IDLE:
      // If oven temperature is still above room temperature
      if (input >= TEMPERATURE_COOL)
      {
        reflowState = REFLOW_STATE_TOO_HOT;

        Serial.print(" - HOT - ");
        Serial.println(input);

        //lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(" - HOT - ");
        lcd.setCursor(11, 1);
        lcd.print(input);
      }
      else
      {
        if (StartTest == true)
        {
          Serial.println(" - Begin  - ");
          //lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(" - Begin  - ");

          //Serial.println("Time Setpoint Input Output");
          timerSeconds = 0;

          // Initialize PID control window starting time
          windowStartTime = millis();

          // Tell the PID to range between 0 and the full window size
          reflowOvenPID.SetOutputLimits(0, windowSize);
          reflowOvenPID.SetSampleTime(PID_SAMPLE_TIME);
          reflowOvenPID.SetMode(AUTOMATIC);

          // Preheat profile
          setpoint = input + PREHEAT_TEMPERATURE_STEP;
          timerSoak = millis() + PREHEAT_MS_STEP;

          // Proceed to preheat stage
          reflowState = REFLOW_STATE_PREHEAT;
          StartTest = false;
        }
      }
      break;

    case REFLOW_STATE_PREHEAT:
      //lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Preheating");

      if (timerSeconds / 100 != 0)
        lcd.setCursor(13, 0);
      else if (timerSeconds / 10 != 0)
        lcd.setCursor(14, 0);
      else
        lcd.setCursor(15, 0);
      lcd.print(timerSeconds);

      lcd.setCursor(0, 1);
      lcd.print(setpoint);

      lcd.setCursor(11, 1);
      lcd.print(input);

      reflowStatus = REFLOW_STATUS_ON;
      if (millis() > timerSoak)
      {
        setpoint = input + PREHEAT_TEMPERATURE_STEP;
        if (setpoint > TEMPERATURE_SOAK_MIN) setpoint = TEMPERATURE_SOAK_MIN;
        timerSoak = millis() + PREHEAT_MS_STEP;

        // If minimum soak temperature is achieve
        if (input >= TEMPERATURE_SOAK_MIN - 2.5)
        {
          reflowOvenPID.SetTunings(PID_KP_SOAK, PID_KI_SOAK, PID_KD_SOAK);
          setpoint = TEMPERATURE_SOAK_MIN + SOAK_TEMPERATURE_STEP;
          timerSoak = millis() + SOAK_MS_STEP;

          // Proceed to soaking state
          reflowState = REFLOW_STATE_SOAK;
        }
      }
      break;

    case REFLOW_STATE_SOAK:
      //lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Soaking");

      lcd.setCursor(11, 1);
      lcd.print(input);
      // If micro soak temperature is achieved
      if (millis() > timerSoak)
      {
        timerSoak = millis() + SOAK_MS_STEP;
        // Increment micro setpoint
        setpoint += SOAK_TEMPERATURE_STEP;
        if (setpoint > TEMPERATURE_SOAK_MAX)
        {
          // Set agressive PID parameters for reflow ramp
          reflowOvenPID.SetTunings(PID_KP_REFLOW, PID_KI_REFLOW, PID_KD_REFLOW);
          // Ramp up to first section of soaking temperature
          setpoint = TEMPERATURE_REFLOW_MAX;
          // Proceed to reflowing state
          reflowState = REFLOW_STATE_REFLOW;
        }
      }
      break;

    case REFLOW_STATE_REFLOW:
      //lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Reflowing");
      lcd.setCursor(11, 1);
      lcd.print(input);
      // We need to avoid hovering at peak temperature for too long
      // Crude method that works like a charm and safe for the components
      if (input >= (TEMPERATURE_REFLOW_MAX - 5.0))
      {
        // Set PID parameters for cooling ramp
        reflowOvenPID.SetTunings(PID_KP_REFLOW, PID_KI_REFLOW, PID_KD_REFLOW);
        // Ramp down to minimum cooling temperature
        setpoint = TEMPERATURE_COOL;
        // Proceed to cooling state
        reflowState = REFLOW_STATE_COOL;

        // Turn Elements Off
        digitalWrite(HeaterPin, LOW);

        // Signal to Open Door
        //for (int i = 0; i<10;i++){
        //  tone(BuzPin,4100,100); //Buzz the Buzzer
        //  delay(250);
        //}

      }
      break;

    case REFLOW_STATE_COOL:
      //lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Cooling");
      lcd.setCursor(11, 1);
      lcd.print(input);
      // If minimum cool temperature is achieve
      if (input <= TEMPERATURE_COOL)
      {
        // Retrieve current time for buzzer usage
        buzzerPeriod = millis() + 1000;

        //for (int i = 0; i<10;i++){
        //  tone(BuzPin,4100,500); //Buzz the Buzzer
        //  delay(250);
        //}

        reflowStatus = REFLOW_STATUS_OFF;
        // Proceed to reflow Completion state
        reflowState = REFLOW_STATE_COMPLETE;
      }
      break;

    case REFLOW_STATE_COMPLETE:
      //lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Reflow Done");
      lcd.setCursor(11, 1);
      lcd.print(input);
      if (millis() > buzzerPeriod)
      {
        reflowState = REFLOW_STATE_IDLE;
      }
      break;

    case REFLOW_STATE_TOO_HOT:
      // If oven temperature drops below room temperature
      if (input < TEMPERATURE_COOL)
      {
        // Ready to reflow
        reflowState = REFLOW_STATE_IDLE;
      }
      break;

    case REFLOW_STATE_ERROR:
      //lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Reflow Error");
      lcd.setCursor(11, 1);
      lcd.print(input);
      // If thermocouple problem is still present
      if (isnan(input))
      {
        // Wait until thermocouple wire is connected
        reflowState = REFLOW_STATE_ERROR;
      }
      else
      {
        // Clear to perform reflow process
        reflowState = REFLOW_STATE_IDLE;
      }
      break;

    case REFLOW_STATE_ABORT:
      //lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Abort");
      lcd.setCursor(11, 1);
      lcd.print(input);
      Serial.println("Abort!");
      StartTest = false;
      reflowStatus = REFLOW_STATUS_OFF;
      reflowState = REFLOW_STATE_IDLE;
      break;
  }

  // PID computation and SSR control
  if (reflowStatus == REFLOW_STATUS_ON)
  {
    now = millis();

    reflowOvenPID.Compute();

    if ((now - windowStartTime) > windowSize)
    {
      // Time to shift the Relay Window
      windowStartTime += windowSize;
    }
    if (output > (now - windowStartTime))
    {
      digitalWrite(HeaterPin, HIGH);
      lcd.setCursor(7, 1);
      lcd.print("ON ");
    }
    else
    {
      digitalWrite(HeaterPin, LOW);
      lcd.setCursor(7, 1);
      lcd.print("OFF");
    }
  }
  // Reflow oven process is off, ensure oven is off
  else
  {
    digitalWrite(HeaterPin, LOW);
  }

}
