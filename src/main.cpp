
#include <Arduino.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "lgfx_ESP32_2432S028.h"
#include <driver/dac.h>
//#include <SPI.h>
#include <SD.h>

static LGFX screen;  // declare display variable
static LGFX_Sprite lcd(&screen); 

#define ADC_PIN 34
#define TRIGGER_PIN 22
#define ECHO_PIN  27
#define DAC_PIN  26

#define SDMOSI 23
#define SDMISO 19
#define SDCLK 18
#define SDCS 5
#define SDSPEED 27000000

 #define MY_TFT_BLACK 0
  #define MY_TFT_BLUE 1
  #define MY_TFT_GREEN 2
  #define MY_TFT_RED 3
  #define MY_TFT_GOLD 4
  #define MY_TFT_NAVY 5 
  #define MY_TFT_YELLOW 6
  #define MY_TFT_MAROON 7
  #define MY_TFT_CYAN 8
  #define MY_TFT_WHITE 9
  #define MY_TFT_DARKGRAY 10
  #define MY_TFT_LIGHTGRAY 11

class LCDRect
{
  public: 
    int x;
    int y;
    int w;
    int h;
    int c;
    String text;
    bool third = false;
    LGFX_Sprite* lcd;
    
    LCDRect(LGFX_Sprite* ldcInstance)
    {
      lcd = ldcInstance;
    }

    void Draw()
    {    
      lcd->fillRect(x,y,w,h,c);
      lcd->drawRect(x,y,w,h,MY_TFT_DARKGRAY);
      lcd->drawRect(x+1,y+1,w-2,h-2,MY_TFT_WHITE);
      lcd->drawRect(x+2,y+2,w-4,h-4,MY_TFT_LIGHTGRAY);
      if (text.length() > 0)
      {        
        if (third)
        {
          lcd->drawString(text.c_str(), x + w / 2, y + h / 3);
        }
        else
        {
          lcd->drawString(text.c_str(), x + w / 2, y + h / 2);
        }
      }
    }

    bool Hits(int px, int py) 
    {
      if((px > x && px < x + w) && (py > y && py < y + h))
      {
        return true;
      }
      return false;
    }
};

#define audio_help_wave_buf_size 20
DRAM_ATTR u_int8_t audio_help_wave_buf[audio_help_wave_buf_size];

hw_timer_t *audio_help_timer = NULL;
portMUX_TYPE audio_help_timerMux = portMUX_INITIALIZER_UNLOCKED;
DRAM_ATTR int audio_help_wave_position = 0;

DRAM_ATTR int audio_help_dacPin = 26;

void IRAM_ATTR audio_help_onTimer()
{
  //portENTER_CRITICAL_ISR(&audio_help_timerMux);
  //digitalWrite(22, !digitalRead(22));
  // write the next val in the waveform to the dac
  u_int8_t val = audio_help_wave_buf[audio_help_wave_position];// * audio_help_volume;
  dac_output_voltage(DAC_CHANNEL_2, val);
  audio_help_wave_position++;
  if (audio_help_wave_position == audio_help_wave_buf_size){
    audio_help_wave_position = 0;}

  //portEXIT_CRITICAL_ISR(&audio_help_timerMux);
}

void audio_help_set_waveform(String signal_mode, float volume)
{
  //portENTER_CRITICAL_ISR(&audio_help_timerMux);

  float inc = (3.14159 * 2) / audio_help_wave_buf_size;
  // Serial.print("inc ");
  // Serial.println(inc, DEC);
  float ramp = 0;
  for (int x = 0; x < audio_help_wave_buf_size; x++)
  {
    //Serial.print(x);
    //Serial.print("=");
    if (signal_mode == "sine")
    {
      //Serial.print(sin(ramp));
      //Serial.print(":");
      audio_help_wave_buf[x] = volume * (((sin(ramp) / 2) + 0.5) * 255);
    }
    else if (signal_mode == "ramp")
    {
      audio_help_wave_buf[x] = volume * x;
    }
    else if (signal_mode == "square")
    {
      if (x > audio_help_wave_buf_size / 2)
        audio_help_wave_buf[x] = 0;
      else
        audio_help_wave_buf[x] = volume * 255;
    }
    
    //Serial.println(audio_help_wave_buf[x]);
    ramp = ramp + inc;
  }

  //portEXIT_CRITICAL_ISR(&audio_help_timerMux);
}

int audio_help_calc_interval(float freq)
{
  // 100 samples in the waveform
  // example: play 100 samples 1000 times per sec
  float totalChangesPerSec = audio_help_wave_buf_size * freq;
  // timer running at 80Mhz with a divisor of 8 is 10000000
  int interval = 10000000 / totalChangesPerSec;
  Serial.print("timer_interval ");
  Serial.println(interval);
  return interval;
}

void audio_help_setFrequency(float freq)
{  
  Serial.print("freq ");
  Serial.println(freq);

  int timer_interval = audio_help_calc_interval(freq);
  if (timer_interval == 0)
  {
    //set it too slow to hear;
    timer_interval = 2000;    
  }
  
  //Serial.println("timerAlarmDisable");
  timerAlarmDisable(audio_help_timer);
  //Serial.println("timerStop");
  timerStop(audio_help_timer);
  //Serial.println("timerAlarmWrite");  
  timerAlarmWrite(audio_help_timer, timer_interval, true);
  //Serial.println("timerRestart");
  timerRestart(audio_help_timer);
  //Serial.println("timerStart");
  timerStart(audio_help_timer);
  //Serial.println("timerAlarmEnable");
  timerAlarmEnable(audio_help_timer);  
}

void audio_help_initialize(int dacPinNumber)
{
  audio_help_dacPin = dacPinNumber;

  audio_help_set_waveform("square", 0);

  // let's start the frequency at 1000
  int timer_interval = audio_help_calc_interval(1000);

  Serial.println("timerBegin");
  audio_help_timer = timerBegin(3, 8, true);
  if (audio_help_timer)
  {
    Serial.println("timerSetAutoReload");
    timerSetAutoReload(audio_help_timer, true);
    Serial.println("timerAttachInterrupt");
    timerAttachInterrupt(audio_help_timer, &audio_help_onTimer, true);
    Serial.println("timerAlarmWrite");
    timerAlarmWrite(audio_help_timer, timer_interval, true);
    Serial.println("dac_output_enable");
    dac_output_enable(DAC_CHANNEL_2);
    Serial.println("timerAlarmEnable");
    timerAlarmEnable(audio_help_timer);
  }
}

struct RecordSettings
{
  fs::File recordFile;
  bool recording = false;
  bool idle = true;
};

RecordSettings recordVar;

// Variables for touch x,y
static int32_t x, y;

int chartWidth = 300;
int chartHeight = 180;
int tempAreaHeight = 30;

float* graphVals;
float scaler = 1;
long timer_initial = 0;

String alert = "";

void displayError(String err) 
{
  lcd.fillRect(0, 0, chartWidth, tempAreaHeight, MY_TFT_BLACK);
  lcd.setCursor(62, 12);
  lcd.printf(err.c_str());
  alert = err;
}

bool ui_drawn = false;
void clearDisplay() 
{
  //lcd->fillRect(0, 0, chartWidth, chartHeight + tempAreaHeight, MY_TFT_BLACK);
  lcd.clearDisplay(MY_TFT_BLACK);
  ui_drawn = false;
}

float high_alarm = 200;
float low_alarm = 75;
LCDRect high_button(&lcd);
LCDRect low_button(&lcd);
bool highAlarm = false;

float maxVal = 0;
float minVal = 1000;

void addVal(float val)
{
  int i = 0;
  for (i = chartWidth; i > 0; i--) {
    graphVals[i] = graphVals[i - 1];
  }

  graphVals[0] = val;

  if (val > high_alarm)
  {
    alert = "HIGH";
    highAlarm=true;
  }
  else
  {
    alert = "LOW";
    highAlarm=false;
  }
}

LCDRect timer(&lcd);

void displayGraph() 
{  
  int bottom = screen.height();

  for (int i = 0; i < chartWidth; i++) {
    lcd.drawLine(chartWidth - i, bottom - graphVals[i], chartWidth - i, bottom, MY_TFT_GREEN);
  }

  long now = millis() - timer_initial;
  long sec = now / 1000;
  long min = now / 1000 / 60;
  long hrs = now / 1000 / 60 / 60;
  sec = sec - min * 60;
  min = min - hrs * 60;

  String sec_str = String(sec);
  String min_str = String(min);
  String hrs_str = String(hrs);
  if (sec_str.length() == 1) {
    sec_str = String("0") + sec_str;
  }
  if (min_str.length() == 1) {
    min_str = String("0") + min_str;
  }
  if (hrs_str.length() == 1) {
    hrs_str = String("0") + hrs_str;
  }
  String time_str = String("Up Time: ");
  time_str.concat(hrs_str);
  time_str.concat(":");
  time_str.concat(min_str);
  time_str.concat(":");
  time_str.concat(sec_str);
  
  timer.w = 190;
  timer.h = 32;
  timer.x = screen.width() / 2 - timer.w / 2;
  timer.y = 2;  
  timer.text = time_str;
  timer.c = MY_TFT_NAVY;
  timer.Draw();

  int chart_inc = 50;
  int line_pos = screen.height() - chart_inc;
  while (line_pos > chart_inc) {
    lcd.drawLine(0, line_pos, chartWidth, line_pos, MY_TFT_CYAN);
    lcd.drawRightString(String(screen.height() - line_pos).c_str(), chartWidth - 5, line_pos);
    line_pos = line_pos - chart_inc;
  }

  String val("Max: ");
  val = val + String(maxVal);
  lcd.drawCenterString(val, chartWidth / 2, 48);
  val = String("Min: ");
  val = val + String(minVal);
  lcd.drawCenterString(val, chartWidth / 2, 68);
  lcd.setTextSize(1.4);
  //lcd.drawRightString(String(graphVals[0]).c_str(), chartWidth / 2 + 40, 95);
  lcd.drawCenterString(String(graphVals[0]).c_str(), chartWidth / 2, 120);
  //lcd.drawEllipse(chartWidth / 2 + 48, 87, 4, 4, MY_TFT_WHITE);  not a degree
  lcd.setTextSize(1);
  lcd.drawString( alert,screen.width() / 2, 150);  
}

void testSD()
{
  Serial.println("Testing SD");
  bool fileExists = SD.exists("/test2.txt");
  if (fileExists) {
    Serial.println("File exists");
  } else {
    Serial.println("File does not exist");
  }

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  recordVar.recordFile = SD.open("/test2.txt", FILE_WRITE, !fileExists);

  // if the file opened okay, write to it:
  if (recordVar.recordFile) {
    Serial.print("Writing to test.txt...");
    recordVar.recordFile.println("45.89");
    recordVar.recordFile.println("120");
    // close the file:
    recordVar.recordFile.close();
    Serial.println("done.");
  } else {
    // if the file didn't open, print an error:
    Serial.print("error opening test.txt, card size =");
    Serial.println(SD.cardSize() / (1024 * 1024));
  }
}

SPIClass sdSPI(HSPI);
double factor = 1;

void setup(void) {

  Serial.begin(115200);  
  while(!Serial){}

  uint32_t Freq = 0;
  Freq = getCpuFrequencyMhz();
  Serial.print("CPU Freq = ");
  Serial.print(Freq);
  Serial.println(" MHz");
  Freq = getXtalFrequencyMhz();
  Serial.print("XTAL Freq = ");
  Serial.print(Freq);
  Serial.println(" MHz");
  Freq = getApbFrequency();
  Serial.print("APB Freq = ");
  Serial.print(Freq);
  Serial.println(" Hz");

  screen.init();
  screen.setRotation(1);

  lcd.setColorDepth(lgfx::palette_4bit);
  lcd.createSprite(screen.width(), screen.height()); 
  lcd.setPaletteColor(MY_TFT_BLACK, TFT_BLACK); 
  lcd.setPaletteColor(MY_TFT_BLUE, TFT_BLUE); 
  lcd.setPaletteColor(MY_TFT_GREEN, TFT_GREEN);
  lcd.setPaletteColor(MY_TFT_RED, TFT_RED);
  lcd.setPaletteColor(MY_TFT_GOLD, TFT_GOLD);
  lcd.setPaletteColor(MY_TFT_NAVY, TFT_NAVY); 
  lcd.setPaletteColor(MY_TFT_YELLOW, TFT_YELLOW);
  lcd.setPaletteColor(MY_TFT_MAROON, TFT_MAROON);
  lcd.setPaletteColor(MY_TFT_CYAN, TFT_CYAN);
  lcd.setPaletteColor(MY_TFT_WHITE, TFT_WHITE);
  lcd.setPaletteColor(MY_TFT_DARKGRAY, TFT_DARKGRAY);
  lcd.setPaletteColor(MY_TFT_LIGHTGRAY, TFT_LIGHTGRAY);

  lcd.fillScreen(MY_TFT_BLUE);  
  lcd.setCursor(10, 10);

  //lcd.setTextSize(2);
  lcd.setTextDatum(lgfx::textdatum::CC_DATUM);
  lcd.setFont(&lgfx::v1::fonts::DejaVu18);
  //lcd.setTextColor(MY_TFT_WHITE, MY_TFT_TRANSPARENT);
  
  chartWidth = screen.width();
  Serial.print("width: ");
  Serial.println(chartWidth);
  chartHeight = screen.height() - tempAreaHeight;
  lcd.drawLine(0, 0, chartWidth, 0, MY_TFT_GOLD);
  lcd.drawLine(0, 0, 0, chartHeight, MY_TFT_GOLD);
  lcd.drawLine(chartWidth, chartHeight, 0, chartHeight, MY_TFT_GOLD);
  lcd.drawLine(chartWidth, chartHeight, chartWidth, 0, MY_TFT_GOLD);

  graphVals = (float*)malloc(sizeof(float) * chartWidth);
  int i = 0;
  for (i = 0; i < chartWidth; i++) {
    graphVals[i] = 0;
  }

  /*use the a2d convertor to read the photo resistor*/
  //this adc pin is for the photo resistor
  adcAttachPin(ADC_PIN);
  //analogSetClockDiv(128);
  //analogSetCycles(4);
  analogSetAttenuation(ADC_0db);

  //factor = screen.height() / (double) 200; //200cm or 2m 
  Serial.print("ADC to screen heght factor=");
  Serial.println(factor);

  pinMode(SDCS, OUTPUT);
  digitalWrite(SDCS, LOW);
  Serial.println("Initializing SPI HSPI_HOST for SD...");

  sdSPI.begin(SDCLK, SDMISO, SDMOSI, SDCS);

  Serial.println("Initializing SD card...");  
  if (!SD.begin(SDCS, sdSPI, SDSPEED)) {
    Serial.println("init failed!");
  }
  Serial.println("init done.");

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  screen.startWrite();
  lcd.printf("Initializing Audio");
  lcd.pushSprite(0, 0);

  pinMode(22, OUTPUT);
  audio_help_initialize(DAC_PIN);
  //play a test tone at start up
  Serial.println("test tone on at 0.01");
  audio_help_set_waveform("sine", 0.01);
  delay(500);
  Serial.println("test tone 750");
  audio_help_setFrequency(750);
  delay(1000);
  Serial.println("test tone 400");
  audio_help_setFrequency(400);
  delay(500);
  Serial.println("test tone 2000");
  audio_help_setFrequency(2000);
  delay(250);
  Serial.println("test tone volume 0");
  audio_help_set_waveform("sine", 0); 
  delay(500);

  testSD();

  Serial.println("ultrasonic sensor");
  // Define inputs and outputs
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

}

const int UI_IDLE = 0;
const int UI_CONFIRM_RECORDER = 1;

const int UI_MOVE_HIGH_ALARM = 2;
const int UI_MOVE_LOW_ALARM = 3;

int ui_mode = UI_IDLE;
String ui_question = "";

LCDRect left_button(&lcd);
LCDRect center_button(&lcd);
LCDRect right_button(&lcd);

void manageUI()
{
  if (ui_mode == UI_IDLE)
  {
    return;
  }

  if (ui_mode == UI_CONFIRM_RECORDER)
  {
    ui_question = "Recording";
  }

  if (ui_drawn)
  {
    return;
  }

  if (ui_question.length() > 0)
  {
    LCDRect prompt(&lcd);
    prompt.x = 30;
    prompt.w = screen.width() - 60;
    prompt.y = 80;
    prompt.h = screen.height() - 120;
    prompt.text = ui_question;
    prompt.c = MY_TFT_BLUE;
    prompt.third = true;
    prompt.Draw();

    int button_width = prompt.w / 2 - 50;

    left_button.x = prompt.x + 1;
    left_button.w = button_width;
    left_button.y = prompt.y + prompt.h - 10 - prompt.h / 3;
    left_button.h = prompt.h / 3 - 10;
    left_button.text = "Record";
    left_button.c = MY_TFT_NAVY;
    left_button.Draw();

    center_button.x = prompt.x + + prompt.w / 2 - button_width / 2;
    center_button.w = button_width;
    center_button.y = prompt.y + prompt.h - 10 - prompt.h / 3;
    center_button.h = prompt.h / 3 - 10;
    center_button.text = "Play";
    center_button.c = MY_TFT_NAVY;
    center_button.Draw();

    right_button.x = prompt.x + prompt.w - 1 - button_width;
    right_button.w = button_width;
    right_button.y = prompt.y + prompt.h - 10 - prompt.h / 3;
    right_button.h = prompt.h / 3 - 10;
    right_button.text = "Idle";
    right_button.c = MY_TFT_NAVY;
    right_button.Draw();
  }

  ui_drawn = true;
}

void displayAlarms()
{
  int x = 5;
  int y = 0;
  int h = screen.height();
  int w = 60;

  lcd.fillRect(x,y,w,h,MY_TFT_DARKGRAY);
  lcd.drawRect(x+1,y+1,w-2,h-2,MY_TFT_WHITE);
  lcd.drawRect(x+2,y+2,w-4,h-4,MY_TFT_LIGHTGRAY);

  int button_height = 32;
  high_button.x = x;
  high_button.w = w;
  high_button.y = screen.height() - high_alarm - button_height / 2;
  high_button.h = button_height;
  high_button.text = (int) high_alarm;
  high_button.c = (ui_mode == UI_MOVE_HIGH_ALARM) ? MY_TFT_YELLOW : MY_TFT_MAROON;
  high_button.Draw();

  low_button.x = x;
  low_button.w = w;
  low_button.y = screen.height() - low_alarm - button_height / 2;
  low_button.h = button_height;
  low_button.text = (int) low_alarm;
  low_button.c = (ui_mode == UI_MOVE_LOW_ALARM) ? MY_TFT_YELLOW : MY_TFT_NAVY;
  low_button.Draw();
}

long last_timestamp = 0;
long ui_mode_time;
int oldReading = 0;

long last_readstamp = 0;
uint16_t readings[5];
int reading_index = 0;
int reading =0;

#define MAX_LINE_LEN 20

String processString()
{
  int index = 0;
  char stringArray[MAX_LINE_LEN];

  int next;
  while ((next = recordVar.recordFile.read()) != -1)
  {
      char nextChar = (char) next;
      if (nextChar == '\n')
      {
          stringArray[index] = '\0';
          String str(stringArray);
          return str;
      }
      else
      {
          stringArray[index] = nextChar;
          index++;
      }
  }
  return String("");
}

long duration;
int distance;
int old_distance;

int ReadDistance()
{
  // Clear the trigPin by setting it LOW:
  digitalWrite(TRIGGER_PIN, LOW);
  
  delayMicroseconds(5);

 // Trigger the sensor by setting the trigPin high for 20 microseconds:
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(20);
  digitalWrite(TRIGGER_PIN, LOW);
  
  // Read the echoPin. pulseIn() returns the duration (length of the pulse) in microseconds:
  duration = pulseIn(ECHO_PIN, HIGH);
  
  // Calculate the distance:
  distance = duration*0.034/2;
  
  // Print the distance on the Serial Monitor
  if (distance != old_distance)
  {
    Serial.print("Distance = ");
    Serial.print(distance);
    Serial.println(" cm");
    old_distance = distance;
  }
  
  delay(100);

  return distance;
}

float getReading()
{
  if (recordVar.idle || recordVar.recording)
  {
    readings[reading_index] =  ReadDistance();
    reading_index++;
    if (reading_index == 5) reading_index = 0; 
    float reading = readings[0] + readings[1] + readings[2] + readings[3] + readings[4];
    float factored = (reading / 5) * factor;
    if (recordVar.recording)
    {
      if (recordVar.recordFile)
      {
        recordVar.recordFile.println(factored);
      }
      else{
        Serial.println("recording but file is null");
      }
    }
    return factored;
  }
  else 
  {
    float factored = processString().toFloat();
    if (!recordVar.recordFile.available())
    {
      recordVar.recordFile.close();
      recordVar.idle = true;
      Serial.print(factored);
      Serial.println(" was the last factored... play back complete");
    }    
    return factored;
  }
}

float last_factored=0;
void loop() {

  if (millis() - last_readstamp > 100) 
  { 
    float factored = getReading();
    addVal(factored);
    if (factored > maxVal)
    {
      maxVal = factored;
    }
    if (factored < minVal)
    {
      minVal = factored;
    }
    if (last_factored != factored)
    {
      if (factored > 5 && highAlarm == false)
      {
        audio_help_setFrequency(5000 * (factored / 250)); 
      }
      else
      {
        audio_help_setFrequency(0);
      }     
      last_factored = factored;
    }
    last_readstamp = millis();  

  }

  if (millis() - last_timestamp > 400) 
  {   
    
    //int reading = (readings[0] + readings[1] + readings[2] + readings[3] + readings[4]) / 5;
    
    //scale the reading to the chart's vertical height
    //addVal(reading * factor);

    // if (reading != oldReading)
    // {
    //   Serial.print(highAlarm);
    //   Serial.print(",");
    //   Serial.print(reading);
    //   Serial.print(",");
    //   Serial.print(reading * factor);
    //   Serial.print(",");
    //   Serial.println(charging);
    //   oldReading = reading;
    // }

    clearDisplay();
    displayGraph();
    displayAlarms();

    last_timestamp = millis();
  }

   manageUI();

  int32_t x, y;
  
  if (ui_mode == UI_MOVE_HIGH_ALARM || ui_mode == UI_MOVE_LOW_ALARM)
  {
    if (millis() - 1000 > ui_mode_time)
    {
      ui_mode = UI_IDLE;
      displayAlarms();
    }
  }

  if ( screen.getTouch(&x, &y)) {
    // Serial.print(x);
    // Serial.print(",");
    // Serial.print(y);
    // Serial.print(",");
    // Serial.println(ui_mode);
    if (ui_mode == UI_IDLE)
    {
      if (timer.Hits(x,y)) 
      {
        ui_mode = UI_CONFIRM_RECORDER;
        Serial.println("UI_CONFIRM_RECORDER");
      }
      if (high_button.Hits(x,y))
      {
        ui_mode = UI_MOVE_HIGH_ALARM;
        ui_mode_time = millis();
      }
      if (low_button.Hits(x,y))
      {
        ui_mode = UI_MOVE_LOW_ALARM;
        ui_mode_time = millis();
      }
    }
    else if (ui_mode == UI_CONFIRM_RECORDER)
    {
      if (left_button.Hits(x,y)) {
        ui_mode = UI_IDLE;
        Serial.println("UI_IDLE Record");
        ui_question = "";
        clearDisplay();
      
        if (recordVar.idle && !recordVar.recording)
        {
          recordVar.recordFile = SD.open("/test.txt", FILE_WRITE, false);
          recordVar.recording = true;
          recordVar.idle = false;
          Serial.println("recording");
        }
        displayGraph();
        timer_initial = millis();
      }
      else if (center_button.Hits(x,y)) {
        ui_mode = UI_IDLE;
        Serial.println("UI_IDLE Play");
        ui_question = "";
        clearDisplay();

        if (recordVar.idle && !recordVar.recording)
        {
          recordVar.recordFile = SD.open("/test.txt");
          recordVar.recording = false;
          recordVar.idle = false;
          Serial.println("playing");
        }
        displayGraph();
      }
      else if (right_button.Hits(x,y)) {
        ui_mode = UI_IDLE;
        Serial.println("UI_IDLE Idle");
        ui_question = "";
        clearDisplay();

        if (!recordVar.idle && recordVar.recording)
        {
          recordVar.recordFile.close();
          recordVar.recording = false;
          recordVar.idle = true;
          Serial.println("idle");
        }

        maxVal = 0;
        minVal = 1000;
        displayGraph();
      }
    }
    else if (ui_mode == UI_MOVE_HIGH_ALARM)
    {
      high_alarm = screen.height() - y;
      displayAlarms();
      ui_mode_time = millis();
      delay(50);
      last_factored = 0;//get it to evaluate the alarm without reading change
    }
    else if (ui_mode == UI_MOVE_LOW_ALARM)
    {
      low_alarm = screen.height() - y;
      
      if (low_alarm == 0)
      { 
        low_alarm = 1;
      }
      audio_help_set_waveform("sine", low_alarm / 240);
      for (byte widx = 0; widx < audio_help_wave_buf_size; widx++)
      {
          Serial.print("wave buffer ");
          Serial.print(widx);
          Serial.print("=");
          Serial.println(audio_help_wave_buf[widx]);
      }
      
      displayAlarms();
      ui_mode_time = millis();
      delay(50);
    }    
  }

  lcd.pushSprite(0, 0);
}
