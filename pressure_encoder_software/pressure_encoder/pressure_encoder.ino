/*

Written by Brady Perkins, December 2025
for Asia University Biodesign Lab

Supports Arduino Uno,
Adafruit Metro ESP32-S3 (for SD/WiFi/date & time functionality)
Arduino ESP32-S3 dev boards (with pin adapter, LittleFS storage, no SD functionality)

*/

// Constants

#define TURN_LIMIT 960 // Outward turning limit, in degrees
#define SPEED_LIMIT 400 // Motor speed limit, in degrees/sec
#define CALIBRATION_POINT 20 // Calibration point, in kPa

#include <math.h> // Include math.h on all platforms

#if defined(ARDUINO_AVR_UNO)
  #define ARDUINO_GENERIC 1
  #define PLATFORM_ESP32 0
#endif

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3)
  #define ARDUINO_GENERIC 0
  #define PLATFORM_ESP32 1
#endif

#if PLATFORM_ESP32
  #if defined (PIN_NEOPIXEL) // This pin is defined on Metro boards, but not other ESP32-S3 platforms
    #define METRO_ESP32 1
    #define DEV_ESP32 0
  #else
    #define METRO_ESP32 0
    #define DEV_ESP32 1
  #endif
#endif

// Pin assignments

#if ARDUINO_GENERIC
  #define PRESSURE_PIN A0
  #define MOTSLEEP_PIN 5
  #define MOTFLTLED_PIN 6
  #define MOTFAULT_PIN 7
  #define STEP_PIN 8
  #define DIR_PIN 9
  #define SOLENOID_PIN 10
#endif

#if METRO_ESP32
  #define MOTSLEEP_PIN 5
  #define MOTFLTLED_PIN 6
  #define MOTFAULT_PIN 7
  #define STEP_PIN 8
  #define DIR_PIN 9
  #define SOLENOID_PIN 10
  #define PRESSURE_PIN 14
  #define CS_PIN 45
#endif

#if DEV_ESP32
  #define PRESSURE_PIN 4
  #define MOTFLTLED_PIN 5
  #define MOTFAULT_PIN 6
  #define STEP_PIN 7
  #define DIR_PIN 8
  #define SOLENOID_PIN 9
  #define MOTSLEEP_PIN 10
  #define PIN_NEOPIXEL 48 // Not pre-defined on generic dev boards
#endif

#if METRO_ESP32 // Include SD card-writing and NeoPixel libraries on Metro board
  #include <SPI.h>
  #include <SD.h>
#endif

#if PLATFORM_ESP32 // Include wireless, time, and file writing functionality on all ESP32
  #include <FS.h>
  #include <LittleFS.h>
  #include <WiFi.h>
  #include <time.h>
  #include <sys/time.h>
  #include <Adafruit_NeoPixel.h>
  Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800); // Create NeoPixel object

  // Network time variables

  bool wifiConnected = 0; // Whether device is connected to WiFi
  String ssid = "cpd_6460"; // WiFi SSID and password (default values here)
  String password = "233234566460";
  struct tm timeinfo;

  // Logging variables

  bool stgConnected = 0; // Whether storage is inserted and initialized
  bool writingFile = 0; // Whether currently writing file
  bool timeRetrieved = 0; // Whether or not time retrieval was successful
  File f;

  // Network time function

  void setDateTime(int year, int month, int day, int hour, int minute, int second) {
    tm t = {};
    t.tm_year = year - 1900; // tm_year counts from 1900
    t.tm_mon  = month - 1; // tm_mon goes 0-11
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = minute;
    t.tm_sec  = second;
    t.tm_isdst = -1; // not using DST

    time_t epochTime = mktime(&t);  // convert tm -> time_t
    if (epochTime != -1) {
      timeval tv = {};
      tv.tv_sec = epochTime;
      tv.tv_usec = 0;
      settimeofday(&tv, nullptr);
    }
  }

  // NeoPixel variables
  unsigned long lastPxRefresh = 0; // Last time NeoPixel was refreshed
  uint8_t pixelRed = 0; // Pixel red level
  uint8_t pixelBlue = 0; // Pixel blue level

  // Calibration variables
  bool toCalibrate = 0; // Whether to calibrate at next motorPosition < 1
#endif

// Motor variables

int motorPosition = 0; // Current position of the motor, in steps (1 step = 1.8 degrees)
int motorSpeed = 300; // Current fixed motor speed, in degrees/sec
int stepsToTurn = 0; // Steps remaining for motor to turn
bool motorDirection = 0; // Current direction of the motor (0 = out, 1 = in)
bool stepState = 0; // Current state of step pin
bool lastMotorState = 0; // Motor last spinning or not
unsigned long lastStepToggle = 0; // Last time the step pin was toggled

// Solenoid variables

unsigned long solCloseTime = 0; // Next time to close the solenoid
bool solState = 0; // Whether solenoid is open

// Serial output and recording variables

unsigned long lastRefresh = 0; // Last serial output refresh
unsigned long startRecMillis = 0; // Timestamp of last recording start
unsigned long recStopTime = 0; // Next time to stop recording data
bool recState = 0; // Whether data is currently being recorded (CSV output)

// Pause timer variables

unsigned long timerDoneTime = 0; // Next time to resume execution after pause
bool timerState = 0; //　Whether pause timer is currently running
bool lastTimerState = 0; // State of timer at last cycle

// Pressure sensor variables

unsigned int pressureRaw = 0; // Raw pressure sensor reading
unsigned int prsMinReading = 565; // 0 kPa reading
unsigned int prsCalReading = 1000; // 10 kPa reading

// Program variables

String program = ""; // Current loaded program
String procCsvPath = ""; // File path of CSV to process
unsigned int instIndex = 0; // Index of current instruction to run
bool readyToExec = 0; // Whether program is ready for next instruction

int motorTurn(int x) { // x = next desired motor position, in degrees
  if(x > TURN_LIMIT) { // Prevent motor from turning past 0 or the turn limit in degrees
    x = TURN_LIMIT;
  } else if(x < 0) {
    x = 0;
  }
  stepsToTurn = round((float)x / 1.8) - motorPosition;

  if(stepsToTurn < 0) {
    motorDirection = 1;
    digitalWrite(DIR_PIN, motorDirection); // Turn motor in closing direction
  } else if(stepsToTurn > 0) {
    motorDirection = 0;
    digitalWrite(DIR_PIN, motorDirection); // Turn motor in opening direction
  } else {
    return 0;
  }

  stepsToTurn = abs(stepsToTurn);
  digitalWrite(MOTSLEEP_PIN, 0); // Turn on stepper motor driver
  return stepsToTurn;
}

void motorStep() { // Does not take input, just steps the motor if necessary. Is called every loop cycle.
  if((micros() - lastStepToggle) > (900000 / motorSpeed)) {
    stepState = !stepState;
    digitalWrite(STEP_PIN, stepState);
    lastStepToggle = micros();

    if(lastMotorState && !digitalRead(MOTFAULT_PIN)) { // Kill program if motor overheats or draws too much current
      digitalWrite(MOTFLTLED_PIN, 0);
      Serial.println("Fatal motor fault. Program stopped.");
      while(true) delay(1000);
    }

    if(!stepState) {
      motorPosition -= (2 * motorDirection) - 1;
      stepsToTurn--;
    }
  }
}

// The solenoid is normally-closed. Do not let it stay open for too long, or it might overheat. Feel it with your hand if you are concerned.
unsigned long solenoid(unsigned int t) { // t = time in ms
  if(t) {
    unsigned long closeTime = millis() + (unsigned long)t;
    solState = 1;
    digitalWrite(SOLENOID_PIN, solState);
    return closeTime;
  } else {
    solState = 0;
    digitalWrite(SOLENOID_PIN, solState);
    return 0;
  }
}

unsigned long record(unsigned int k) { // k = number of OCT frames @ 25 Hz
  if(k) {
    unsigned long stopTime = millis() + ((1000 * (unsigned long)k) / 25);
    startRecMillis = millis();
    recState = 1;
    #if PLATFORM_ESP32
    if(stgConnected) {
      if(timeRetrieved) {
        if(!getLocalTime(&timeinfo)) {
          Serial.println("Timestamp update failed, using fallback.");
          setDateTime(2025, 1, 1, 0, 0, 0);
        }
      }
      char filePath[40];
      sprintf(filePath,
        "/out/%04d%02d%02d_%02d%02d%02d_oct.csv",
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec
        );
      Serial.print("Creating ");
      Serial.print(filePath);
      Serial.println("...");
      #if METRO_ESP32
        f = SD.open(filePath, FILE_WRITE);
      #endif
      #if DEV_ESP32
        f = LittleFS.open(filePath, "w");
      #endif
      if(f) {
        writingFile = 1;
      } else {
        Serial.println("File open failed.");
      }
    }
    #endif
    Serial.println("Begin CSV.");
    return stopTime;
  } else {
    recState = 0;
    #if PLATFORM_ESP32
    if(writingFile) {
      f.close();
    }
    #endif
    Serial.println("Done.");
    return 0;
  }
}

unsigned long timer(unsigned int p) { // p = timer/pause length in ms
  if(p) {
    unsigned long doneTime = (millis() + (unsigned long)p);
    timerState = 1;
    return doneTime;
  } else {
    timerState = 0;
    return 0;
  }
}

#if PLATFORM_ESP32
  String processCsv() {
    String instructionProgData = ""; // Empty string for program instructions
    String outputProgData = ""; // Empty string for output program
    unsigned int procCsvPos = 0; // Current position in CSV file read

    int timeInit = 0; // Initial instruction time
    int posInit = 0; // Initial instruction motor position
    int instSpeed = 0; // Current motor speed in program

    String timeFinalStr = ""; // String for reading instruction final time
    String posFinalStr = ""; // String for reading instruction final position
    int timeFinal = 0; // Final instruction time
    int posFinal = 0; // Final instruction motor position

    int totalTime = 0; // Total time for program completion

    File procCsvData = SD.open(procCsvPath, FILE_READ);

    if(procCsvData) {
      String currentInst = ""; // Empty string for current instruction
      String timeInitStr = ""; // Time at beginning of instruction

      while(procCsvData.available()) { // Read initial time
        char c = procCsvData.read();
        if(c == '\r') continue; // Ignore \r in \r\n newlines
        if(c == ',') break; // Read time from first column
        timeInitStr += c;
      }
      String posInitStr = ""; // Motor position at beginning of instruction
      while(procCsvData.available()) { // Read initial position
        char c = procCsvData.read();
        if(c == '\r') continue;
        if(c == '\n') break; // Break at newline
        posInitStr += c; // Read motor position from second column
      }

      timeInit = timeInitStr.toInt();
      posInit = posInitStr.toInt();

      while(procCsvData.available()) {
        currentInst = ""; // Empty string for current instruction
        timeFinalStr = ""; // Time at end of instruction

        while(procCsvData.available()) {
          char c = procCsvData.read();
          if(c == '\r') continue;
          if(c == ',') break;
          timeFinalStr += c;
        }
        posFinalStr = ""; // Motor position at end of instruction
        while(procCsvData.available()) {
          char c = procCsvData.read();
          if(c == '\r') continue;
          if(c == '\n') break;
          posFinalStr += c;
        }

        timeFinal = timeFinalStr.toInt();
        posFinal = posFinalStr.toInt();

        int instTime = timeFinal - timeInit; // Time for instruction completion
        int instDisp = posFinal - posInit; // Motor displacement during instruction completion

        totalTime += instTime;

        if(instDisp != 0) {
          if(instSpeed != (1000 * abs(instDisp)) / instTime) { // Check if speed has changed since last instruction
            instSpeed = (1000 * abs(instDisp)) / instTime; // Motor velocity during instruction (degrees/sec)
            currentInst += "v" + String(instSpeed) + ",m" + String(posFinal) + ","; // Add velocity and movement to current instruction
          } else {
            currentInst += "m" + String(posFinal) + ","; // Skip speed instruction if unchanged
          }
        } else {
          currentInst += "t" + String(instTime) + ","; // Add timer to current instruction
        }

        instructionProgData += currentInst; // Append current instruction to string of instructions

        timeInit = timeFinal; // Set initial time to current final for next instruction
        posInit = posFinal; // Set initial position to current final for next instruction
      }

      outputProgData += "r" + String(totalTime / 40) + ",s" + String(totalTime) + ","; // Add initial instructions to output program data
      outputProgData += instructionProgData; // Append instructions to output program data
      outputProgData.remove(outputProgData.length() - 1, 1); // Remove last comma from final program string

      Serial.print("Program loaded from ");
      Serial.print(procCsvPath);
      Serial.println(".");
      return outputProgData;
    } else {
      Serial.print(procCsvPath);
      Serial.println(" does not exist or file open failed.");
      return "";
    }
  }

  bool calibrate() { // Calibration process for ESP32 platforms
    unsigned int highCalibration = 0; // Storage variable for high calibration measurements
    unsigned int lowCalibration = 0; // Storage variable for low calibration measurements

    Serial.println("Beginning calibration.");
    digitalWrite(SOLENOID_PIN, 1);
    delay(1500);
    Serial.println("Gently place your finger at the end of the outlet tube.");
    delay(1500);
    Serial.println("Adjust your finger: seal and release air and watch the pressure gauge move.");
    delay(4500);
    Serial.print("Adjust your finger until the gauge reads as close to ");
    Serial.print(CALIBRATION_POINT);
    Serial.println(" kPa as possible.");
    delay(1500);
    for(int i = 1; i < 4; i++) {
      Serial.print("(");
      Serial.print(i);
      Serial.print(") ");
      Serial.print("Type 'next' and enter when the gauge steadily reads ");
      Serial.print(CALIBRATION_POINT);
      Serial.print(" kPa.");

      int k = 0; // Counting variable
      while(!(Serial.readStringUntil('\n') == "next") && k < 10) {
        delay(1000);
        Serial.print(".");
        k++;
      }
      if(k == 10) {
        digitalWrite(SOLENOID_PIN, 0);
        Serial.println("Timed out. Calibration incomplete.");
        return 0;
      } else {
        unsigned int pressureVal = analogRead(PRESSURE_PIN);
        highCalibration += pressureVal;
        Serial.print("(");
        Serial.print(i);
        Serial.print(") ");
        Serial.println(pressureVal);
        delay(500);
      }
      k = 0;
    }
    digitalWrite(SOLENOID_PIN, 0);
    highCalibration = highCalibration / 3; // Find average of the three points
    Serial.print("Average: ");
    Serial.println(highCalibration);
    prsCalReading = highCalibration; // Set high calibration value
    delay(1500);

    Serial.println("Now taking zero points. Remove your finger from the tube end.");
    delay(4500);

    for(int i = 1; i < 4; i++) {
      unsigned int zeroVal = analogRead(PRESSURE_PIN);
      lowCalibration += zeroVal;
      Serial.print("(");
      Serial.print(i);
      Serial.print(") ");
      Serial.println(zeroVal);
      delay(1500);
    }
    lowCalibration = lowCalibration / 3; // Find average of the three points
    Serial.print("Average: ");
    Serial.println(lowCalibration);
    prsMinReading = lowCalibration; // Set low calibration value

    #if METRO_ESP32 // Open calibration.csv
      f = SD.open("/sys/calibration.csv", FILE_WRITE);
    #endif
    #if DEV_ESP32
      f = LittleFS.open("/sys/calibration.csv", "w");
    #endif
    if(!f) {
      Serial.println("File open failed.");
    } else {
      f.print(lowCalibration); // Write new data to calibration.csv
      f.print(",");
      f.print(highCalibration);
      f.close();
      Serial.println("Data saved to /sys/calibration.csv.");
    }

    toCalibrate = 0;
    Serial.println("Calibration complete.");
    return 1;
  }
#endif

void setup() {
  // put your setup code here, to run once:
  pinMode(PRESSURE_PIN, INPUT);
  pinMode(MOTFAULT_PIN, INPUT);
  pinMode(MOTFLTLED_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(MOTSLEEP_PIN, OUTPUT);
  pinMode(SOLENOID_PIN, OUTPUT);

  digitalWrite(MOTFLTLED_PIN, 1); // Disable motor fault LED at startup
  digitalWrite(STEP_PIN, 0);
  digitalWrite(DIR_PIN, 0);
  digitalWrite(MOTSLEEP_PIN, 0); // Turn stepper motor driver on to avoid startup fault
  digitalWrite(SOLENOID_PIN, 0);

  delay(1000); // Wait until serial terminal is ready for connection on other end

  digitalWrite(MOTSLEEP_PIN, 1); // Disable stepper motor driver

  Serial.begin(115200);

  #if ARDUINO_GENERIC
    Serial.println("Running on Arduino (generic).");
  #endif

  #if PLATFORM_ESP32
  File wifiCredentials;
  File calibrationData;

  pixel.begin(); // NeoPixel
  pixel.clear();
  pixel.setPixelColor(0, pixel.Color(0, 255, 0));
  pixel.setBrightness(50);
  pixel.show();

  pixelRed = (uint8_t)(150 * abs(motorSpeed - 50) / 175); // Prepare red & blue for motor speed after startup
  pixelBlue = (uint8_t)(150 * abs(motorSpeed - 300) / 175);
  #endif

  #if DEV_ESP32 // Boot sequence for ESP32-S3 N16R8
    Serial.println("Running on ESP32 (N16R8 Dev Module)");

    if(!LittleFS.begin(true)) {
      Serial.println("LittleFS mount failed.");
    } else {
      stgConnected = 1;
      Serial.println("LittleFS mount successful.");
    }

    wifiCredentials = LittleFS.open("/sys/wifi.csv");
    calibrationData = LittleFS.open("/sys/calibration.csv");
  #endif

  #if METRO_ESP32 // Boot sequence for Adafruit Metro ESP32-S3
    Serial.println("Running on ESP32 (Adafruit Metro).");

    if(!SD.begin(CS_PIN, SPI, 1000000)) {
      SPI.end();
      Serial.println("No SD card present.");
    } else {
      stgConnected = 1;
      Serial.println("SD card initialized.");
    }

    wifiCredentials = SD.open("/sys/wifi.csv");
    calibrationData = SD.open("/sys/calibration.csv");
  #endif
  
  #if PLATFORM_ESP32
    if(wifiCredentials) {
      ssid = "";
      while(wifiCredentials.available()) {
        char c = wifiCredentials.read();
        if(c == ',') break; // Read SSID before comma
        ssid += c;
      }
      password = "";
      while(wifiCredentials.available()) {
        char c = wifiCredentials.read();
        if(c == '\n') break; // Break if any newlines exist
        password += c; // Read password from rest of file
      }
      Serial.println("WiFi credentials read successfully.");
    } else {
      Serial.println("Failed to read WiFi credentials.");
      Serial.println("Ensure wifi.csv is placed in the /sys/ folder.");
      Serial.println("Trying default SSID and password.");
    }

    if(calibrationData) {
      String prsMinReadingStr = "";
      while(calibrationData.available()) {
        char c = calibrationData.read();
        if(c == ',') break;
        prsMinReadingStr += c;
      }
      prsMinReading = prsMinReadingStr.toInt();
      String prsCalReadingStr = "";
      while(calibrationData.available()) {
        char c = calibrationData.read();
        if(c == '\n') break;
        prsCalReadingStr += c;
      }
      prsCalReading = prsCalReadingStr.toInt();
      Serial.println("Calibration data read successfully.");
    } else {
      Serial.println("Failed to read calibration data.");
      Serial.println("Ensure calibration.csv is placed in the /sys/ folder.");
      Serial.println("Using default calibration values.");
    }
  #endif

  #if PLATFORM_ESP32 // Connect to WiFi on all ESP32
    timeinfo.tm_sec  = 0; // Default time (2025/01/01 00:00:00)
    timeinfo.tm_min  = 0;
    timeinfo.tm_hour = 0;
    timeinfo.tm_mday = 1;
    timeinfo.tm_mon  = 0;
    timeinfo.tm_year = 125;

    Serial.print("Attempting connection to ");
    Serial.print(ssid);
    Serial.print("...");

    WiFi.begin(ssid, password);

    int j = 0;
    while(WiFi.status() != WL_CONNECTED && j < 10) {
      delay(500);
      Serial.print(".");
      j++;
    } if(WiFi.status() != WL_CONNECTED) {
      Serial.println("\nConnection failed.");
    } else {
      wifiConnected = 1;
      Serial.println("\nConnection succeeded.");
      Serial.print("Local IP: ");
      Serial.println(WiFi.localIP());
    }

    Serial.print("Retrieving date and time...");

    if(wifiConnected) {
      configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // GMT+8 for Taiwan
      tzset(); // Set time zone
      int retry = 0; // Wait until time is successfully synchronized
      while(!getLocalTime(&timeinfo) && retry < 3) {
        delay(500);
        retry++;
        Serial.print(".");
      }

      if(retry > 2) {
        Serial.println("\nFailed to get correct date and time. Using fallback.");
        setDateTime(2025, 1, 1, 0, 0, 0);
      } else {
        Serial.println("\nDate and time retrieved successfully.");
      }
      setDateTime(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec
      );
      if(getLocalTime(&timeinfo)) {
        timeRetrieved = 1;
        Serial.print("Current time: ");
        char dateTimeF[40];
        sprintf(dateTimeF,
          "%04d-%02d-%02d %02d:%02d:%02d",
          timeinfo.tm_year+1900,
          timeinfo.tm_mon+1,
          timeinfo.tm_mday,
          timeinfo.tm_hour,
          timeinfo.tm_min,
          timeinfo.tm_sec);
        Serial.println(dateTimeF);
      } else {
        Serial.println("Defaulting to 2025/01/01 00:00:00");
      }
    } else {
      Serial.println("\nNot connected to a network. Using fallback date & time.");
      setDateTime(2025, 1, 1, 0, 0, 0);
    }
  #endif

  Serial.print("Ready at ");
  Serial.print(millis());
  Serial.println(" ms.");
}

void loop() {
  // put your main code here, to run repeatedly:
  if(Serial.available()) { // Read user commands over serial
    String input = Serial.readStringUntil('\n');
    input.trim();

    if(input.startsWith("mot")) { // Move the motor to a specified position. "mot 540"
      int spaceIndex = input.indexOf(' ');
      if(spaceIndex > 0) {
        String degPosStr = input.substring(spaceIndex + 1);
        int degreePos = degPosStr.toInt();
        if(!recState) {
          Serial.print("Motor is turning ");
          Serial.print(motorTurn(degreePos));
          Serial.println(" steps.");
        } else {
          motorTurn(degreePos);
        }
      }
    } else if(input.startsWith("sol")) { // Open the solenoid for a specified number of ms. "sol 3000"
      int spaceIndex = input.indexOf(' ');
      if(spaceIndex > 0) {
        String solTimeStr = input.substring(spaceIndex + 1);
        unsigned int solOpenTime = solTimeStr.toInt();
        if(!recState) {
          Serial.print("Solenoid is opening for ");
          solCloseTime = solenoid(solOpenTime);
          Serial.print(solCloseTime - millis());
          Serial.println(" ms.");
        } else {
          solCloseTime = solenoid(solOpenTime);
        }
      }
    } else if(input.startsWith("vel")) { // Update motor velocity. "vel 300"
      int spaceIndex = input.indexOf(' ');
      if(spaceIndex > 0) {
        String motSpeedStr = input.substring(spaceIndex + 1);
        motorSpeed = motSpeedStr.toInt(); // Update motor speed variable
        if(motorSpeed > SPEED_LIMIT) motorSpeed = SPEED_LIMIT;
        #if PLATFORM_ESP32
          pixelRed = (uint8_t)(150 * abs(motorSpeed - 50) / (SPEED_LIMIT / 2)); // Update NeoPixel color
          pixelBlue = (uint8_t)(150 * abs(motorSpeed - 300) / (SPEED_LIMIT / 2));
        #endif
        Serial.print("Motor speed is ");
        Serial.print(motorSpeed);
        Serial.println(" deg/sec.");
      }
    } else if(input.startsWith("run")) { // Run a program. "run" (to run current program) or "run a1,b2,c3"
      int spaceIndex = input.indexOf(' ');
      if(spaceIndex > 0) {
        program = input.substring(spaceIndex + 1); // Run program from input
        program.replace(" ", ""); // Strip white spaces
        instIndex = 0; // Start at beginning of program
        readyToExec = 1; // Flag to begin interpreter
        Serial.println("Running from input.");
      } else {
        instIndex = 0; // Start at beginning of program
        readyToExec = 1; // If no program is provided, run what is loaded
        Serial.println("Running from memory.");
      }
    } else if(input == "list") { // List currently loaded program
      Serial.println(program);
    }
    #if PLATFORM_ESP32
    else if(input.startsWith("load")) { // Convert CSV from /in/ folder to program
      if(stgConnected) {
        int spaceIndex = input.indexOf(' ');
        if(spaceIndex > 0) {
          String procCsvName = input.substring(spaceIndex + 1); // Read CSV file name
          procCsvPath = "/in/" + procCsvName + ".csv"; // Generate complete file path
          program = processCsv(); // Run CSV to program function
        }
      } else {
        Serial.println("Failed to open file.");
        Serial.println("Re-insert storage, restart device, and try again.");
      }
    } else if(input.startsWith("wifi")) { // Enter WiFi credentials. "wifi ssid,password"
      if(stgConnected) {
        int spaceIndex = input.indexOf(' ');
        if(spaceIndex > 0) {
          String wifiCredentialsStr = input.substring(spaceIndex + 1);
          wifiCredentialsStr.replace(" ", ""); // Strip white spaces
          #if METRO_ESP32
            f = SD.open("/sys/wifi.csv", FILE_WRITE);
          #endif
          #if DEV_ESP32
            f = LittleFS.open("/sys/wifi.csv", "w");
          #endif
          if(f) {
            writingFile = 1;
            f.print(wifiCredentialsStr);
            Serial.println("Credentials saved successfully.");
          }
          writingFile = 0;
          f.close();
        }
      } else {
        Serial.println("Failed to open credentials file.");
        Serial.println("Re-insert storage, restart device, and try again.");
      }
    } else if(input == "cal") { // Run calibration. "cal"
      if(motorPosition < 295) motorTurn(TURN_LIMIT); // Move motor to turn limit
      toCalibrate = 1;
    } else if(input == "help") { // Print help screen. "help"
      Serial.println("\n --- HOW TO OPERATE ---");
      Serial.println("\n*****\n");
      Serial.println("Manual control commands:\n");
      Serial.println("mot [value]: Move the motor to a specified position in degrees.");
      Serial.println("sol [value]: Open the solenoid valve for a specified number of milliseconds.");
      Serial.println("vel [value]: Adjust the motor speed to a specified value in degrees/second.");
      Serial.println("\nProgram commands:\n");
      Serial.println("run, run [program]: Run the currently-loaded program, or manually type in a program to load and run.");
      Serial.println("list: List the set of comma-separated instructions currently loaded.");
      #if METRO_ESP32
      Serial.println("load [filename]: Load a program from [filename].csv from the /in/ folder of the SD card.");
      #endif
      #if DEV_ESP32
      Serial.println("load [filename]: Load a program from [filename].csv from the /in/ folder of the LittleFS volume.");
      #endif
      Serial.println("\nSystem commands:\n");
      #if METRO_ESP32
      Serial.println("wifi [ssid],[password]: Load a set of WiFi credentials to the /sys/wifi.csv file of the SD card.");
      Serial.println("cal: Run a calibration routine and save the results to the /sys/calibration.csv file of the SD card.");
      #endif
      #if DEV_ESP32
      Serial.println("wifi [ssid],[password]: Load a set of WiFi credentials to the /sys/wifi.csv file of the LittleFS volume.");
      Serial.println("cal: Run a calibration routine and save the results to the /sys/calibration.csv file of the LittleFS volume.");
      Serial.println("wipe: Wipe the LittleFS volume.");
      #endif
      Serial.println("help: Print this help screen.");
      Serial.println("\n*****\n");
      #if METRO_ESP32
      Serial.println("SD card file structure:");
      #endif
      #if DEV_ESP32
      Serial.println("LittleFS file structure:");
      #endif
      #if PLATFORM_ESP32
      Serial.println("in/");
      Serial.println("  [your motor time-position tables in CSV format]");
      Serial.println("out/");
      Serial.println("  [output data generated during trials in CSV format]");
      Serial.println("sys/");
      Serial.println("  wifi.csv");
      Serial.println("  calibration.csv");
      Serial.println("\n*****\n");
      #endif
      Serial.println("Comma-separated instruction example:");
      Serial.println("  r100,s5000,t1000,v300,m540,m0,t1000\n");
      Serial.println("    r[value] = Record for [value] number of OCT frames @ 25 Hz");
      Serial.println("    s[value] = Open the solenoid valve for [value] milliseconds");
      Serial.println("    t[value] = Set a pause timer for [value] milliseconds");
      Serial.println("    v[value] = Set the motor velocity to [value] degrees/second");
      Serial.println("    m[value] = Turn the motor to the position of [value] degrees");
      Serial.println("\n    t[value] and m[value] are blocking, meaning that further instructions will not run until they are complete.");
      Serial.println("\n*****\n");
    }
    #if DEV_ESP32
      else if(input == "wipe") { // LittleFS must be wiped in script, storage not accessible on other devices
          LittleFS.format();
          Serial.println("LittleFS wiped successfully.");
      }
      #endif
    #endif
  }

  // Step motor, close solenoid, or stop recording if ready
  if(stepsToTurn > 0) motorStep();
  if(solState && millis() >= solCloseTime) solCloseTime = solenoid(0);
  if(recState && millis() >= recStopTime) recStopTime = record(0);
  if(timerState && millis() >= timerDoneTime) timerDoneTime = timer(0);

  // Take reading from pressure sensor
  pressureRaw = analogRead(PRESSURE_PIN);

  // Program interpreter
  if(readyToExec) {
    int instEndIndex = program.indexOf(',', instIndex); // End boundary of instruction
    if(instEndIndex < 0) instEndIndex = program.length(); // Protect when no comma at end of program
    String instruction = program.substring(instIndex, instEndIndex); // Read instruction

    char command = instruction.charAt(0); // Read the type of command
    unsigned int value = instruction.substring(1).toInt(); // Read the value of the command

    if(command == 'm') {
      motorTurn(value);
      readyToExec = 0; // Pause execution for motor turn
    } else if(command == 's') {
      solCloseTime = solenoid(value);
    } else if(command == 'r') {
      recStopTime = record(value);
    } else if(command == 't') {
      timerDoneTime = timer(value);
      readyToExec = 0; // Pause execution while timer runs
    } else if(command == 'v') { // Max speed 300 deg/sec, min speed 150 deg/sec
      if(value > SPEED_LIMIT) value = SPEED_LIMIT;
      motorSpeed = value; // Update motor speed
      #if PLATFORM_ESP32
        pixelRed = (uint8_t)(150 * abs(motorSpeed - 50) / (SPEED_LIMIT / 2)); // Update NeoPixel color
        pixelBlue = (uint8_t)(150 * abs(motorSpeed - 300) / (SPEED_LIMIT / 2));
      #endif
    } else {
      Serial.print("Invalid instruction at ");
      Serial.print(instIndex);
      Serial.println(":");
      Serial.print(" > ");
      Serial.println(instruction);
      solCloseTime = solenoid(0);
      recStopTime = record(0);
      instEndIndex = program.length(); // Move to end of program to abort
    }

    String next = program.substring(instEndIndex); // Look after current instruction
    if(!next.length()) { // Reset index to 0 if no more to read
      instIndex = 0;
    } else { // Otherwise, move the index forward
      instIndex += instruction.length() + 1;
    }
  }
  if(!instIndex && readyToExec) readyToExec = 0;
  
  if(!stepsToTurn && lastMotorState && instIndex) readyToExec = 1; // Resume program execution after blocking commands if not over
  if(!timerState && lastTimerState && instIndex) readyToExec = 1;

  if(!stepsToTurn && lastMotorState) digitalWrite(MOTSLEEP_PIN, 1); // Turn stepper motor driver off after completed turn

  // CSV data out in loop
  if(millis() - lastRefresh >= 100 && recState) { // 10 Hz recording speed
    unsigned long currentMs = millis() - startRecMillis; // Get timestamp based on recording start time
    int intReading = (pressureRaw > prsMinReading) ? (pressureRaw - prsMinReading) : 0; // Don't allow negative pressure results
    int intPrsScale = prsCalReading - prsMinReading; // Find the difference between calibrated & minimum pressure readings
    float pressureOut = (((float)intReading / (float)intPrsScale) * (float)CALIBRATION_POINT); // Normalize calibrated pressure output to calibration point
    pressureOut = round(pressureOut * 1000) / 1000.0f; // Round to three decimal places
    float forceOut = (2.0f * 0.00001257 * pressureOut * 1000.0f * 1000.0f) / 9.81;
    forceOut = round(forceOut * 1000) / 1000.0f;
    // 2 mm tube radius, 1000 for kPa -> Pa conversion, 9.81 for N -> kg conversion, 1000 for kg -> g conversion
    // https://www.ucl.ac.uk/~uceseug/Fluids2/Notes_Momentum.pdf
    // Bernoulli's equation, constant height/horizontal tube, v = 2*sqrt*(Pressure/density), F = density*(area)*(v^2) = 2*area(m^2)*P
    Serial.print("Force(g):");
    Serial.println(forceOut);
    Serial.print("Position(steps):");
    Serial.println(motorPosition);
    #if PLATFORM_ESP32
    if(writingFile) { // CSV format for data processing
      f.print(currentMs);
      f.print(",");
      f.print(forceOut);
      f.print(",");
      f.print(pressureOut);
      f.print(",");
      f.println(motorPosition);
    }
    #endif

    lastRefresh = millis();
  }

  #if PLATFORM_ESP32
    if(toCalibrate && motorPosition > 295) { // Run calibration if ready
      calibrate(); // Run calibration
      motorTurn(0); // Move motor back to 0 when complete
    }

    if(millis() - lastPxRefresh > 50) { // Update NeoPixel with motor speed and position
      uint8_t pixelBrightness = (150 * motorPosition / TURN_LIMIT / 2);
      pixel.setPixelColor(0, pixel.Color(pixelRed, 0, pixelBlue));
      pixel.setBrightness(pixelBrightness);
      pixel.show();
      lastPxRefresh = millis();
    }
  #endif

  lastMotorState = (stepsToTurn > 0);
  lastTimerState = timerState;
}