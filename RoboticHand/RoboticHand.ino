/*
 * 

  This code works with the Machines That Emulate Humans workbook and lesson plan
  Available from the Microsoft Education Workshop at http://aka.ms/hackingSTEM   
  It also contians code for the Rock, Paper, Scissors (RPS) workbook
   
  This projects uses an Arduino UNO microcontroller board. More information can
  be found by visiting the Arduino website: https://www.arduino.cc/en/main/arduinoBoardUno
 
  See https://www.arduino.cc/en/Guide/HomePage for board specific details and tutorials.

  This project relies upon the construction of a sensorized glove that is used to generate
  signals that simultaneously drive a set of servos and display data visualization in Microsoft Excel. 

  The RPS functionality captures the classic hand gestures of rock (all fingers full flexion), 
  paper (all fingers full extension), and scissors (thumb, ring, pinky full flexion, and index, middle full extension). 
  
  The RPS code uses the game loop design pattern so that differnet time intervals can be used simultaneously
  to control output frequency. This is necessary because the serial data needs to be sent at 75 millisecond
  intervals but the servo need to be updated every 15 milliseconds. This is same concept found in Blink Without Delay.

  A match of RPS is started after receiving a trigger from Excel. This resets several program flow variables 
  and storage arrays. The match consists of rounds. Each round consists of a countdown sequence and the 
  detection of hand gesture. At the end of the rounds a match ending flag is switched. After a pause the
  final results are displaued in Excel. 

  David Myka, 2017 Microsoft Education Workshop
 * 
 */

#include <Servo.h>  // Arduino servo library

// Constants that appear in the serial message.
const String mDELIMETER = ",";            // cordoba add-in expects a comma delimeted string

String mInputString = "";                 // string variable to hold incoming data
boolean mStringComplete = false;          // variable to indicate mInputString is complete (newline found)

// Time intervals used to control delays in serial messaging, servo output, and program flow. 
int mServo_Interval = 35;                 // Interval between servo position updates
unsigned long mServo_PreviousTime = millis();    // Timestamp to track interval

int mSerial_Interval = 75;                // Intervel between serial writes
unsigned long mSerial_PreviousTime = millis();   // Timestamp to track interval

int mRound_Interval = 5000;               // Interval between rounds
unsigned long mRound_PreviousTime = millis();    // Timestamp to track interval

int mMatchEnd_Interval = 3000;            // Interval between end of match and final results display
unsigned long mMatchEnd_PreviousTime = millis();    // Timestamp to track interval

// countdown variables
int mCountDown = 0;                       // variable to hold the countdown number sequence
unsigned long mCountDownStartTime = 0;    // timestamp to start countdown timer

// Hand gesture constants
const int ROCK = 1;
const int PAPER= 2;
const int SCISSORS = 3;
const int NAG = -1;                       //NOT A GESTURE

// Hand gesture variables
int mPlayer1RPSgesture = 0;
int mPlayer2RPSgesture = 0;
int mExcelRPSgesture = 0;

// Censoring constants used in censorTheBird() - censors WHEN:
const int MIN_BIRD = 25;                  // middle finger is below this
const int MAX_BIRD = 55;                  // AND remaining digits are above this

// Sensor min/max constants
const int mSENSOR_MIN = 0;                // sets the lowest sensor reading to 0
const int mSENSOR_MAX = 100;              // sets the highest sensor reading to 100

// Servo min/max constants 0-180 (Note: TowerPro SG90 servos jitter when set to extreme positions)
const int mSERVO_MIN = 4;                 // sets the lowest servo position
const int mSERVO_MAX = 176;               // sets the highest servo position

// Flexion/Extension threshold for detecting finger position
const int flexThreshold = 25;

// Sensor number constant
const int mNUM_SENSORS = 5;               // 5 fingers

// 7x2 Array to store MIN/MAX values for auto calibration
int mMinMax[mNUM_SENSORS][2] = {0};

// 7x16 Array to store last 16 values for eliminating spikes
const int NUM_SAMPLES = 16;
int smoothingIndex = 0;
int mSensorSmoothing[mNUM_SENSORS][NUM_SAMPLES] = {0};
int mSensorTotal[mNUM_SENSORS] = {0};

// program flow variables
int mMatchTrigger = 0;                    // Excel sends 1 immediately followed by a 0 to set mStartMatch
int mStartMatch = 0;                      // mMatchTrigger sets this to 1 to start a match
int mMatchEnding = 0;                     // When the match is ending but not yet complete
int mMatchComplete = 1;                   // After final display of round data the match is complete

int mRoundsPerMatch = 5;                  // The number of rounds per match
int mRound = 0;                           // The current round number
//int mRoundWinner;                         // handled by Excel
//int mMatchWinner = 0;                     // handled by Excel

// Arrays to hold each gesture in a round 
int mPlayer1rounds[5];
int mPlayer2rounds[5];

// Flex sensor variables
int sensor0; int sensor1; int sensor2; int sensor3; int sensor4;

// Servo variables
Servo servo0; Servo servo1; Servo servo2; Servo servo3; Servo servo4; 

void setup() {
  Serial.begin(9600);  
  
  // Hand 1 servos
  servo0.attach(2); servo1.attach(3); servo2.attach(4); servo3.attach(5); servo4.attach(6);
}

/*
 * START OF MAIN LOOP -------------------------------------------------------------
 */ 
void loop()
{
  if(mMatchTrigger==1) // Excel sends a trigger to enter into a match. 
  {    
    mMatchTrigger = 0;              // reset so we only enter into this once per match
    mMatchComplete = 0;             // reset match complete flag
    mRound = 0;                     // reset rounds count    
    for(int i=0; i<5; i++)          // reset round gesture data
    {
      mPlayer1rounds[i] = 0;
      mPlayer2rounds[i] = 0;
    }
    mStartMatch = 1;                // start match
  }

  if(mStartMatch==1) // Enter into this section once every round.
  {    
    if( (millis() - mRound_PreviousTime) > mRound_Interval )
    {
      mRound++;                       // increment round number     
      mCountDownStartTime = millis(); // reset countdown start time      
      countDown();                    // enter contdown sequence
      getRPSGestures();               // gather gesture data from glove      
      mRound_PreviousTime=millis();   // reset round interval timer
    }

    // Enter into this section at the end of the match
    if(mRound == mRoundsPerMatch)     // After last round reset match
    {
      mMatchEnding = 1;               
      mMatchTrigger = 0;
      mStartMatch = 0;  
      mMatchEnd_PreviousTime = millis(); // Start the mMatchEnd_Interval 
    }
  }

  // After last round wait for mMatchEnd_Interval to elapse,
  // then enter into this section to complete the match.
  if(mMatchEnding==1 && (millis() - mMatchEnd_PreviousTime) > mMatchEnd_Interval) 
  {
    mMatchEnding = 0;
    mMatchComplete = 1;       // Trigger sent to Excel to display final results of match
  }

  // Process sensors and drive servos - keep the hand moving and data flowing
  processSensorsServos();
 
  // Read Excel commands from serial port
  processIncomingSerial();

  // Process and send data to Excel via serial port
  processOutgoingSerial();  
}


/*
 * RPS GESTURE DETECTION
 */
void getRPSGestures()
{
/*
 *   Sensors are read and finger position is determined
 *   as either full extension ("e") or full flexion ("f")
 *   gesture is a string that is built up of 3 letters 
 *   example of full flexion of 3 fingers: gesture = "fff"
 *   
 *   Note: thumb and pinkie are very unreliable so they are 
 *   not used for gesture detection
 */

  readSensors();                        // get current position of fingers

  String gesture1="";                   // build 
  gesture1 += fingerPosition(sensor1);  // i-index
  gesture1 += fingerPosition(sensor2);  // m-middle
  gesture1 += fingerPosition(sensor3);  // a-ring

  mPlayer1RPSgesture = getGesture(gesture1);      // Read player 1 RPS gesture
  mPlayer1rounds[mRound-1] = mPlayer1RPSgesture;  // add it to player 2 round data array
  
  mPlayer2rounds[mRound-1] = mExcelRPSgesture;  // add it to player 2 round data array
  mPlayer2RPSgesture = mExcelRPSgesture;
}

// translates finger position (0-100) into flexion, extension, or out of range
String fingerPosition(int sensor)
{
  if(sensor>=0 && sensor <=flexThreshold)
    {return "e";}  // full extension
  else if(sensor>=flexThreshold && sensor <=100)
    {return "f";}  // full flexion 
  else 
    {return "x";}  // out of range (should never happen)
}


// translates flexion/extension into hand gesture
int getGesture(String gesture)
{ //index, middle, ring only
  if(gesture == "fff")
    {return ROCK;}
  else if(gesture == "eee")
    {return PAPER;}
  else if(gesture == "eef")
    {return SCISSORS;}
  else
  {return NAG;}   // Not A Gesture
}

/* 
 *  COUNTDOWN SEQUENCE
 */
void countDown()  
{ // enter into this and stay here until complete
  int countdownFinished = 0;                          // reset countown flag
  while(countdownFinished==0) {
    int timeSlice = millis() - mCountDownStartTime;   // determine time passed
    if(timeSlice >= 0 && timeSlice <= 1000) {         // 1st second interval
      mCountDown = 4;
    }
    if(timeSlice >= 1001 && timeSlice <= 2000 ) {
      mCountDown = 3;
    }
    if(timeSlice >= 2001 && timeSlice <= 3000 ) { 
      mCountDown = 2;
    }
    if(timeSlice >= 3001 && timeSlice <= 4000 ) { 
      mCountDown = 1;
    }
    if(timeSlice >= 4001 && timeSlice <= 5250 ) { 
      mCountDown = 0;
    }
    if(timeSlice > 5251) {
      mCountDown = -1;
      countdownFinished = 1;
    }
    processSensorsServos();   // Keep the hand moving   
    processIncomingSerial();  // Read Excel commands from serial port
    processOutgoingSerial();  // Process and send message to Excel via serial port
  }
}


/*
 * SENSOR INOUT AND SERVO OUTPUT CODE--------------------------------------------------------------
 */
void processSensorsServos()
{
  if((millis() - mServo_PreviousTime) > mServo_Interval) // Enter into this only when interval has elapsed
  {
    mServo_PreviousTime = millis();         // Reset interval timestamp
    readSensors();
    driveServos();
  } 
}


void readSensors()
{
  // Hand sensor reads from analog pins
  sensor0 = getSensorValue(0);  // p-thumb
  sensor1 = getSensorValue(1);  // i-index
  sensor2 = getSensorValue(2);  // m-middle
  sensor3 = getSensorValue(3);  // a-ring
  sensor4 = getSensorValue(4);  // c-pinky

  // censor the middle finger gesture
  sensor2 = censorTheBird(sensor0, sensor1, sensor2, sensor3, sensor4);
  
  smoothingIndex++;                       // increment smoothing array index
  if(smoothingIndex >= NUM_SAMPLES)       // if we hit then end of the array...
  {
    smoothingIndex = 0;                   // reset smoothing array index
  }
}


void driveServos()
{
  // Hand 1 servo writes
  servo0.write(mapServo(sensor0)); // p-thumb
  servo1.write(mapServo(sensor1)); // i-index
  servo3.write(mapServo(sensor3)); // m-middle
  servo4.write(mapServo(sensor4)); // a-ring
  servo2.write(mapServo(sensor2)); // c-pinky
}


int censorTheBird(int thumb, int index, int middle, int ring, int pinky)
{
  if(index>MAX_BIRD && middle<MIN_BIRD && ring>MAX_BIRD)
  {
    return 100;             // pull it down
  } else {
    return middle;          // leave it be
  }
}


int getSensorValue(int sensorPin)
{   
  int sensorValue = analogRead(sensorPin);                // read sensor values  
  sensorValue = smooth(sensorValue, sensorPin);           // smooth out voltage peaks  
  if(sensorValue < mMinMax[sensorPin][0]) {mMinMax[sensorPin][0] = sensorValue;}  // set min
  if(sensorValue > mMinMax[sensorPin][1]) {mMinMax[sensorPin][1] = sensorValue;}  // set max  
  // Map the raw ADC values (5v to range 0-1023) to range 0-100
  sensorValue = map(sensorValue, mMinMax[sensorPin][0], mMinMax[sensorPin][1], mSENSOR_MIN, mSENSOR_MAX);
  return sensorValue;
}


int mapServo(int sensorValue)
{   // map sensor value to servo position
  return map(sensorValue, mSENSOR_MIN, mSENSOR_MAX, mSERVO_MIN, mSERVO_MAX);
}


int smooth(int sensorValue, int sensorPin)
{
  mSensorTotal[sensorPin] = sensorValue;                            // add to totals array
  mSensorSmoothing[sensorPin][smoothingIndex] = sensorValue;        // add to smoothing array
  mSensorTotal[sensorPin] = mSensorTotal[sensorPin] + sensorValue;  // add to total
  sensorValue = mSensorTotal[sensorPin]/NUM_SAMPLES;                // get moving average 
  return sensorValue;
}


/*
 * INCOMING SERIAL DATA PROCESSING CODE-------------------------------------------------------------------
 */

void processIncomingSerial()
{
  getSerialData();
  parseSerialData();
}


//Gather bits from serial port to build mInputString
void getSerialData() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();      // get new byte
    mInputString += inChar;                  // add it to input string
    if (inChar == '\n') {                   // if we get a newline... 
      mStringComplete = true;                // we have a complete string of data to process
    }
  }
}


void parseSerialData() 
{
  if (mStringComplete) { // process data from mInputString to set program variables. 
    //process serial data - set variables using: var = getValue(mInputString, ',', index).toInt(); // see getValue function below
    
    mRound_Interval       = getValue(mInputString, ',', 4).toInt();   //Data Out worksheet cell E5
    mRound_Interval       = mRound_Interval * 1000;

    if(mMatchComplete==1){
      mMatchTrigger       = getValue(mInputString, ',', 5).toInt();   //Data Out worksheet cell F5
    }

    mExcelRPSgesture    = getValue(mInputString, ',', 8).toInt();   //Data Out worksheet cell I5
      
    mInputString = "";                         // reset mInputString
    mStringComplete = false;                   // reset stringComplete flag
  }
}


//Get value from mInputString using a matching algorithm
String getValue(String mDataString, char separator, int index)
{ // mDataString is mInputString, separator is a comma, index is where we want to look in the data 'array'
  int matchingIndex = 0;
  int strIndex[] = {0, -1};
  int maxIndex = mDataString.length()-1; 
  for(int i=0; i<=maxIndex && matchingIndex<=index; i++){     // loop until end of array or until we find a match
    if(mDataString.charAt(i)==separator || i==maxIndex){       // if we hit a comma OR we are at the end of the array
      matchingIndex++;                                        // increment matchingIndex to keep track of where we have looked
      // set substring parameters (see return)
      strIndex[0] = strIndex[1]+1;                            // increment first substring index
      // ternary operator in objective c - [condition] ? [true expression] : [false expression] 
      strIndex[1] = (i == maxIndex) ? i+1 : i;                // set second substring index
    }
  }
  return matchingIndex>index ? mDataString.substring(strIndex[0], strIndex[1]) : ""; // if match return substring or ""
}


/*
 * OUTGOING SERIAL DATA PROCESSING CODE-------------------------------------------------------------------
 */
void processOutgoingSerial()
{
  if((millis() - mSerial_PreviousTime) > mSerial_Interval)  // Enter into this only when interval has elapsed
  {
    mSerial_PreviousTime = millis(); // Reset interval timestamp
    sendDataToSerial(); 
  }
}

void sendDataToSerial()
{
  //Program flow variables for Rock,Paper,Scissors workbook
  Serial.print(0);                 //mWorkbookMode - not used anymore;
  
  Serial.print(mDELIMETER);
  Serial.print(mMatchTrigger);     // Starts a Rock,Paper,Scissors match

  Serial.print(mDELIMETER);
  Serial.print(mMatchComplete);    // Flag for match completion
  
  Serial.print(mDELIMETER);
  Serial.print(mCountDown);        // Countdown in between match rounds

  // Hand 1 sensor data for visualization in Machines That Emulate Humans workbook. 
  Serial.print(mDELIMETER);
  Serial.print(sensor0);
  
  Serial.print(mDELIMETER);
  Serial.print(sensor1);
  
  Serial.print(mDELIMETER);
  Serial.print(sensor2);
  
  Serial.print(mDELIMETER);
  Serial.print(sensor3);
  
  Serial.print(mDELIMETER);
  Serial.print(sensor4);
  
  //Current round gesture variables for Rock,Paper,Scissors workbook
  Serial.print(mDELIMETER);
  Serial.print(mRound);
  
  Serial.print(mDELIMETER);
  Serial.print(mPlayer1RPSgesture);
  
  Serial.print(mDELIMETER);
  Serial.print(mPlayer2RPSgesture);

  //Player1 gestures rounds 1-5
  Serial.print(mDELIMETER);
  Serial.print(mPlayer1rounds[0]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer1rounds[1]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer1rounds[2]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer1rounds[3]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer1rounds[4]);  
  
  //Player2 gestures rounds 1-5
  Serial.print(mDELIMETER);
  Serial.print(mPlayer2rounds[0]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer2rounds[1]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer2rounds[2]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer2rounds[3]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer2rounds[4]);

  Serial.println();
}

