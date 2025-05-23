#include <Wire.h>
#include "Olimex16x2.h"
#include <Servo.h>
#include <Arduino.h>

Olimex16x2 lcd(&Wire);
Servo myServo;

constexpr uint8_t BUTTON_1   = 0; // +30 min, Debug: Reverse
constexpr uint8_t BUTTON_2   = 1; // +1 hour, Debug: Forward
constexpr uint8_t BUTTON_3   = 2; // Start/Servo Stop
constexpr uint8_t BUTTON_4   = 3; // Reset/Debug Exit

constexpr int SERVO_PIN = 7;
constexpr int SERVO_STOP = 90;        // Stop for 360° servo
constexpr int SERVO_REVERSE = 0;      // Full speed reverse
constexpr int SERVO_FORWARD = 180;    // Full speed forward

int totalSeconds = 0;
bool timerStarted = false;
bool postTimerMenu = false;
bool debugMode = false;

unsigned long lastTick = 0;
bool isRewinding = false;
unsigned long rewindStartTime = 0;
constexpr unsigned long rewindDuration = 13000;

unsigned long button4HoldStart = 0;
bool button4Held = false;

enum Mode { NONE, PLUS_30, PLUS_1H };
Mode lastUsedMode = NONE;
Mode currentPress = NONE;
bool modeSelected = false;

constexpr unsigned long inactiveTimeout = 5UL * 60UL * 1000UL; // 5 minutes
unsigned long lastActivityMillis = 0;
bool screenDimmed = false;

constexpr unsigned long SERIAL_PRINT_INTERVAL = 3000;
unsigned long lastSerialPrint = 0;

constexpr size_t SERIAL_BUF_SIZE = 32;
char serialBuffer[SERIAL_BUF_SIZE];
byte serialIndex = 0;

bool prevBtn1 = false, prevBtn2 = false, prevBtn3 = false, prevBtn4 = false;

// Fun: Joke bank for serial command 'joke'
const char* jokes[] = {
  "Why do programmers prefer dark mode? Because light attracts bugs.",
  "Why do Java developers wear glasses? Because they don't see sharp.",
  "How many programmers does it take to change a light bulb? None. It's a hardware problem.",
  "What do you call 8 hobbits? A hobbyte.",
  "Why did the developer go broke? Because he used up all his cache.",
  "Knock knock. Who's there? Recursion. Recursion who? Knock knock.",
  "Why do Python programmers wear glasses? Because they can't C.",
  "Real programmers count from 0.",
  "I'm not lazy. I'm on energy-saving mode.",
  "To understand recursion you must first understand recursion."
};

bool timerIsDefault = true;

void processSerialCommand(char* command);
void showSetupScreen();
void updateTimerDisplay();
void updateCountdownDisplay();
void resetTimerState();
void showPostTimerMenu();
void printSerialStatus(bool showButtons, bool showMode, bool showMemory, bool showTime);
void tellRandomJoke();
void lcdAnimation();
void displayUptime();
void fortune();

void lcdUpdateLine(uint8_t line, const char* text) {
  lcd.clearLine(line);
  lcd.drawLine(line, text);
}

bool debounceBtn(bool &prev, bool current) {
  bool pressed = (!prev && current);
  prev = current;
  return pressed;
}

int freeMemory() { return -1; }

void showSetupScreen() {
  lcd.clear();
  lcdUpdateLine(0, "Set Timer:");
  lcdUpdateLine(1, "1:+30 2:+1h 3:Go");
}

// Fun: fortunes for serial "fortune" command
const char* fortunes[] = {
  "You will debug successfully today.",
  "A surprise awaits in your code.",
  "A wild bug appears!",
  "Happiness is a warm microcontroller.",
  "The answer is always 42.",
  "Your project will compile on the first try.",
  "Beware the infinite loop.",
  "You will find an easter egg soon.",
  "Today is a good day to flash firmware.",
  "Debugging is like being the detective in a crime movie where you are also the murderer."
};

void updateTimerDisplay() {
  char buffer[17];
  int hours = totalSeconds / 3600;
  int minutes = (totalSeconds % 3600) / 60;
  if (hours > 0)
    snprintf(buffer, sizeof(buffer), "%dh %02dm", hours, minutes);
  else
    snprintf(buffer, sizeof(buffer), "%2d min", minutes);
  lcdUpdateLine(0, "Set Timer:");
  lcdUpdateLine(1, "3:Start 4:Reset");
}

void updateCountdownDisplay() {
  char buffer[17];
  int hours = totalSeconds / 3600;
  int minutes = (totalSeconds % 3600) / 60;
  int seconds = totalSeconds % 60;
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, seconds);
  lcdUpdateLine(0, "Time left");
  lcdUpdateLine(1, buffer);
}

void showPostTimerMenu() {
  lcd.clear();
  lcdUpdateLine(0, "Time's up!");
  lcdUpdateLine(1, "2:Rewd 4:Reset");
}

void resetTimerState() {
  totalSeconds = 5 * 60 * 60;     // Reset to 5 hours
  timerStarted = false;
  postTimerMenu = false;
  debugMode = false;
  isRewinding = false;
  lastUsedMode = NONE;
  currentPress = NONE;
  modeSelected = false;
  timerIsDefault = true;          // Mark as default again

  if(myServo.attached()) myServo.detach();
  updateTimerDisplay();           // Reflect new timer value
  showSetupScreen();
  delay(500);
}

// First button press after startup resets to zero, then adds. Further presses just add like normal.
void handleSmartSelection(Mode buttonMode, int addSeconds, const char* label) {
  char msg[17];
  if (timerIsDefault) {
    totalSeconds = 0;
    timerIsDefault = false;
  }
  if (lastUsedMode == buttonMode || (modeSelected && currentPress == buttonMode)) {
    totalSeconds += addSeconds;
    snprintf(msg, sizeof(msg), "Added %s", label);
    lcdUpdateLine(1, msg);
    delay(800);
    updateTimerDisplay();
    lastUsedMode = buttonMode;
    modeSelected = false;
  } else {
    currentPress = buttonMode;
    modeSelected = true;
    lcdUpdateLine(1, label);
  }
}

// DEBUG MODE for 360° Servo
void handleDebugMode(bool btn1, bool btn2, bool btn3, bool btn4) {
  static bool wasAttached = false;
  if (!wasAttached) {
    myServo.attach(SERVO_PIN);
    wasAttached = true;
    lcd.clear();
    lcdUpdateLine(0, "DEBUG MODE");
    lcdUpdateLine(1, "1:Rev 2:Fwd 3:Sto");
    delay(1200);
    lcdUpdateLine(0, "DEBUG MODE");
    lcdUpdateLine(1, "4:Exit");
  }
  if (debounceBtn(prevBtn1, btn1)) {
    myServo.write(SERVO_REVERSE);
    lcdUpdateLine(1, "Servo: REVERSE");
  }
  if (debounceBtn(prevBtn2, btn2)) {
    myServo.write(SERVO_FORWARD);
    lcdUpdateLine(1, "Servo: FORWARD");
  }
  if (debounceBtn(prevBtn3, btn3)) {
    myServo.write(SERVO_STOP);
    lcdUpdateLine(1, "Servo: STOPPED");
  }
  if (debounceBtn(prevBtn4, btn4)) {
    myServo.write(SERVO_STOP);
    myServo.detach();
    wasAttached = false;
    lcdUpdateLine(0, "Exit debug...");
    lcdUpdateLine(1, "");
    delay(800);
    debugMode = false;
    showSetupScreen();
  }
}

void handleSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialIndex > 0) {
        serialBuffer[serialIndex] = 0;
        processSerialCommand(serialBuffer);
        serialIndex = 0;
      }
    } else if (serialIndex < SERIAL_BUF_SIZE - 1) {
      serialBuffer[serialIndex++] = c;
    } else {
      serialIndex = 0;
      Serial.println(F("Serial buffer overflow! Command too long."));
    }
  }
}

void tellRandomJoke() {
  int n = sizeof(jokes) / sizeof(jokes[0]);
  int idx = random(n);
  Serial.println(jokes[idx]);
}

void fortune() {
  int n = sizeof(fortunes) / sizeof(fortunes[0]);
  int idx = random(n);
  Serial.print(F("Fortune: "));
  Serial.println(fortunes[idx]);
}

void lcdAnimation() {
  // Fun: LCD spinning bar animation on timer complete
  char frames[][17] = {
    "|               ",
    " |              ",
    "  |             ",
    "   |            ",
    "    |           ",
    "     |          ",
    "      |         ",
    "       |        ",
    "        |       ",
    "         |      ",
    "          |     ",
    "           |    ",
    "            |   ",
    "             |  ",
    "              | ",
    "               |"
  };
  for (int i = 0; i < 16; ++i) {
    lcdUpdateLine(1, frames[i]);
    delay(60);
  }
  for (int i = 15; i >= 0; --i) {
    lcdUpdateLine(1, frames[i]);
    delay(60);
  }
}

void displayUptime() {
  unsigned long ms = millis();
  unsigned long s = ms / 1000;
  unsigned long m = s / 60;
  unsigned long h = m / 60;
  s %= 60;
  m %= 60;
  Serial.print(F("Uptime: "));
  Serial.print(h);
  Serial.print(F("h "));
  Serial.print(m);
  Serial.print(F("m "));
  Serial.print(s);
  Serial.println(F("s"));
}

void processSerialCommand(char* command) {
  if (strcasecmp(command, "help") == 0) {
    Serial.println(F("Commands:"));
    Serial.println(F("  status    - Show timer status"));
    Serial.println(F("  reset     - Reset timer and servo"));
    Serial.println(F("  start     - Start timer"));
    Serial.println(F("  add30     - Add 30 minutes"));
    Serial.println(F("  add1h     - Add 1 hour"));
    Serial.println(F("  debug     - Toggle debug mode"));
    Serial.println(F("  servo on  - Attach servo"));
    Serial.println(F("  servo off - Detach servo"));
    Serial.println(F("  pause     - Pause timer"));
    Serial.println(F("  resume    - Resume paused timer"));
    Serial.println(F("  time      - Show time left"));
    Serial.println(F("  uptime    - Show device uptime"));
    Serial.println(F("  add10     - Add 10 seconds"));
    Serial.println(F("  sub10     - Subtract 10 seconds"));
    Serial.println(F("  mem       - Show free memory"));
    Serial.println(F("  servo dance - Make servo dance"));
    Serial.println(F("  joke      - Tell a random joke"));
    Serial.println(F("  fortune   - Get your fortune"));
    Serial.println(F("  beep      - Make beep sound (via serial)"));
    Serial.println(F("  about     - About this project"));
    Serial.println(F("  flip      - Flip servo back and forth"));
  }
  else if (strcasecmp(command, "about") == 0) {
    Serial.println(F("Fun Timer Project v1.0 - By Lankin1010, 2025"));
    Serial.println(F("GitHub: github.com/Lankin1010/glowing-funicular"));
  }
  else if (strcasecmp(command, "status") == 0) printSerialStatus(true, true, true, true);
  else if (strcasecmp(command, "reset") == 0) { resetTimerState(); Serial.println(F("Timer reset.")); }
  else if (strcasecmp(command, "start") == 0) {
    if (totalSeconds > 0) { timerStarted = true; lastTick = millis(); Serial.println(F("Timer started.")); }
    else Serial.println(F("Timer is zero, cannot start."));
  }
  else if (strcasecmp(command, "pause") == 0) {
    if (timerStarted) { timerStarted = false; Serial.println(F("Timer paused.")); }
    else Serial.println(F("Timer is not running."));
  }
  else if (strcasecmp(command, "resume") == 0) {
    if (!timerStarted && totalSeconds > 0) { timerStarted = true; lastTick = millis(); Serial.println(F("Timer resumed.")); }
    else if (timerStarted) Serial.println(F("Timer already running."));
    else Serial.println(F("No time to resume."));
  }
  else if (strcasecmp(command, "time") == 0) {
    int h = totalSeconds / 3600, m = (totalSeconds % 3600) / 60, s = totalSeconds % 60;
    Serial.print(F("Time left: "));
    if (h > 0) Serial.print(h), Serial.print(F("h "));
    Serial.print(m); Serial.print(F("m ")); Serial.print(s); Serial.println(F("s"));
  }
  else if (strcasecmp(command, "uptime") == 0) { displayUptime(); }
  else if (strcasecmp(command, "add10") == 0) { totalSeconds += 10; updateTimerDisplay(); Serial.println(F("Added 10 seconds.")); }
  else if (strcasecmp(command, "sub10") == 0) {
    if (totalSeconds > 10) { totalSeconds -= 10; updateTimerDisplay(); Serial.println(F("Subtracted 10 seconds.")); }
    else Serial.println(F("Cannot subtract, timer too low."));
  }
  else if (strcasecmp(command, "mem") == 0) {
    Serial.print(F("Free memory: ")); Serial.println(F("(not available on this board)"));
  }
  else if (strcasecmp(command, "servo dance") == 0) {
    Serial.println(F("Servo dance time!"));
    myServo.attach(SERVO_PIN);
    for (int i = 0; i < 3; i++) {
      myServo.write(SERVO_REVERSE); delay(300); myServo.write(SERVO_FORWARD); delay(300);
    }
    myServo.write(SERVO_STOP); myServo.detach();
  }
  else if (strcasecmp(command, "joke") == 0) { tellRandomJoke(); }
  else if (strcasecmp(command, "fortune") == 0) { fortune(); }
  else if (strcasecmp(command, "beep") == 0) Serial.println(F("*beep beep beep*"));
  else if (strcasecmp(command, "flip") == 0) {
    Serial.println(F("Flipping servo back and forth!"));
    myServo.attach(SERVO_PIN);
    myServo.write(45); delay(400); myServo.write(135); delay(400); myServo.write(45); delay(400);
    myServo.write(SERVO_STOP); myServo.detach();
  }
  else if (strcasecmp(command, "add30") == 0) { totalSeconds += 1800; updateTimerDisplay(); Serial.println(F("Added 30 minutes.")); }
  else if (strcasecmp(command, "add1h") == 0) { totalSeconds += 3600; updateTimerDisplay(); Serial.println(F("Added 1 hour.")); }
  else if (strcasecmp(command, "debug") == 0) { debugMode = !debugMode; Serial.print(F("Debug mode is now ")); Serial.println(debugMode ? "ON" : "OFF"); }
  else if (strcasecmp(command, "servo on") == 0) { myServo.attach(SERVO_PIN); myServo.write(SERVO_STOP); Serial.println(F("Servo attached.")); }
  else if (strcasecmp(command, "servo off") == 0) { myServo.write(SERVO_STOP); myServo.detach(); Serial.println(F("Servo detached.")); }
  else { Serial.print(F("Unknown command: ")); Serial.println(command); }
  Serial.println();
}

void printSerialStatus(bool showButtons, bool showMode, bool showMemory, bool showTime) {
  Serial.println(F("\n=== Timer Status ==="));
  Serial.print(F("Timer Duration: ")); Serial.print(totalSeconds); Serial.println(F(" seconds"));
  Serial.print(F("Timer Running: ")); Serial.println(timerStarted ? "Yes" : "No");
  Serial.print(F("Post Timer Menu: ")); Serial.println(postTimerMenu ? "Yes" : "No");
  if (showMemory) {
    Serial.print(F("Free Memory: ")); Serial.println(F("(not available on this board)"));
  }
  if (showTime) {
    Serial.print(F("Millis uptime: ")); Serial.print(millis()); Serial.println(F(" ms"));
    Serial.print(F("Total ms counted: ")); Serial.print((unsigned long)totalSeconds * 1000UL); Serial.println(F(" ms"));
  }
  if (showButtons) {
    Serial.println(F("\nButton Assignments:"));
    Serial.println(F("  Btn1 = +30 min"));
    Serial.println(F("  Btn2 = +1 hour"));
    Serial.println(F("  Btn3 = Start/Stop"));
    Serial.println(F("  Btn4 = Reset/Debug (hold 3s)"));
  }
  Serial.println();
}

void setup() {
  Serial.begin(9600);
  while (!Serial) delay(10);
  Wire.begin();
  lcd.begin();
  lcd.setBacklight(255);
  lcd.clear();

  totalSeconds = 5 * 60 * 60; // 5 hours at startup
  timerIsDefault = true;      // Mark timer as still default at startup
  updateTimerDisplay();

  showSetupScreen();
  lastActivityMillis = millis();

  Serial.println(F("=== Timer Initialized ==="));
  Serial.println(F("Type 'help' for commands."));
  Serial.println();
}

void loop() {
  unsigned long now = millis();

  handleSerialInput();

  bool btn1   = lcd.readButton(BUTTON_1);
  bool btn2   = lcd.readButton(BUTTON_2);
  bool btn3   = lcd.readButton(BUTTON_3);
  bool btn4   = lcd.readButton(BUTTON_4);

  if (btn1 && btn2 && btn3 && btn4) {
    lcd.clear();
    lcdUpdateLine(0, "Secret Menu!");
    lcdUpdateLine(1, "Hello, Hacker :)");
    delay(1400);
    showSetupScreen();
  }

  if (btn1 || btn2 || btn3 || btn4) {
    lastActivityMillis = now;
    if (screenDimmed) {
      screenDimmed = false;
      lcd.setBacklight(255);
      delay(200);
      return;
    }
  }

  if (!screenDimmed && (now - lastActivityMillis > inactiveTimeout)) {
    screenDimmed = true;
    lcd.setBacklight(20);
  }

  if (btn4) {
    if (!button4Held) {
      button4HoldStart = now;
      button4Held = true;
    } else if (now - button4HoldStart >= 3000) {
      debugMode = !debugMode;
      button4Held = false;
      if (debugMode) {
        lcd.clear();
        lcdUpdateLine(0, "DEBUG MODE");
        lcdUpdateLine(1, "1:Rev 2:Fwd 3:Sto");
        delay(1200);
        lcdUpdateLine(0, "DEBUG MODE");
        lcdUpdateLine(1, "4:Exit");
      } else {
        showSetupScreen();
        delay(500);
      }
    }
  } else {
    button4Held = false;
  }

  if (debugMode) {
    handleDebugMode(btn1, btn2, btn3, btn4);
    if (now - lastSerialPrint >= 1000) {
      printSerialStatus(true, true, true, true);
      lastSerialPrint = now;
    }
    return;
  }

  if (!timerStarted && !postTimerMenu) {
    if (debounceBtn(prevBtn1, btn1))   handleSmartSelection(PLUS_30, 30 * 60, "+30 min");
    if (debounceBtn(prevBtn2, btn2))   handleSmartSelection(PLUS_1H, 60 * 60, "+1 hour");
    if (debounceBtn(prevBtn3, btn3) && totalSeconds > 0) {
      timerStarted = true;
      lastTick = now;
      lcd.clear();
      updateCountdownDisplay();
    }
  }

  if (timerStarted) {
    if ((now - lastTick) >= 1000) {
      lastTick = now;
      if (totalSeconds > 0) {
        totalSeconds--;
        updateCountdownDisplay();
        Serial.print(F("Tick: "));
        Serial.println(totalSeconds);
      } else {
        timerStarted = false;
        isRewinding = true;
        rewindStartTime = now;
        lcd.clear();
        lcdUpdateLine(0, "Rewinding...");
        myServo.attach(SERVO_PIN);
        myServo.write(SERVO_REVERSE);
        lcdAnimation();
      }
    }
    if (debounceBtn(prevBtn4, btn4)) {
      resetTimerState();
    }
  }

  if (isRewinding && now - rewindStartTime >= rewindDuration) {
    isRewinding = false;
    myServo.write(SERVO_STOP);
    delay(200);
    myServo.detach();
    postTimerMenu = true;
    showPostTimerMenu();
  }

  if (postTimerMenu) {
    if (btn2) {
      if (!myServo.attached()) myServo.attach(SERVO_PIN);
      myServo.write(SERVO_REVERSE);
    } else {
      if (myServo.attached()) {
        myServo.write(SERVO_STOP);
        myServo.detach();
      }
    }
    if (debounceBtn(prevBtn4, btn4)) {
      if (myServo.attached()) {
        myServo.write(SERVO_STOP);
        myServo.detach();
      }
      resetTimerState();
      delay(100);
      NVIC_SystemReset();
    }
  }

  if (now - lastSerialPrint >= SERIAL_PRINT_INTERVAL) {
    printSerialStatus(true, true, true, true);
    lastSerialPrint = now;
  }
}

// End of file (line 500)
