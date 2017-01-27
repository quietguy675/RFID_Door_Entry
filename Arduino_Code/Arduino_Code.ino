//     Bit Explanation:   P P A A A A A A A A A A A A B B B B B B B B B B B B B B B B B B B B P
//     Bit Numbers:       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4
//     Example:           1,0,0,0,1,0,0,0,0,0,0,0,1,1,0,0,1,0,0,0,1,0,1,0,0,0,1,0,0,1,0,1,0,0,1 // Stores an ID read from EEPROM
//                        0,-,4,-,-,2,-,-,4,-,-,2,-,-,2,-,-,4,-,- =0,424,224
#include <SD.h>

#define MAX_NUM_IDS 100
//Pin definitions
#define DELETE_MODE_LED 9 // Yellow LED
#define PROGRAM_MODE_LED A5 // Purple LED
#define DOOR_PIN 8
#define SD_PIN 4
//Keypad
#define COLUMN_ONE 5
#define COLUMN_TWO 6
#define COLUMN_THREE 7
#define ROW_ONE A0
#define ROW_TWO A1
#define ROW_THREE A2
#define ROW_FOUR A3
#define DATA0 2
#define DATA1 3

int programModeCode = 1924;
int deleteModeCode = 2013;
int timeKeeper = 0;
char prevChar; //debouncing for keypad
int readCode; //keypad code holder
char readChar; //temp holder for keypad;
unsigned long IDarray[MAX_NUM_IDS]; //id array
int lastNum=-1; // keeps track of the position of the last value in the array

//bools
boolean programMode = false; // Initialize program mode to false
boolean deleteMode = false; // Initialize delete mode to false

//for interrupts
volatile int bitCount = 0;
volatile unsigned long lastBitArrivalTime;
volatile unsigned long tagID = 0;

// interrupt to read in zeros
void isrZero(void) {
  if(bitCount >= 14 && bitCount <=33){
    tagID << 1;
  }
  bitCount++;
}

// interrupt to read in a one
void isrOne(void) {
  if(bitCount >= 14 && bitCount <=33){
    tagID << 1;
    tagID |= 1;
  }
  bitCount++;
}

// on reset, this is run before the main loop
// this sets up the pins, serial communication, interrupts
// reads in from SD card all ids if there is a sd card
void setup()
{
  pinMode(COLUMN_ONE, OUTPUT);
  pinMode(COLUMN_TWO, OUTPUT);
  pinMode(COLUMN_THREE, OUTPUT);
  pinMode(DOOR_PIN, OUTPUT);
  pinMode(PROGRAM_MODE_LED, OUTPUT);
  digitalWrite(PROGRAM_MODE_LED, LOW);
  pinMode(DELETE_MODE_LED, OUTPUT);
  digitalWrite(DELETE_MODE_LED, LOW);

  pinMode(DATA0, INPUT);
  digitalWrite(DATA0, HIGH);
  attachInterrupt(0, isrZero, RISING);
  
  pinMode(DATA1, INPUT);
  digitalWrite(DATA1, INPUT);
  attachInterrupt(1, isrOne, RISING);

  Serial.begin(9600);
  
  while(!SD.begin(SD_PIN)){
    Serial.println("SD Failed, Retry...");
    delay(1000);
    return;
  }
  Serial.println("SD is good");
  readFromSD(IDarray, &lastNum);
  Serial.print("lastNum = ");
  Serial.println(lastNum);
  printArray(lastNum);
}

/*****************************************************************
 *******************MAIN LOOP!!***********************************
 ****************************************************************/
void loop(){
  // if we have read in all bits we need from a UW card
  if(bitCount > 0 && (millis() - lastBitArrivalTime) > 250){
    timeKeeper = 0;
    Serial.println(tagID, DEC);
    Serial.print(" bits: ");
    Serial.println(tagID);
    // if we are adding this card
    if (tagID < 10 || bitCount != 35){
      clearAllBuffers();
    } else {
      if ( programMode ) {
        insertID(tagID, &lastNum);
        printArray(lastNum);
      } // if we are deleting this card
      else if ( deleteMode ) {
        removeID(tagID, &lastNum,validID(tagID,lastNum));
        printArray(lastNum);
      } // check if this is a valid card to open the door
      else if ( validID(tagID, lastNum) >= 0 ) {
        openDoor();
      }
    }
    
    //clear the buffers.
    bitCount = 0;
    tagID = 0;
  }

  //keypad logic
  readChar = scanKeypad();
  if(readChar != 'r' && readChar != 'n') {
    timeKeeper = 0;
    if ((int)readChar >= (int)'0' && (int)readChar <= (int)'9'){
      readCode = readCode * 10 + (int)readChar;
    } else if (readChar == '#'){
      if (readCode == programModeCode) {
        programMode != programMode;
        deleteMode = false;
      } else if (readCode == deleteModeCode) {
        deleteMode != deleteMode;
        programMode = false;
      } else {
        //if it is not a valid code, flash red 3 times
        for (int i = 0; i < 3; i++){
          digitalWrite(DELETE_MODE_LED, HIGH);
          delay(500);
          digitalWrite(DELETE_MODE_LED, LOW);
          delay(500);
        }
      }
      readCode = 0;
    }
    Serial.print("keycode: ");
    Serial.println(readCode);
  }

  //can send commands from serial console!
  if (Serial.available() > 0) {
    timeKeeper = 0;
    byte incomingByte;
    // read the incoming byte:
    incomingByte = Serial.read();
    // say what you got:
    //Serial.print("I received: ");
    //Serial.println(incomingByte);

    switch (incomingByte) {
    case 'o':
      openDoor();
      break;
    case 'p':
      deleteMode = false;
      programMode != programMode;
      break;
    case 'd':
      programMode = false;
      deleteMode != deleteMode;
      break;
    }
    Serial.print("programMode: ");
    Serial.println(programMode);
    Serial.print("deleteMode: ");
    Serial.println(deleteMode);
  }

  //resets all the buffers if there are no inputs for awhile
  // this allows for correct functionality given a card that does not have 35 bits
  if(timeKeeper < 1023){
    timeKeeper++;
  }
  else {
    timeKeeper = 0;
    Serial.println("Timeout, Reset all the things!");
    clearAllBuffers();
  }
  // If a sd card is inserted then it will load
  // the current ids onto it
  if (SD.begin(SD_PIN)) {
    writeToSD(lastNum); 
  }

  //for status LEDS
  digitalWrite(PROGRAM_MODE_LED, programMode);
  digitalWrite(DELETE_MODE_LED, deleteMode);
  delay(10);
}


//****************************************************
//***********end of main loop, start of functions*****
//****************************************************
void clearAllBuffers(){
  //for the keypad
  readCode = 0;
  //for the RFID Reader
  tagID = 0;
  bitCount = 0;
  //to return to normal mode
  programMode = false;
  deleteMode = false;
}


boolean charCompare(char firstChar[], char secondChar[]){
  String firstString(firstChar);
  String secondString(secondChar);
  if(firstString == secondString){
    return true;
  }
  return false;
}

// prints the contents of the id array
void printArray (int last){
  for(int i=0;i<=last;i++){  
    Serial.print("IDarray: ");
    Serial.print(i);
    Serial.print(" = ");
    Serial.println(IDarray[i]);
  }
}

// this opens the door
void openDoor(){
  digitalWrite(8, HIGH);
  Serial.println("Door is open!");
  delay(5000);
  digitalWrite(8, LOW);
}


// inserts an id into to id array at the last position
// it updates the last number that is passed in
void insertID(unsigned long ID, int* last) {
  if (*last > MAX_NUM_IDS - 1){ // if we have too many, dont add
    return -1;
  }
  int valid = validID(ID,*last);
  if (validID(ID,*last)>=0){
    return; 
  }

  *last = *last + 1;
  Serial.print("last= ");
  Serial.println(*last);
  IDarray[*last]=ID;
  writeToSD(*last);
}

// checks if this is a valid id
// returns the position in the array
int validID(unsigned long ID, int last){
  for (int i = 0; i <= last; i++){
    if (ID==IDarray[i]) return i;
  }
  return -1;
}

//removes the id at the given position, updates the location of the last position
void removeID(unsigned long ID,int *last,int pos){
  if (pos<0) return;
  for (int i = pos; i < *last; i++){
    IDarray[i]=IDarray[i+1];
  }
  (*last)--;
  writeToSD(*last);
}

// overwrites all id's to a file
void writeToSD(int lastNum) {
  File toWrite;
  if (SD.exists("IDFile.txt")) {
    SD.remove("IDFile.txt");
  }
  toWrite = SD.open("IDFile.txt",FILE_WRITE);
  for (int i =0; i <= lastNum; i++) {
    toWrite.println(String(IDarray[i]));
  }
  toWrite.close();
}

// read all ids on a file
void readFromSD(long unsigned int IDArray[],int* total) {
  if (!SD.exists("IDFile.txt")) {
    return -1;
  }
  int lastNum = *total;
  int count = 0;
  IDArray[0] = 0;
  char value;

  File toRead = SD.open("IDFile.txt",FILE_READ);
  if(toRead){
    while (toRead.available()) {
      value = toRead.read();
      if (((int)value >= (int)'0') && ((int)value <= (int)'9')) {
        int add = (int)value - (int)'0';
        IDArray[count] = IDArray[count] * 10 + add;
      }
      if ((int)value == '\n') {
        count++;
        IDArray[count] = 0;
      }
    }
  }
  //clears the rest of the array
  for (int i = count; i < lastNum; i++) {
    IDArray[i] = 0;
  }
  *total = count - 1; //because we don't care about the last return in the IDFile.
  toRead.close();
}

//returns either the key being pressed, r for repeat, or n for no return
char scanKeypad(){
  char returnChar = 'n'; //n for no return
  digitalWrite(COLUMN_ONE, HIGH);
  if(digitalRead(ROW_ONE)){
    returnChar = '1';
  }
  else if(digitalRead(ROW_TWO)){
    returnChar = '4';
  }
  else if(digitalRead(ROW_THREE)){
    returnChar = '7';
  }  
  else if(digitalRead(ROW_FOUR)){
    returnChar = '*';
  }
  digitalWrite(COLUMN_ONE, LOW);

  digitalWrite(COLUMN_TWO, HIGH);
  if(digitalRead(ROW_ONE)){
    returnChar = '2';
  }
  else if(digitalRead(ROW_TWO)){
    returnChar = '5';
  }
  else if(digitalRead(ROW_THREE)){
    returnChar = '8';
  }  
  else if(digitalRead(ROW_FOUR)){
    returnChar = '0';
  }
  digitalWrite(COLUMN_TWO, LOW);

  digitalWrite(COLUMN_THREE, HIGH);
  if(digitalRead(ROW_ONE)){
    returnChar = '3';
  }
  else if(digitalRead(ROW_TWO)){
    returnChar = '6';
  }
  else if(digitalRead(ROW_THREE)){
    returnChar = '9';
  }  
  else if(digitalRead(ROW_FOUR)){
    returnChar = '#';
  }
  digitalWrite(COLUMN_THREE, LOW);

  if (returnChar == prevChar) {
    return 'r'; //r for repeat
  }
  else {
    prevChar = returnChar;
    return returnChar; 
  }
}
