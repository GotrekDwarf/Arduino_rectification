void(* resetFunc) (void) = 0; //Перезагрузка

//Для мониторинга через порт
#if defined(USB_PID) && USB_PID == 0x804E 
#define Serial SerialUSB
#endif

//Для отображения на экране
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include "FontsRus/FreeSans8.h"
MCUFRIEND_kbv tft;


#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define GRAY    0x2408

int XP=8,XM=A2,YP=A3,YM=9; //240x320 ID=0x9595
const int TS_LEFT=104,TS_RT=896,TS_TOP=127,TS_BOT=901;


#define TFT_BEGIN()  tft.begin(ID)

uint16_t readID(void) {
    uint16_t ID = tft.readID();
    if (ID == 0xD3D3) ID = 0x9486;
    return ID;
}

//Для тача
#include "TouchScreen_kbv.h"         
#define TouchScreen TouchScreen_kbv
#define TSPoint     TSPoint_kbv
#define MINPRESSURE 200
#define MAXPRESSURE 1000

TouchScreen ts(XP, YP, XM, YM, 300);   //re-initialised after diagnose
TSPoint tp;                            //global point
int pixel_x, pixel_y;     //Touch_getXY() updates global vars

bool Touch_getXY(void)
{
    TSPoint p = ts.getPoint();
    pinMode(YP, OUTPUT);      //restore shared pins
    pinMode(XM, OUTPUT);      //because TFT control pins
    bool pressed = (p.z > MINPRESSURE && p.z < MAXPRESSURE);
    if (pressed) {
        pixel_x = map(p.x, TS_LEFT, TS_RT, 0, tft.height()); //.kbv makes sense to me
        pixel_y = map(p.y, TS_TOP, TS_BOT, 0, tft.width());
    }
    return pressed;
}
//Термодатчики
#include <microDS18B20.h>
MicroDS18B20<12> sensor;
//Пищалка
int piezoPin = 11;

///////////////////Основная программа
#define BACKGROUND BLACK    //Цвет Фона
#define TEXT WHITE          //Цвет текста

float TempDelta = 0.2;            // Значение гестерезиса по температуре(в градусах цельсия) при котором колонна считается стабильной
int HeadSamplingTime = 10800;     // Время отбора голов в секундах(3 часа)
float Temp = 0;                   // Переменная для хранения температуры
float FixTemp = -1000;            // Переменная для температурной полки

int Stage = 0;                    // Стадии ректификации



/////////////////////////Тут пытаемся оптимизировать

void Clear_Sreen(int font_size)
{
  tft.fillScreen(BACKGROUND);
  tft.setTextSize(font_size);
  tft.setTextColor(TEXT);
}

void Replace_text(int y, int x, int block_width, int block_height, int text_size, int cursor_y, int cursor_x)
{
  tft.fillRect(y, x, block_width, block_height, BACKGROUND);
  tft.setTextSize(text_size);
  tft.setTextColor(TEXT);
  tft.setCursor(cursor_y, cursor_x);
}

void Sampling(int Time, int Next_Stage)
{
  Adafruit_GFX_Button skip_btn;
  char skip_title[] = "Пропустить";
  skip_btn.initButton(&tft, 160, 220, 320, 40, WHITE, CYAN, BLACK, skip_title, 0);
  skip_btn.drawButton(false);

  unsigned long time_buf = millis();
  int i = 0;
  while(i < Time)
  {
    bool down = Touch_getXY();
    if(time_buf+1000 < millis())
    {
      Replace_text(165, 90, 200, 30, 2, 165, 116);
      int hours = (Time-i)/3600;
      int minutes = ((Time-i)-(hours*3600))/60;
      int sec = (Time-i)-((minutes*60)+(hours*3600));  
      tft.print(String(hours));
      tft.print(":");
      tft.print(String(minutes));
      tft.print(":");
      tft.print(String(sec));
      Replace_text(165, 127, 200, 30, 2, 165, 155);
      tft.println(Temp_Meas());
      time_buf = millis();
      i++;
    }
    skip_btn.press(down && skip_btn.contains(pixel_y, pixel_x));
    if (skip_btn.justReleased())
      skip_btn.drawButton();      
    if (skip_btn.justPressed()) {
      skip_btn.drawButton(true);
      Stage = Next_Stage;
      return;
    }
  }
  Stage = Next_Stage;
}


float Temp_Meas()       
{                     
  sensor.requestTemp();
  if (sensor.readTemp()) 
    return sensor.getTemp();
  else 
    return -10000;   
}                     


void setup()
{
    uint16_t ID = readID();         //Получить ID экрана
    TFT_BEGIN();                    //Инициализировать экран
    tft.setFont(&FreeSans8pt8b);    //Выбрать шрифт
    tft.setRotation(1);             //развернуть экран
    pinMode(A5, OUTPUT);            //Включаем пин под пищалку
    Serial.begin(9600);             //Включаем последовательный порт
}

void loop() {
  if(Stage == 0) Setup_process();
  if(Stage == 1) Heat_process();
  if(Stage == 2) Stabilization_process();
  if(Stage == 3) Stabilization_check();
  if(Stage == 4) Head_Sampling();
  if(Stage == 5) Heart_Sampling();
  if(Stage == 10) Tail_Sampling();
    
    
}

void Setup_process()
{
  Adafruit_GFX_Button ok_btn, cancel_btn, delta_plus_btn, delta_minus_btn, head_plus_btn, head_minus_btn;
  char ok[] = "START";
  char plus[] = "+";
  char minus[] = "-";
  ok_btn.initButton(&tft, 140, 220, 280, 40, WHITE, CYAN, BLACK, ok, 0);
  delta_plus_btn.initButton(&tft, 290, 40, 40, 40, WHITE, CYAN, BLACK, plus, 0);
  delta_minus_btn.initButton(&tft, 290, 85, 40, 40, WHITE, CYAN, BLACK, minus, 0);
  head_plus_btn.initButton(&tft, 290, 130, 40, 40, WHITE, CYAN, BLACK, plus, 0);
  head_minus_btn.initButton(&tft, 290, 175, 40, 40, WHITE, CYAN, BLACK, minus, 0);
  int hours = HeadSamplingTime/3600;
  int minutes = (HeadSamplingTime-(hours*3600))/60;
  
  Clear_Sreen(0);
  tft.setCursor(0, 40);
  tft.println("Допустимое отклонение");
  tft.println("температуры");
  tft.print("при отборе тела: ");
  tft.print(String(TempDelta));
  tft.print(" C");
  tft.println("");
  tft.println("");
  tft.println("");
  tft.println("Время");
  tft.print("отбора голов: ");
  tft.print(String(hours));
  tft.print(":");
  tft.print(String(minutes));
  tft.println(":00");
  delta_plus_btn.drawButton(false);
  delta_minus_btn.drawButton(false);
  head_plus_btn.drawButton(false);
  head_minus_btn.drawButton(false);
  ok_btn.drawButton(false);
  
  while(true)
  {
    bool down = Touch_getXY();
    ok_btn.press(down && ok_btn.contains(pixel_y, pixel_x));
    delta_plus_btn.press(down && delta_plus_btn.contains(pixel_y, pixel_x));
    delta_minus_btn.press(down && delta_minus_btn.contains(pixel_y, pixel_x));
    head_plus_btn.press(down && head_plus_btn.contains(pixel_y, pixel_x));
    head_minus_btn.press(down && head_minus_btn.contains(pixel_y, pixel_x));
    if (ok_btn.justReleased())
      ok_btn.drawButton();
    if (delta_plus_btn.justReleased())
      delta_plus_btn.drawButton();
    if (delta_minus_btn.justReleased())
      delta_minus_btn.drawButton();
    if (head_plus_btn.justReleased())
      head_plus_btn.drawButton();
    if (head_minus_btn.justReleased())
      head_minus_btn.drawButton();
      
    if (ok_btn.justPressed()) {
      ok_btn.drawButton(true);
      Stage = 1;
      return;
    }
    
    if (delta_plus_btn.justPressed()) {
      delta_plus_btn.drawButton(true);
      TempDelta+=0.1;
      Replace_text(0, 64, 260, 20, 0, 0, 78);
      tft.print("при отборе тела: ");
      tft.print(String(TempDelta));
      tft.println(" C");
    }
    if (delta_minus_btn.justPressed()) {
      delta_minus_btn.drawButton(true);
      TempDelta-=0.1;
      if(TempDelta<0)
      {
        TempDelta = 0;
      }
      Replace_text(0, 64, 260, 20, 0, 0, 78);
      tft.print("при отборе тела: ");
      tft.print(String(TempDelta));
      tft.println(" C");
    }
    if (head_plus_btn.justPressed()) {
      head_plus_btn.drawButton(true);
      HeadSamplingTime+=600;
      int hours = HeadSamplingTime/3600;
      int minutes = (HeadSamplingTime-(hours*3600))/60;
      Replace_text(0, 140, 260, 20, 0, 0, 154);
      tft.print("отбора голов: ");
      tft.print(String(hours));
      tft.print(":");
      tft.print(String(minutes));
      tft.print(":00");
    }
    if (head_minus_btn.justPressed()) {
      head_minus_btn.drawButton(true);
      HeadSamplingTime-=600;
      if(HeadSamplingTime<0)
      {
        HeadSamplingTime = 0;
      }
      int hours = HeadSamplingTime/3600;
      int minutes = (HeadSamplingTime-(hours*3600))/60;
      Replace_text(0, 140, 260, 20, 0, 0, 154);
      tft.print("отбора голов: ");
      tft.print(String(hours));
      tft.print(":");
      tft.print(String(minutes));
      tft.print(":00");
    }
  }
}

void Heat_process()
{
  unsigned long time_buf = millis();
  Adafruit_GFX_Button stop_btn;
  char stop_title[] = "STOP";
  stop_btn.initButton(&tft, 160, 220, 320, 40, WHITE, CYAN, BLACK, stop_title, 0);
  Clear_Sreen(2);
  tft.setCursor(0, 40);
  tft.println("Греем куб!");
  tft.println("В колонне: ");
  stop_btn.drawButton(false);
  
  while(true)
  {
    bool down = Touch_getXY();
    if(time_buf+1000 < millis())
    {
      Temp = Temp_Meas();
      Replace_text(0, 102, 200, 30, 2, 0, 130);
      tft.print(String(Temp));
      tft.print(" C");
      if(Temp > 60)
      {
        Stage = 2;
        return;
      }
      time_buf = millis();
    }

    stop_btn.press(down && stop_btn.contains(pixel_y, pixel_x));
    if (stop_btn.justReleased())
      stop_btn.drawButton();      
    if (stop_btn.justPressed()) {
      stop_btn.drawButton(true);
      Stage = 0;
      return;
    }
  }
}

void Stabilization_process()
{ 
  Clear_Sreen(2);
  tft.setCursor(0, 40);
  tft.println("Стабилизация");
  tft.println("колонны!");
  tft.println("Осталось: ");
  tft.println("В колонне: ");
  Sampling(1500, 3);
}

void Stabilization_check()
{
  Temp = Temp_Meas();
  float TempArray[90];
  Adafruit_GFX_Button start_btn;
  char start_title[] = "Начать";
  start_btn.initButton(&tft, 160, 220, 320, 40, WHITE, CYAN, BLACK, start_title, 0);
  Clear_Sreen(2);
  tft.setCursor(0, 40);
  tft.println("Стабилизация");
  tft.println("колонны!");
  tft.println("Проверка.");
  tft.println("В колонне: ");
  
  for (int i=0; i< 90; i++) { // собираем показания температуры за 1,5 минуты
    TempArray[i] = Temp_Meas();
    Replace_text(165, 127, 200, 30, 2, 165, 155);
    tft.print(TempArray[i]);
    delay(1000);
  }
  for (int i=0; i< 90; i++) {
    if((TempArray[i]+TempDelta < Temp) or (TempArray[i]-TempDelta > Temp))
    {
      return;
    }
  }
  
  Clear_Sreen(1);
  tft.setCursor(0, 40);
  tft.println("Колонна стабилизирована,"); 
  tft.println("можно начинать отбор голов!");
  tone(piezoPin, 1000, 5000);
  start_btn.drawButton(false);  
  while(true)
  {
    bool down = Touch_getXY();
    start_btn.press(down && start_btn.contains(pixel_y, pixel_x));
    if (start_btn.justReleased())
      start_btn.drawButton();      
    if (start_btn.justPressed()) {
      start_btn.drawButton(true);
      Stage = 4;
      return;
    }
  }
}

void Head_Sampling()
{
  Clear_Sreen(2);
  tft.setCursor(0, 40);
  tft.println("Отбор голов!");
  tft.println("");
  tft.println("Осталось: ");
  tft.println("В колонне: ");
  Sampling(HeadSamplingTime, 5);

}

void Heart_Sampling()
{
  int FaultTailKeeper = 0;
  
  Clear_Sreen(2);
  tft.setCursor(0, 40);
  tft.println("Отбор тела!");
  tft.println("");
  tft.println("");
  tft.println("В колонне: ");
  tone(piezoPin, 1000, 5000);
  FixTemp = Temp_Meas();
  while(true) { 
    Temp = Temp_Meas();
    Replace_text(165, 127, 200, 30, 2, 165, 155);
    tft.print(Temp);
    if(((FixTemp+TempDelta<Temp) or (FixTemp-TempDelta>Temp)) and FixTemp>-300)
    {
      FaultTailKeeper++;
      if(FaultTailKeeper > 3)
      {
        Stage = 10;
        return;
      }
    }
    else
    {
      FaultTailKeeper = 0;
    }
    delay(1000);
  }
}

void Tail_Sampling()
{
  unsigned long time_buf = millis();
  Adafruit_GFX_Button Tail_btn, Heart_btn;
  char Tail_title[] = "Хвосты";
  Tail_btn.initButton(&tft, 75, 220, 140, 40, WHITE, CYAN, BLACK, Tail_title, 0);
  char Heart_title[] = "Тело";
  Heart_btn.initButton(&tft, 225, 220, 140, 40, WHITE, CYAN, BLACK, Heart_title, 0);

  Clear_Sreen(2);
  tft.setCursor(0, 40);
  tft.println("Отбор хвостов!");
  tft.println("");
  tft.println("");
  tft.println("В колонне: ");
  
  Tail_btn.drawButton(false);
  Heart_btn.drawButton(false);

  tone(piezoPin, 1000);
  
  while(true) { 
    bool down = Touch_getXY();   

    if(time_buf+1000 < millis())
    {
      Replace_text(165, 127, 200, 30, 2, 165, 155);
      tft.print(Temp_Meas());
      time_buf = millis();
    }
    
    Tail_btn.press(down && Tail_btn.contains(pixel_y, pixel_x));
    if (Tail_btn.justReleased())
      Tail_btn.drawButton();      
    if (Tail_btn.justPressed()) {
      Tail_btn.drawButton(true);
      noTone(piezoPin);
    }
    Heart_btn.press(down && Heart_btn.contains(pixel_y, pixel_x));
    if (Heart_btn.justReleased())
      Heart_btn.drawButton();      
    if (Heart_btn.justPressed()) {
      Heart_btn.drawButton(true);
      Stage = 5;
      noTone(piezoPin);
      return;
    }
  }
}
