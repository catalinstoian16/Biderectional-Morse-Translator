#include <Wire.h>    
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);

#define PANIC 2
#define DIT 4
#define DAH 7
#define BREAK 8
#define BUZZER 11
#define LED 12
#define POTENT A0
#define FREQ 300

#define SOS "...,---,...;"

#define VERBOSE false

String morse_alphabet[] = {
  ".-", "-...", "-.-.",         //ABC
  "-..", ".", "..-.",           //DEF
  "--.", "....", "..",          //GHI
  ".---", "-.-", ".-..",        //JKL
  "--", "-.", "---",            //MNO
  ".--.", "--.-", ".-.", "...", //PQRS
  "-", "..-", "...-",           //TUV
  ".--", "-..-", "-.--", "--.." //WXYZ
};

String translate_c2m(char c) {
  if (c >= 'a' && c <= 'z') {
    return morse_alphabet[c - 'a'];
  }
  else if (c >= 'A' && c <= 'Z') {
    return morse_alphabet[c - 'A'];
  }
}

char translate_m2c(String morse) {
  for (int i = 0; i < 26; i++) {
    if (morse == morse_alphabet[i]) {
      return ('A' + i);
    }
  }
  return '\n';
}

String sentence2morse(String sentence) {
  String result = "";
  int len = sentence.length();

  for (int i = 0; i < len; i++) {
    if (sentence.charAt(i) == ' ') {
      result += ';';
      continue;
    }
    result += translate_c2m(sentence.charAt(i));
    if (i != len - 1 && sentence.charAt(i + 1) != ' ')
      result += ',';
  }
  return result;
}

String morse2sentence(String morse) {
  String sentence = "";
  String code = "" + morse;
  String temp = "";

  code.replace(",,,", "");
  code.replace(",,", ";");

  int len = code.length();

  for (int i = 0; i < len; i++) {
    if (i == len - 1 || code.charAt(i) == ',' || code.charAt(i) == ';') {
      if (i == len - 1)
        temp += code.charAt(i);
      for (int j = 0; j < 26; j++) {
        if (temp == morse_alphabet[j]) {
          char c = 'A';
          c += j;
          sentence += c;
          temp = "";
          if (code.charAt(i) == ';')
            sentence += ' ';
        }
      }
    }
    else
      temp += code.charAt(i);
  }

  return sentence;
}

int panicButtonState;
int lastPanicButtonState;
unsigned long lastPanicDebounceTime = 0;

int ditButtonState;
int lastDitButtonState = HIGH;
unsigned long lastDitDebounceTime = 0;

int dahButtonState;
int lastDahButtonState = HIGH;
unsigned long lastDahDebounceTime = 0;

int breakButtonState;
int lastBreakButtonState = HIGH;
unsigned long lastBreakDebounceTime = 0;

bool panic = false;
bool translating2morse = false;
bool translating2latin = false;

unsigned long debounceDelay = 50;

void setup() {
  Serial.begin(9600);
 
  pinMode(PANIC, INPUT_PULLUP);
  panicButtonState = digitalRead(PANIC);
  lastPanicButtonState = panicButtonState;
  pinMode(DIT, INPUT_PULLUP);
  ditButtonState = digitalRead(DIT);
  lastDitButtonState = ditButtonState;
  pinMode(DAH, INPUT_PULLUP);
  dahButtonState = digitalRead(DAH);
  lastDahButtonState = dahButtonState;
  pinMode(BREAK, INPUT_PULLUP);
  breakButtonState = digitalRead(BREAK);
  lastBreakButtonState = breakButtonState;
  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  lcd.init();                     
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("   Waiting to");
  lcd.setCursor(0,1);
  lcd.print("  translate...");
}

String buffer = "";
String result = "";

long ptime = 0;
long ttime = 0;
int index = -1;
int potent_read;
int dit_time;
int duration;
bool up_down;
int break_count = 0;
long ltime = 0;
int lcd_index = 0;

void loop() {
  // Panic button debounce
  int readingPanic = digitalRead(PANIC);
  if (readingPanic != lastPanicButtonState)
    lastPanicDebounceTime = millis();

  if ((millis() - lastPanicDebounceTime) > debounceDelay) {
    if (readingPanic != panicButtonState) {
      panicButtonState = readingPanic;

      if (panicButtonState == HIGH) {
        panic = !panic;
        noTone(BUZZER);
        lcd.clear();
        digitalWrite(LED, LOW);
        if (!panic) {
          translating2morse = false;
          result = "";
          index = -1;
          lcd_index = 0;

          lcd.setCursor(0,0);
          lcd.print("   Waiting to");
          lcd.setCursor(0,1);
          lcd.print("  translate...");
        }
        else {
          if (translating2latin) {
            lcd_index = 0;
            Serial.println("Aborting translation...");
          }
          buffer = "";
          result = SOS;
          translating2morse = true;
          translating2latin = false;
          index = 0;
          up_down = true;
          duration = 0;
          ttime = millis();

          lcd.setCursor(0,0);
          lcd.print("  Transmitting");
          lcd.setCursor(0,1);
          lcd.print("     SOS...");
        }
      }
    }
  }
  lastPanicButtonState = readingPanic;


  // Break button debounce + start morse to latin translation
  if (!panic && !translating2morse && !translating2latin) {
    int readingBreak = digitalRead(BREAK);
    if (readingBreak != lastBreakButtonState)
      lastBreakDebounceTime = millis();

    if ((millis() - lastBreakDebounceTime) > debounceDelay) {
      if (readingBreak != breakButtonState) {
        breakButtonState = readingBreak;

        if (breakButtonState == HIGH) {
          translating2latin = true;
          break_count = 0;
          lcd_index = 0;
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Translating M2L:");
          lcd.setCursor(0,1);
        }
      }
    }
    lastBreakButtonState = readingBreak;
  }


  // Dit, dah, break debounce + translation
  if (!panic && translating2latin) {
    int readingDit = digitalRead(DIT);
    if (readingDit != lastDitButtonState)
      lastDitDebounceTime = millis();

    if ((millis() - lastDitDebounceTime) > debounceDelay) {
      if (readingDit != ditButtonState) {
        ditButtonState = readingDit;

        if (ditButtonState == HIGH) {
          result += '.';
          lcd.setCursor(0, 1);
          if (result.length() <= 16)
            lcd.print(result);
          else
            lcd.print(result.substring(result.length() - 16, result.length()));

          if (VERBOSE)
            Serial.println(result);
          break_count = 0;
        }
      }
    }
    lastDitButtonState = readingDit;


    int readingDah = digitalRead(DAH);
    if (readingDah != lastDahButtonState)
      lastDahDebounceTime = millis();

    if ((millis() - lastDahDebounceTime) > debounceDelay) {
      if (readingDah != dahButtonState) {
        dahButtonState = readingDah;

        if (dahButtonState == HIGH) {
          result += '-';
          lcd.setCursor(0, 1);
          if (result.length() <= 16)
            lcd.print(result);
          else
            lcd.print(result.substring(result.length() - 16, result.length()));

          if(VERBOSE)
            Serial.println(result);
          break_count = 0;
        }
      }
    }
    lastDahButtonState = readingDah;


    int readingBreak = digitalRead(BREAK);
    if (readingBreak != lastBreakButtonState)
      lastBreakDebounceTime = millis();

    if ((millis() - lastBreakDebounceTime) > debounceDelay) {
      if (readingBreak != breakButtonState) {
        breakButtonState = readingBreak;

        if (breakButtonState == HIGH) {
          result += ',';
          lcd.setCursor(0, 1);
          if (result.length() <= 16)
            lcd.print(result);
          else
            lcd.print(result.substring(result.length() - 16, result.length()));

          if (VERBOSE)
            Serial.println(result);
          break_count++;
          if (break_count == 3) {
            String temp = morse2sentence(result);
            Serial.println("Translation is " + temp);
            result = "";
            break_count = 0;
            translating2latin = false;
 
            lcd_index = 0;
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("   Waiting to");
            lcd.setCursor(0,1);
            lcd.print("  translate...");
          }
        }
      }
    }
    lastBreakButtonState = readingBreak;
  }

  
  // Potentiometer reading
  if ((millis() - ptime) >= 100) {
    ptime = millis();
    potent_read = analogRead(POTENT);
    dit_time = map(potent_read, 0, 1023, 100, 1000);
  }

  // Non-blocking morse transmission
  if (translating2morse && ((millis() - ttime) >= duration)) {
    if (up_down) {
      if (result.charAt(index) == '.') {
        digitalWrite(LED, HIGH);
        tone(BUZZER, FREQ);
        duration = dit_time;
      }
      else if (result.charAt(index) == '-') {
        digitalWrite(LED, HIGH);
        tone(BUZZER, FREQ);
        duration = 3 * dit_time;
      }
      else if (result.charAt(index) == ',')
        duration = 2 * dit_time;
      else if (result.charAt(index) == ';')
        duration = 6 * dit_time;
    }
    else {
      noTone(BUZZER);
      digitalWrite(LED, LOW);
      duration = dit_time;
      index++;
    }

    up_down = !up_down;
    ttime = millis();
  }

  // Display scrolling
  if (translating2morse && buffer.length() > 16) {
    if ((millis() - ltime) >= 1000) {
      if (lcd_index == buffer.length() - 16)
        lcd_index = -1;
      ltime = millis();
      lcd_index++;
      lcd.setCursor(0, 1);
      lcd.print(buffer.substring(lcd_index, lcd_index + 16));
    }
  }

  // End of morse message
  if (index == result.length()) {
    if (!panic) {
      Serial.println("Finished transmitting " + buffer + ".");
      translating2morse = false;
      result = "";
      buffer = "";
      index = -1;

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("   Waiting to");
      lcd.setCursor(0,1);
      lcd.print("  translate...");
    }
    else
      index = 0;
  }


  // Translate message from serial and initiate translating
  if (!panic && !translating2morse && Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      Serial.println("Transmitting " + buffer + "...");
      result = sentence2morse(buffer);
      
      translating2morse = true;
      index = 0;
      up_down = true;
      duration = 0;
      ttime = millis();

      lcd_index = 0;
      ltime = millis();
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Translating L2M:");
      lcd.setCursor(0,1);
      lcd.print(buffer);
    }
    else {
      buffer += c;
    }
  }
}
