/**
* Lcd Slave for LENR logger
*
* Mini pro code for a bv4242 Lcd/Keypad
* see: http://www.pichips.co.uk/index.php/BV4242#arduino
*
* Version 0.0.1.2
*  added 'E' error message view, display error message until esc or menu buttons are hit
* todo: tidy up, sort out menu items to have more menus

* see: https://github.com/DavidAndrews/Arduino_LCD_Menu - example of oop menu system but not what i need - somehting like effects unit menus from 1980's, maybe when i have more time will write it
*/
#include <SoftwareSerial.h>

#include <bv4242.h>
#include <Wire.h>

SoftwareSerial master(6, 7);// RX, TX

// 7 bit adddress is used, address of pad, address of LCD
BV4242 ui(0x3d);
//8 = esc / cancel
//16 = menu / ok
#define DEBUG_TO_SERIAL 1
#define ROB_WS_MAX_STRING_DATA_LENGTH 120


typedef struct {
  float tc1, tc2, power;
  int psi;
} LenrLoggerData;

LenrLoggerData masterData;

// Screen views
#define LLSM_LOADING 199
#define LLSM_READY 0
#define LLSM_MENU 1
#define LLSM_RUNNING 2
#define LLSM_STOPPED 3
#define LLSM_TIMER 4
#define LLSM_ERROR 5

//menu screens
#define LLM_MAIN 0
#define LLM_MODE 1
#define LLM_SPEED 2
#define LLM_SET_TIMER 1
#define LLM_SET_TEMP 2
#define LLM_SET_WIFI 3

//keys
#define LLK_ENTER 10
#define LLK_ESC 2
#define LLK_UP 1
#define LLK_DOWN 9
#define LLK_RUN 16
#define LLK_STOP 8
#define LLK_TIMER 7

//todo
// struc for logger data
// struc for

int screenMode = LLSM_LOADING;
int lastScreenMode = LLSM_READY;
int menuItem = 0;
int lastMenuItem = 0;
int modeMenuItem = 0;
int menu = LLM_MAIN;
boolean isTimerRunning = false; // is a run

boolean tickOrTock = false;
int runTimeLeft = 0;
int runTotalTime = 0;
int hBridgeOldSpeed = 0;
int hBridgeSpeed = 0;

short pos = 0; // position in read serialBuffer
char serialBuffer[ROB_WS_MAX_STRING_DATA_LENGTH + 1];
char inByte = 0;

char masterMessageLine1[17];
char masterMessageLine2[17];

char errorMessage[17];

void resetMasterData()
{
  masterData.tc1 = 0.00;
  masterData.tc2 = 0.00;
  masterData.power = 0.00;
  masterData.psi = 0;
}

void sendCommand(char c)
{
  char cmd[10];
  sprintf(cmd, "%c|!", c);
  master.println(cmd);
  if (DEBUG_TO_SERIAL == 1) {
    Serial.print("REQUEST SENT: ");
    Serial.println(cmd);
  }
}

void sendCommand(char c, int i)
{
  char cmd[10];
  sprintf(cmd, "%c|%d|!", c, i);
  master.println(cmd);
  if (DEBUG_TO_SERIAL == 1) {
    Serial.print("REQUEST SENT: ");
    Serial.println(cmd);
  }
}

/**
* expects [cmd]|[values..|]|! type string eg: 'C|A|123|456|!'
*/
void processMasterSerial()
{
  // send data only when you receive data:
  while (master.available() > 0)
  {

    // read the incoming byte:
    inByte = master.read();

    if (inByte == '\r') continue;

    // add to our read serialBuffer
    serialBuffer[pos] = inByte;
    //   Serial.println(inByte);
    pos++;

    if (inByte == '\n' || pos == ROB_WS_MAX_STRING_DATA_LENGTH - 1) //end of max field length
    {
      serialBuffer[pos - 1] = 0; // delimit
      if (DEBUG_TO_SERIAL == 1) {
        Serial.print("REQUEST: ");
        Serial.println(serialBuffer);
      }
      processMastersRequest();
      serialBuffer[0] = '\0';
      pos = 0;
    }

  }
}

void processMastersRequest() {
  char recordType = serialBuffer[0];
  if (serialBuffer[0] != '*' && serialBuffer[strlen(serialBuffer) - 1] != '!') return; //check we have full data string ending with '!'

  switch (recordType) {
    case 'E' : // Error message, user must press Esc to return to last screen
      getValue(serialBuffer, '|', 1).toCharArray(errorMessage, 17);
      lastScreenMode = screenMode;
      screenMode = LLSM_ERROR;
      ui.clrBuf(); // keypad buffer
      ui.clear(); // display
      break;
      
    case 'S' : // Run stopped
      if (isTimerRunning) {
        isTimerRunning = false;
        lastScreenMode = LLSM_STOPPED;
        screenMode = LLSM_STOPPED;
        ui.clrBuf(); // keypad buffer
        ui.clear(); // display
      }
      break;
      
    case 'R' : // Run started ok
      if (!isTimerRunning) {
        isTimerRunning = true;
        lastScreenMode = LLSM_RUNNING;
        screenMode = LLSM_RUNNING;
        ui.clrBuf(); // keypad buffer
        ui.clear(); // display
      }
      break;

    case 'D' : // New data
      masterData.tc1 = getValue(serialBuffer, '|', 1).toFloat();
      masterData.tc2 = getValue(serialBuffer, '|', 2).toFloat();
      masterData.power = getValue(serialBuffer, '|', 3).toFloat();
      masterData.psi = getValue(serialBuffer, '|', 4).toInt();
      runTimeLeft = getValue(serialBuffer, '|', 5).toInt();
      runTotalTime = getValue(serialBuffer, '|', 6).toInt();
      break;

    case 'M' : // Message line 1
      getValue(serialBuffer, '|', 1).toCharArray(masterMessageLine1, 17);
      ui.setCursor(1, 1);
      ui.print("Loading: ");
      ui.print(masterMessageLine1);
      break;

    case 'm' : // Message line 2
      getValue(serialBuffer, '|', 1).toCharArray(masterMessageLine2, 17);
      ui.setCursor(1, 2);
      ui.print(masterMessageLine2);
      break;

    case 'C' : // Loading Completed
      ui.clrBuf(); // keypad buffer
      ui.clear(); // display
      screenMode = LLSM_READY;
      break;

    case 'A' : // Auto mode set ok
      modeMenuItem = 0;
      break;
      
    case 'a' : // Manual mode set ok
      modeMenuItem = 1;
      break;
      
    case 'H': // set hbridge speed
      hBridgeSpeed = getValue(serialBuffer, '|', 1).toInt();
      hBridgeOldSpeed = hBridgeSpeed;
      break;
      
    case '*' : //hand shake / reset
      master.print("**\n");
      menuItem = 0;
      modeMenuItem = 0;
      if (screenMode != LLSM_LOADING) {
        isTimerRunning = false;
        lastScreenMode = LLSM_READY;
        screenMode = LLSM_LOADING;
        menu = LLM_MAIN;
        ui.clrBuf(); // keypad buffer
        ui.clear(); // display
      }
      break;
  }
}


void printRunStatus(int lineNumber) {
  ui.setCursor(15, lineNumber);

  if (lineNumber == 1) {
    if (!isTimerRunning) ui.print(" >");
    else ui.print("  ");
  }
  else {

    if (isTimerRunning) {
      if (tickOrTock) ui.print(" >");
      else ui.print("  ");
    }
    else {
      ui.print("  ");
    }
  }
}

void defaultView()
{

  // ui.clrBuf(); // keypad buffer
  ///ui.clear(); // display
  ui.setCursor(1, 1);
  //sprintf todo
  //%-10s
  char lcdBuffer[17];


  char floatBuf[9];
  if (isnan(masterData.tc1)) {
    sprintf(floatBuf, "%s", "ERR TC1");
  } else {
    String tempStr = String(masterData.tc1, 2);

    tempStr.concat("C");
    tempStr.toCharArray(floatBuf, 9);
  }
  char floatBuf2[9];
  if (isnan(masterData.tc2)) {
    sprintf(floatBuf2, "%s", "ERR TC2");
  } else {
    String tempStr2 = String(masterData.tc2, 2);

    tempStr2.concat("C");
    tempStr2.toCharArray(floatBuf2, 9);
  }

  String tempStr3 = String(masterData.power, 1);
  char floatBuf3[7];
  tempStr3.concat("W");
  tempStr3.toCharArray(floatBuf3, 7);

  String tempStr4 = String(masterData.psi);
  char floatBuf4[7];
  tempStr4.concat("p");
  tempStr4.toCharArray(floatBuf4, 7);

  sprintf(lcdBuffer, "%-8s%6s", floatBuf, floatBuf3);
  ui.print( lcdBuffer);
  printRunStatus(1);

  ui.setCursor(1, 2);
  sprintf(lcdBuffer, "%-8s%6s", floatBuf2, floatBuf4);
  ui.print( lcdBuffer);
  printRunStatus(2);

  int  k = 0;
  int count = 0;
  while (count < 50) {
    k = ui.key();
    if (k == LLK_ENTER) {
      lastScreenMode = LLSM_RUNNING;
      screenMode = LLSM_MENU;
      menu = LLM_MAIN;
      lastMenuItem = menuItem;
      ui.clrBuf(); // keypad buffer
      ui.clear(); // display
      return;
    } else if (!isTimerRunning && k == LLK_RUN) {
      sendCommand('R');

      return;
    }  else if (isTimerRunning && k == LLK_STOP) {

      sendCommand('S');

      return;
    } else if (isTimerRunning && k == LLK_TIMER) {
      lastScreenMode = LLSM_RUNNING;
      screenMode = LLSM_TIMER;
      ui.clrBuf(); // keypad buffer
      ui.clear(); // display
      return;
    }

    delay(10);
    count++;
  }
}





void displayMainMenu ()
{

  ui.setCursor(1, 1);
  //sprintf
  if (menuItem == 0) ui.print("*");
  else ui.print(" ");
  ui.print(" Mode          ");
  ui.setCursor(1, 2);
  if (menuItem == 1) ui.print("*");
  else ui.print(" ");
  ui.print(" H-bridge speed");

  ui.setCursor(1, menuItem + 1); //todo
  ui.cursor(); // turn on cursor

  int  k = 0;
  while (true) {
    k = ui.key();

    if (k == LLK_ESC) {
      screenMode = LLSM_RUNNING; //lastScreenMode;
      menuItem = lastMenuItem;
      ui.clrBuf(); // keypad buffer
      ui.clear(); // display
      ui.noCursor();
      return;
    } else if (k == LLK_ENTER) {
      //screenMode = lastScreenMode;
      ui.clrBuf(); // keypad buffer
      ui.clear(); // display
      ui.noCursor();
      ui.clear(); // display
      if (menuItem == 0) menu  = LLM_MODE;
      else menu  = LLM_SPEED;
      return;
    } else {
      if (processLinePosKeysMainMenu(k)) {
        return;
      }
    }

    delay(10);
  }
}

void displayModeMenu ()
{

  ui.setCursor(1, 1);
  //sprintf
  if (modeMenuItem == 0) ui.print(">");
  else ui.print(" ");
  ui.print(" AUTO PID MODE ");
  ui.setCursor(1, 2);
  if (modeMenuItem == 1) ui.print(">");
  else ui.print(" ");
  ui.print(" MANUAL MODE   ");

  ui.setCursor(1, modeMenuItem + 1); //todo
  ui.cursor(); // turn on cursor

  int  k = 0;
  while (true) {
    k = ui.key();

    if (k == LLK_ESC) {
      //screenMode = lastScreenMode;
      menu = LLM_MAIN;
      ui.clrBuf(); // keypad buffer
      ui.clear(); // display
      ui.noCursor();
      return;
    } else if (k == LLK_ENTER) {
      //screenMode = lastScreenMode;
      menu = LLM_MAIN;
      ui.clrBuf(); // keypad buffer
      ui.clear(); // display
      ui.noCursor();
      ui.print(" Saving...");
      delay(500);
      ui.clear(); // display
      if (modeMenuItem == 0) sendCommand('A');// auto run using masters plan once run button is hit
      else sendCommand('M');// stop and start heater manually based on run and stop buttons
      return;
    } else {
      if (processLinePosKeysModeMenu(k)) {
        return;
      }
    }
    delay(10);
  }
}

void displaySpeedMenu ()
{

  ui.setCursor(1, 1);
  //sprintf
  ui.print(" Speed:");
  ui.setCursor(1, 2);
  ui.print(" ");
  ui.print(hBridgeSpeed);
  ui.print(" %");

  ui.setCursor(2, 2);
  ui.cursor(); // turn on cursor

  int  k = 0;
  while (true) {
    k = ui.key();

    if (k == LLK_ESC) {
      //screenMode = lastScreenMode;
      menu = LLM_MAIN;
      ui.clrBuf(); // keypad buffer
      ui.clear(); // display
      ui.noCursor();
      hBridgeSpeed = hBridgeOldSpeed;
      return;
    } else if (k == LLK_ENTER) {
      //screenMode = lastScreenMode;
      menu = LLM_MAIN;
      ui.clrBuf(); // keypad buffer
      ui.clear(); // display
      ui.noCursor();
      ui.print(" Saving...");
      delay(500);
      ui.clear(); // display
      hBridgeOldSpeed = hBridgeSpeed;
      sendCommand('H', hBridgeSpeed);
      return;
    } else {
      if (processLinePosKeysSpeedMenu(k)) {
        return;
      }
    }

    delay(10);
  }
}

void displayTimerView()
{
  char tmpBuf[17];
  ui.noCursor();
  ui.setCursor(1, 1);
  //todo display runTimeLeft which is in secs as hours, mins and secs
  sprintf(tmpBuf, "Time : %-7d", runTimeLeft);
  ui.print(tmpBuf);
  ui.setCursor(1, 2);
  sprintf(tmpBuf, "Total: %-7d", runTotalTime);
  ui.print(tmpBuf);
  int count = 0;
  int k = 0;
  while (count < 25) {
    k = ui.key();
    if (k == LLK_ESC) {
      screenMode = lastScreenMode;
      ui.clrBuf(); // keypad buffer
      ui.clear(); // display
      return;
    }
    delay(10);
    count++;
  }
}

void displayErrorView()
{
  ui.noCursor();
  ui.setCursor(1, 1);
  ui.print("ERROR **********");
  ui.setCursor(1, 2);
  ui.print(errorMessage);
  int count = 0;
  int k = 0;
  while (count < 100) {
    k = ui.key();
    if (k == LLK_ESC || k == LLK_ENTER) {
      screenMode = lastScreenMode;
      ui.clrBuf(); // keypad buffer
      ui.clear(); // display
      return;
    }
    delay(10);
    count++;
  }
}

void displayLoadingScreen()
{
  //nothing needs to update here, screen updated message commandZ are recieved
}

boolean  processLinePosKeysModeMenu(int k) {
  switch ( k ) {
    case LLK_DOWN:
      modeMenuItem++;
      if (modeMenuItem == 2) modeMenuItem = 1;
      return true;
      break;
    case LLK_UP:
      modeMenuItem--;
      if (modeMenuItem < 0) modeMenuItem = 0;
      return true;
      break;
  }

  return false;
}

boolean  processLinePosKeysMainMenu(int k) {
  switch ( k ) {
    case LLK_DOWN:
      menuItem++;
      if (menuItem == 2) menuItem = 1;
      return true;
      break;
    case LLK_UP:
      menuItem--;
      if (menuItem < 0) menuItem = 0;
      return true;
      break;
  }

  return false;
}


boolean  processLinePosKeysSpeedMenu(int k) {
  switch ( k ) {
    case LLK_DOWN:
      hBridgeSpeed--;
      if (hBridgeSpeed < 0) hBridgeSpeed = 0;
      return true;
      break;
    case LLK_UP:
      hBridgeSpeed++;
      if (hBridgeSpeed > 100) hBridgeSpeed = 100;
      return true;
      break;
  }

  return false;
}

void menuView()
{
  //lib i found is not very good at display so needs work, is basic 1980's OOP menu pattern, should be one already written some where
  switch ( menu ) {
    case LLM_MAIN:
      displayMainMenu();
      break;
    case LLM_MODE:
      displayModeMenu();
      break;
    case LLM_SPEED:
      displaySpeedMenu();
      break;
  }

}


void setup()
{
  Serial.println("*** LENR Logger ***");
  Serial.begin(9600);
  master.begin(9600);
  //  ui.lcd_contrast(45);  // 3V3 contrast
  ui.lcd_contrast(22);  // 5V contrast
  ui.noCursor();
  ui.clear(); // display
  ui.lcd_mode(1); // dual ht
  ui.print("  Free Phases");
  delay(1500);
  ui.clear(); // display
  ui.print("  LENR Logger");
  delay(1500);
  ui.clear(); // display
  ui.lcd_mode(0); // single ht
  if (DEBUG_TO_SERIAL == 1) {
    Serial.println("*** LENR Logger ***");
  }
  resetMasterData();
  ui.clear(); // display
  ui.clrBuf(); // keypad buffer
};

void loop()
{
  processMasterSerial();
  tickOrTock = !tickOrTock; // used to change screens to allow messages to be flashed
  switch (screenMode) {
    case LLSM_LOADING:
      displayLoadingScreen();
      break;
    case LLSM_MENU:
      menuView();
      break;
    case LLSM_TIMER:
      displayTimerView();
      break;
    case LLSM_ERROR:
      displayErrorView();
      break;
    default:
      defaultView();
      break;
  }
}

