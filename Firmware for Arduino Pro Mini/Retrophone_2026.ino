// Whizzbizz.com - Arnoud van Delden - July 2023 ~ July 2025 ~ April 2026
//
// This sketch is a further development of my project found at https://www.whizzbizz.com/en/gpo746-t65-rotary-dial-phone-mp3-wav-player
// that needed modification of a Dutch PTT T65 rotary dial phone or a contemporary retro-model GPO746 dial telephone
//
// Waits for hook switch signal and then reads the number from the rotary dial, or decodes the DTMF tones of push button (TDK) phones.
// The 'flash'-button on the telephone, or the generic 'function' button on the pcb may be used to make a direct random selection from all samples, 
// otherwise the first (or only) number determines the subdir/bank to be used (each bank has 10 WAVs) and the last number dialed the WAV within this subdirectory/bank.
// Samples are played through DFPlayer (Mini MP3 Player) controlled over SoftwareSerial with RX=10 and TX=11
//
// Expects a SD-card with the correct directory structure of ten banks/subdirs of ten WAVs each
// The bank/folder/subdir '00' contains the sounds that are played when only one number was dialed
// Otherwise first number is the bank/subdir, last number dialed the sound that is to be played
//
// Important: the player module can only play constant bitrate (CBR) MP3's, 128 kbs is preferred
// Convert and prepare MP3's accordingly with e.g fre:ac on Windows, or Switch when you're on a Mac
//
// First found WAV/MP3 in the root of the SD card is used as dial tone
// Second found WAV/MP2 in the root of the SD card is used as busy tone
//
// The whole system to determine if the audio player is 'busy' was stripped. In the former version there was a DO_ANALOG_BUSY define to switch this 
// to an analog input of the ATMega328 because the level from pin 12 of the 74LS14 did not seem apropriate. But later this was okay, and further more:
// whats the use of knowing (on an hardware level) that the audio player is 'busy'. It is busy right away by playing the dial tone...
//
// Findings / Pitfalls
// -- The DfPlayer modules marked with 'HW247A' (red LED) do NOT have the functions readFileCountsInFolder() and readFolderCounts() implemented! 
//    It is not possible to count files in folders with these! Beter use the ones marked MP3-TF-16P with the blue LED!
// -- Usage of and/or define pinMode of A7 will cause the DfPlayer (both models) to freeze. Not solved yet, use another input!
//

#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"

//#define DEBUG          // Outcomment for production...

#define DO_FILECHECK     // Enable if busy tone must be player when audio file is not found. This must be audio fragment 2 on the SD-card.

// #define DO_FLASH      // Only useful if green ground wire is on clamp 2 of the T65 (or clamp 13 of the SSB 51553) and in the RJ9 cable
//#define DO_ANALOG_BUSY // Read MP3 player 'busy' not digtally from D12 but analog from A6

#define BlinkTime 10000

#define input_hook     3 // Receiver picked up: hook off (HIGH active)
#define input_pulse    4 // Rotary dial pulses (HIGH active)

#define input_std      5 // DTMF StD, DTMF tone stable
#define input_q1       6 // DTMF Q1
#define input_q2       7 // DTMF Q2
#define input_q3       8 // DTMF Q3
#define input_q4       9 // DTMF Q4

// These two are optional, may not be placed on the pcb (and R13 left out)
#define set_button    14 // Util/Config button (HIGH active)
#define led_config    15 // Util/Config LED: used to flash pulses or decoded DTMF (HIGH active)

// These are always there...
#define led_dial_play 16 // Yellow Hook/Dial LED: lit on hook-off, flashes when dialing (LOW active)
#define led_playing   17 // Green LED: lit when music is played (LOW active)

#ifdef DO_FLASH
// A (yet) unsolved mystery: using analog input A7 will HANG the MP3Player! Using D13 instead...
#define input_flash   13 // Green ground wire in phone must be on clamp 2 (for T65) or clamp 13 (SSB 51553) and in the RJ9 cable
#endif

// Busy/Playing signal from DFPlayer Mini (digital HIGH is active) is NOT used/read anymore...
// #define MONITOR_BUSY 
#ifdef MONITOR_BUSY
#define input_busy 12
#endif

unsigned long debounceTimeHook  = 0; // Debounce time for this button/pin...
unsigned long debounceTimePulse = 0; // Debounce time for this button/pin...
unsigned long dialingTimeout = 0;     // To keep track of dialing time out...
unsigned long debounceDelay = 50;      // Global debounce time...

bool hookUp = false;
bool dialing = false;
bool processNumber = false;
bool pulseCounting = false;
bool playerActive = false;
uint8_t dtmfTone;

int hookState;
int pulseState;
int setState;
int lastHookState = LOW;
int lastGpioStatePulse = HIGH;
int lastSetButtonState = HIGH;
int lastNumberDialed = 0;
int nrsDialed = 0;
int pulseCount = 0;
int blinkCount = 0; // For the blinking LED...

int gpioStatePulse;
int hookSwitchState;
int setButtonState;

#ifdef DO_FILECHECK
int firstFolderNr = -1;
uint8_t nrFilesInFolder[20]; // Limited to max 10
int nrFiles;
int nrFolders;
uint8_t folderNr;
#endif

int bankNr = 0;
int songNr = 0;

SoftwareSerial mySoftwareSerial(11, 10); // RX, TX
DFRobotDFPlayerMini myDFPlayer;
uint8_t playerError;
void printDetail(uint8_t type, int value);

void setup() {
  mySoftwareSerial.begin(9600);
  Serial.begin(38400);

  // A (yet) unsolved mystery: using analog input A7 will HANG the MP3Player! Using D13 instead...
  pinMode(input_std, INPUT);
  pinMode(input_q1, INPUT);
  pinMode(input_q2, INPUT);
  pinMode(input_q3, INPUT);
  pinMode(input_q4, INPUT);
  #ifdef MONITOR_BUSY
    pinMode(input_busy, INPUT);
  #endif
  pinMode(input_pulse, INPUT);
  pinMode(input_hook, INPUT);
  #ifdef DO_FLASH
    pinMode(input_flash, INPUT); 
  #endif

  pinMode(led_config, OUTPUT);
  pinMode(led_dial_play, OUTPUT);
  pinMode(led_playing, OUTPUT);

  #ifdef DEBUG
    Serial.println(F("Retrophone Basic 2026"));
    Serial.println(F("Initializing DFPlayer..."));
  #endif
  while (!myDFPlayer.begin(mySoftwareSerial, /*isACK = */true, /*doReset = */true)) {  //Use serial to communicate with mp3.
    #ifdef DEBUG
      Serial.println(F("MP3 Player not ready..."));
      Serial.println(F("Will try again in 1 sec..."));
    #endif
    delay(1000);
  }
  #ifdef DEBUG  
    Serial.println(F("DFPlayer Mini online"));
  #endif

  // Various MP3 player settings...
  myDFPlayer.stop();          // End any audio after soft reset...
  myDFPlayer.setTimeOut(500); //Set serial communictaion time out 500ms
  myDFPlayer.volume(30);      //Set volume value (0~30).
  myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);

  #ifdef DO_FILECHECK
    // Determine first bankNr (folder) and nr of files per (max. 10) folder...
    for (folderNr=0; folderNr<=20; folderNr++) {
      nrFiles = myDFPlayer.readFileCountsInFolder(folderNr);
      #ifdef DEBUG  
      Serial.print(F("Folder "));
      Serial.print(folderNr);
      Serial.print(F(": "));
      Serial.print(nrFiles);
      Serial.println(F(" files"));
      #endif    
      if (nrFiles>0) {
        if (firstFolderNr<0) {
          firstFolderNr = folderNr;
          #ifdef DEBUG  
          Serial.print(F("First audio folder is "));
          Serial.println(firstFolderNr);
          #endif          
        }
        nrFilesInFolder[folderNr-firstFolderNr] = nrFiles;
        if (folderNr-firstFolderNr >= 9)
          break;
      }
    }
    nrFolders = sizeof(nrFilesInFolder)/sizeof(int);
    #ifdef DEBUG  
      Serial.print(F("nrFolders="));
      Serial.println(nrFolders);
      for (folderNr=0; folderNr<10; folderNr++) {
        Serial.print(F("nrFilesInFolder["));
        Serial.print(folderNr);
        Serial.print(F("] = "));
        Serial.println(nrFilesInFolder[folderNr]);
      }
    #endif
  #endif

  // Init LEDs...
  digitalWrite(led_dial_play, HIGH); // Off!
  digitalWrite(led_playing, HIGH);  // Off!
  digitalWrite(led_config, LOW);     // Off!

  #ifdef DEBUG
    Serial.println(F("Setup done..."));
  #endif
}

void loop() {
  // Read 'Hook' state...
  hookSwitchState = digitalRead(input_hook);  
  if (hookSwitchState != lastHookState) {
    debounceTimeHook = millis();
  }
  if ((millis() - debounceTimeHook) > debounceDelay) {
    if (hookSwitchState != hookState) {
      hookState = hookSwitchState;
      if (hookState == HIGH) {
        #ifdef DEBUG
          Serial.println(F("Receiver was picked up..."));
        #endif
        hookUp = true;
        digitalWrite(led_dial_play, LOW); // Turn on LED...
        bankNr = -1;
        nrsDialed = 0;
        // Output dial tone...
        myDFPlayer.play(1);
      } else {
        #ifdef DEBUG
          Serial.println(F("Receiver is put back..."));
        #endif
        hookUp = false;
        dialing = false;
        myDFPlayer.stop(); // End dial tone...
        digitalWrite(led_dial_play, HIGH); // Turn off LED...
        digitalWrite(led_playing, HIGH);  // Turn off LED...
      }
    }
  }
  lastHookState = hookSwitchState;

  #ifdef DEBUG
  #ifdef DO_FLASH
    Serial.print("input_flash=");
    Serial.println(digitalRead(input_flash));
  #endif
  #endif

  if (hookUp) { // Only if receiver was picked up...
    if (!playerActive) { // Only decode possible BankNr and SongNr if not already playing...
      
      if (dialing == true) {
        if (blinkCount > BlinkTime) {
          blinkCount = 0;
          digitalWrite(led_dial_play, !digitalRead(led_dial_play));  // Blink the Hook/Dialing LED...
        }
        blinkCount++;
      } else {
        digitalWrite(led_dial_play, LOW); // LED is on when MP3 player is playing...
      }

      #ifdef DEBUG
      #ifdef DO_FLASH
          if (digitalRead(input_flash))
            digitalWrite(led_playing, HIGH); // led_playing LED on...
          else
            digitalWrite(led_playing, LOW);  // led_playing LED on...
      #endif
      #endif

      if (digitalRead(input_std)==HIGH) {
        if (!dialing) { // Start dial cycle...
          #ifdef DEBUG
            Serial.println(F("Dialing started..."));
          #endif
          processNumber = false;
          dialing = true;
          nrsDialed = 0;
          bankNr = -1;
          songNr = -1;
          myDFPlayer.stop(); // End dial tone...   
        }

        // Read possible DTMF...
        delay(100);
        dtmfTone = ( 0x00 | (digitalRead(input_q1) << 0) | (digitalRead(input_q2) << 1) | (digitalRead(input_q3) << 2) | (digitalRead(input_q4) << 3) );
        if (dtmfTone>10)
          dtmfTone = 10; // DTMF has codes > 10 for '*' and '#'
        lastNumberDialed = dtmfTone;

        digitalWrite(led_config, HIGH);  // Turn on Util/Config LED...
        while (digitalRead(input_std)==HIGH)
          delay(10);
        nrsDialed++;

        #ifdef DEBUG
          Serial.print(F("DTMF number ("));
          Serial.print(nrsDialed);
          Serial.print(F(") dialed: "));
          Serial.println(lastNumberDialed);
        #endif    

        if (bankNr<0) {
          bankNr = dtmfTone;
          #ifdef DEBUG
            Serial.println("bankNr: "+(String)bankNr);
          #endif    
        }
        dialingTimeout = millis(); // Keep resetting timer during dialing...

      } else { // Pulse dialing phase...

        // Read possible pulses from rotary dial...
        digitalWrite(led_config, LOW);  // Turn off Util/Config LED...

        pulseCounting = true;
        while (pulseCounting == true) {
        
          gpioStatePulse = digitalRead(input_pulse);
          if (gpioStatePulse == HIGH)
            digitalWrite(led_config, HIGH);  // Turn on Util/Config LED...
          else
            digitalWrite(led_config, LOW);  // Turn off Util/Config LED...
          if (gpioStatePulse != lastGpioStatePulse) {
            debounceTimePulse = millis();
          }
          if ((millis() - debounceTimePulse) > 20) {
            if (gpioStatePulse != pulseState) {
              pulseState = gpioStatePulse;
              if (gpioStatePulse==HIGH) {
                // The standard pulse length for pulse dialing is 60 milliseconds (ms) on and  
                // 40ms off for each pulse, with an inter-digit pause of 700ms between digits
                delay(35); 
                if (digitalRead(input_pulse)==LOW) { // If it is NOT the longer Hook down pulse (>100ms)
                  pulseCount++;
                  if (!dialing) { // Start dial cycle...
                    #ifdef DEBUG
                      Serial.println(F("Dialing started..."));
                    #endif
                    processNumber = false;
                    dialing = true;
                    nrsDialed = 0;
                    bankNr = -1;
                    myDFPlayer.stop(); // End dial tone...   
                  }
                }
              }
              dialingTimeout = millis(); // Keep resetting timer during dialing...
            }
          }
          lastGpioStatePulse = gpioStatePulse;

          // Time-out between pulse dialed numbers...
          if (pulseCounting && (millis() - debounceTimePulse) > 300) {
            pulseCounting = false;
            if (pulseCount>0) {
              #ifdef DEBUG
                Serial.print(F("Pulse Count: "));
                Serial.println(pulseCount);
              #endif
              nrsDialed++;
              #ifdef DEBUG
                Serial.print(F("Dial number ("));
                Serial.print(nrsDialed);
                Serial.print(F(") dialed: "));
                Serial.println(pulseCount);
              #endif

              if (bankNr<0) {
                bankNr = pulseCount;
                #ifdef DEBUG
                  Serial.println("bankNr: "+(String)bankNr);
                #endif
              }
              lastNumberDialed = pulseCount;
              #ifdef DEBUG
                Serial.println("lastNumberDialed: "+(String)lastNumberDialed);
              #endif
              pulseCount = 0;   
            }   
          }

        }
      }

      // Detecting if dialing phase has ended...
      if (dialing && ((millis() - dialingTimeout) > 2500)) {
        // Dial time out reached...
        #ifdef DEBUG
          Serial.println(F("Dialing finished..."));
        #endif
        processNumber = true;
        dialing = false;
      } 

      // Handling the audio if dialing phase has ended...
      if (processNumber) {
        processNumber = false;
        if (nrFolders>1) {
          if (nrsDialed==1)
            bankNr = 1; // Only one number dialed: use default bank 1...

          // Play WAV....
          #ifdef DEBUG
            Serial.println("Want to play bankNr "+(String)bankNr+" song "+(String)lastNumberDialed);
            Serial.println("Nr files in that folder is "+(String)nrFilesInFolder[bankNr-1]);
          #endif

          if (lastNumberDialed<=nrFilesInFolder[bankNr-1]) {
            digitalWrite(led_dial_play, HIGH); // Power off green LED...
            digitalWrite(led_playing, LOW);    // Turn the red LED on (HIGH=off)
            myDFPlayer.playFolder(bankNr, lastNumberDialed); // Play the song...
            playerActive = true;
          } else { // Sound file with this bank and file index does not exist...
            #ifdef DEBUG
              Serial.println("bankNr "+(String)bankNr+" song "+(String)lastNumberDialed+" does not exist...");
            #endif
            // Output busy tone in stead...
            myDFPlayer.play(2);
          }
          
        } else { // Config error: no folders...
          // Output busy tone in stead...
          myDFPlayer.play(2);
        }
      }
    } // END: if !playerActive
  } else { // END: if hookUp...
    digitalWrite(led_config, LOW); // Turn off Util/Config LED...
    playerActive = false;
    dialing = false;
  }
}

void printDetail(uint8_t type, int value) {
  switch (type) {
    case TimeOut:
      Serial.println(F("DFPlayer: Time Out!"));
      break;
    case WrongStack:
      Serial.println(F("DFPlayer: Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("DFPlayer: Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("DFPlayer: Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("DFPlayer: Card Online!"));
      break;
    case DFPlayerUSBInserted:
      Serial.println(F("DFPlayer: USB Inserted!"));
      break;
    case DFPlayerUSBRemoved:
      Serial.println(F("DFPlayer: USB Removed!"));
      break;
    case DFPlayerPlayFinished:
      Serial.println("DFPlayer: Number:"+(String)value+" Play Finished!");
      break;
    case DFPlayerError:
      switch (value) {
        case Busy:
          Serial.println(F("DFPlayerError: Card not found"));
          break;
        case Sleeping:
          Serial.println(F("DFPlayerError: Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("DFPlayerError: Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("DFPlayerError: Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("DFPlayerError: File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("DFPlayerError: Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("DFPlayerError: In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}
