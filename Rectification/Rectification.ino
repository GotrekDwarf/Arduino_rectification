#include <Wire.h>
#include <LiquidCrystal_I2C.h>



///////////////////////////////////////////////////////
byte bukva_B[8]   = {B11110,B10000,B10000,B11110,B10001,B10001,B11110,B00000,}; // Буква "Б"
byte bukva_G[8]   = {B11111,B10001,B10000,B10000,B10000,B10000,B10000,B00000,}; // Буква "Г"
byte bukva_D[8]   = {B01111,B00101,B00101,B01001,B10001,B11111,B10001,B00000,}; // Буква "Д"
byte bukva_ZH[8]  = {B10101,B10101,B10101,B11111,B10101,B10101,B10101,B00000,}; // Буква "Ж"
byte bukva_Z[8]   = {B01110,B10001,B00001,B00010,B00001,B10001,B01110,B00000,}; // Буква "З"
byte bukva_I[8]   = {B10001,B10011,B10011,B10101,B11001,B11001,B10001,B00000,}; // Буква "И"
byte bukva_IY[8]  = {B01110,B00000,B10001,B10011,B10101,B11001,B10001,B00000,}; // Буква "Й"
byte bukva_L[8]   = {B00011,B00111,B00101,B00101,B01101,B01001,B11001,B00000,}; // Буква "Л"
byte bukva_P[8]   = {B11111,B10001,B10001,B10001,B10001,B10001,B10001,B00000,}; // Буква "П"
byte bukva_Y[8]   = {B10001,B10001,B10001,B01010,B00100,B01000,B10000,B00000,}; // Буква "У"
byte bukva_F[8]   = {B00100,B11111,B10101,B10101,B11111,B00100,B00100,B00000,}; // Буква "Ф"
byte bukva_TS[8]  = {B10010,B10010,B10010,B10010,B10010,B10010,B11111,B00001,}; // Буква "Ц"
byte bukva_CH[8]  = {B10001,B10001,B10001,B01111,B00001,B00001,B00001,B00000,}; // Буква "Ч"
byte bukva_Sh[8]  = {B10101,B10101,B10101,B10101,B10101,B10101,B11111,B00000,}; // Буква "Ш"
byte bukva_Shch[8]= {B10101,B10101,B10101,B10101,B10101,B10101,B11111,B00001,}; // Буква "Щ"
byte bukva_Mz[8]  = {B10000,B10000,B10000,B11110,B10001,B10001,B11110,B00000,}; // Буква "Ь"
byte bukva_IYI[8] = {B10001,B10001,B10001,B11001,B10101,B10101,B11001,B00000,}; // Буква "Ы"
byte bukva_Yu[8]  = {B10010,B10101,B10101,B11101,B10101,B10101,B10010,B00000,}; // Буква "Ю"
byte bukva_Ya[8]  = {B01111,B10001,B10001,B01111,B00101,B01001,B10001,B00000,}; // Буква "Я"
//////////////////////////////////////////////////////////////////////////////

LiquidCrystal_I2C lcd(0x27,16,2); //Устанавливаем дисплей
#define THERMISTORPIN A0          // к какому аналоговому пину подключен термистор
#define THERMISTORNOMINAL 100000  // сопротивление при 25 градусах по Цельсию
#define TEMPERATURENOMINAL 25     // temp. для номинального сопротивления (практически всегда равна 25 C)
#define NUMSAMPLES 5              // сколько показаний используем для определения среднего значения
#define BCOEFFICIENT 3950         // бета коэффициент термистора (обычно 3000-4000)
#define SERIESRESISTOR 10000      // сопротивление второго резистора

const int OkButtonPin = 3;        // номер входа, подключенный к кнопке Ok
const int UpButtonPin = 4;        // номер входа, подключенный к кнопке Up
const int DownButtonPin = 5;      // номер входа, подключенный к кнопке Down
const int CancelButtonPin = 6;    // номер входа, подключенный к кнопке Cancel
const byte piezoPin = 2;          // к какому выводу подключена пищалка

float Temp = 0;                   // Переменная для хранения температуры
float FixTemp = -1000;            // Переменная для температурной полки
int HoldOkButton = 0;             // Переменная которая считает количество секунд пока нажата кнопка Ok
int HoldUpButton = 0;             // Переменная которая считает количество секунд пока нажата кнопка Up
int HoldDownButton = 0;           // Переменная которая считает количество секунд пока нажата кнопка Down
int HoldCancelButton = 0;         // Переменная которая считает количество секунд пока нажата кнопка Cancel
float TempDelta = 0.2;            // Значение гестерезиса по температуре(в градусах цельсия) при котором колонна считается стабильной
int SelfWorkTime = 1500;           // Время работы колонны "на себя"
int HeadSamplingTime = 10800;     // Время отбора голов в секундах(3 часа)

int SetupStage = 0;               // Стадии настройки
// 0 - Установка допустимого отклонения температуры при отборе тела
// 1 - Установка длительности работы "на себя"
// 2 - Установка длительности отбора голов
int Stage = 0;                    // Стадии ректификации
// 0 - настройки
// 1 - нагрев куба
// 2 - работа колонны "На себя"
// 3 - ждем стабилизации колонны
// 4 - ожидание начала отбора голов(НАДО НАПИСАТЬ КАК ОТБИРАЮТСЯ ГОЛОВЫ!!!)
// 5 - отбор голов
// 6 - отбор тела

// 10 - окончание ректификации


int samples[NUMSAMPLES];

void setup(void) {
  Serial.begin(9600);
  pinMode(OkButtonPin, INPUT_PULLUP);     // инициализируем пин, подключенный к кнопке, как вход
  pinMode(UpButtonPin, INPUT_PULLUP);     // инициализируем пин, подключенный к кнопке, как вход
  pinMode(DownButtonPin, INPUT_PULLUP);     // инициализируем пин, подключенный к кнопке, как вход
  pinMode(CancelButtonPin, INPUT_PULLUP);     // инициализируем пин, подключенный к кнопке, как вход

  pinMode(piezoPin, OUTPUT);        // настраиваем вывод piezoPin на выход
  
  lcd.init();
  lcd.backlight();
  lcd.print("Temp:init");
  //analogReference(EXTERNAL);      //баллансировка через AREF
}

void loop(void) {
  float TempArray[90];
  
  if(Stage == 0)
  {
    lcd.createChar(1, bukva_I);      // Создаем символ под номером 1
    lcd.createChar(2, bukva_D);      // Создаем символ под номером 2
    lcd.createChar(3, bukva_L);      // Создаем символ под номером 3
    lcd.createChar(4, bukva_P);      // Создаем символ под номером 4
    lcd.createChar(5, bukva_IYI);      // Создаем символ под номером 5
    lcd.createChar(6, bukva_B);      // Создаем символ под номером 6
    lcd.createChar(7, bukva_G);      // Создаем символ под номером 7
    lcd.createChar(8, bukva_CH);      // Создаем символ под номером 8


    if(SetupStage == 0)   // 0 - Установка допустимого отклонения температуры при отборе тела
    {
      lcd.setCursor(0, 0);
      lcd.print("\2O\4YCT\1MOE OTK\3.");
      lcd.setCursor(0, 1);
      lcd.print("TEM\4EPATYP\5:");
      lcd.setCursor(12, 1);
      lcd.print(TempDelta);
      if (!digitalRead(UpButtonPin)) {    // Если хотим дистилляцию - увеличиваем  
        TempDelta+=0.1;
      }
      if (!digitalRead(DownButtonPin)) { // Если хотим ректификацию - уменьшаем, но всегда нужно опасаться погрешности датчика
        TempDelta-=0.1;
        if(TempDelta<0)
        {
          TempDelta = 0;
        }
      }
      if (!digitalRead(OkButtonPin)) { 
        SetupStage=1;
        delay(1000);
      }
    }
    if(SetupStage == 1)   // 1 - Установка длительности работы "на себя"   
    {
      lcd.setCursor(0, 0);
      lcd.print("                ");
      lcd.setCursor(0, 0);
      lcd.print("CTA\6\1\3\13:");
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      int hours = SelfWorkTime/3600;
      int minutes = (SelfWorkTime-(hours*3600))/60;
      lcd.print(String(hours)+":"+String(minutes)+":00");
      if (!digitalRead(UpButtonPin)) { 
        SelfWorkTime+=60;
      }
      if (!digitalRead(DownButtonPin)) { 
        SelfWorkTime-=60;
        if(SelfWorkTime<0)
        {
          SelfWorkTime = 0;
        }
      }
      if (!digitalRead(OkButtonPin)) { 
      SetupStage = 2;
      }
      if (!digitalRead(CancelButtonPin)) { // Вернуться в начало
      SetupStage = 0;
      }
    }
    if(SetupStage == 2)   // 2 - Установка длительности отбора голов
    {
      lcd.setCursor(0, 0);
      lcd.print("                ");
      lcd.setCursor(0, 0);
      lcd.print("OT\6OP \7O\3OB");
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      int hours = HeadSamplingTime/3600;
      int minutes = (HeadSamplingTime-(hours*3600))/60;
      lcd.print(String(hours)+":"+String(minutes)+":00");
      if (!digitalRead(UpButtonPin)) { 
        HeadSamplingTime+=600;
      }
      if (!digitalRead(DownButtonPin)) { 
        HeadSamplingTime-=600;
        if(HeadSamplingTime<0)
        {
          HeadSamplingTime = 0;
        }
      }
      if (!digitalRead(OkButtonPin)) { 
      Stage = 1;
      }
      if (!digitalRead(CancelButtonPin)) { // Вернуться в начало
      SetupStage = 0;
      }
    }
  }
  if(Stage == 1)    // Ждем когда температура в колонне станет выше 60 градусов и переходим на стабилизацию фракций в колонне
  {
    Temp = Temp_Meas();
    lcd.createChar(1, bukva_G);      // Создаем символ под номером 1
    lcd.createChar(2, bukva_B);      // Создаем символ под номером 2
    lcd.createChar(3, bukva_L);      // Создаем символ под номером 3
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("\1PEEM KY\2!");
    lcd.setCursor(0, 1);
    lcd.print("B KO\3OHHE: "+String(Temp));
    if(Temp > 60)
    {
      Stage = 2;
    }
    if (!digitalRead(CancelButtonPin)) { // Не ждать прогрева колонны, сразу перейти к работе на себя. В основном для дебага.
      Stage = 2;
    }
  }
  if(Stage == 2)    // даем колонне поработать на себя указанное время
  {
    int i;
    lcd.createChar(1, bukva_Ya);      // Создаем символ под номером 1
    lcd.createChar(2, bukva_B);      // Создаем символ под номером 2
    lcd.createChar(3, bukva_L);      // Создаем символ под номером 3
    lcd.createChar(4, bukva_I);      // Создаем символ под номером 4
    lcd.createChar(5, bukva_Z);      // Создаем символ под номером 5
    lcd.createChar(6, bukva_TS);      // Создаем символ под номером 6
    lcd.createChar(7, bukva_P);      // Создаем символ под номером 7
    lcd.createChar(8, bukva_Mz);      // Создаем символ под номером 8

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CTA\2\4\3\4\5:"); 
    lcd.setCursor(0, 1);
    lcd.print("B KO\3OHHE:");
    for (i=0; i< SelfWorkTime; i++) { 
      lcd.setCursor(9, 0);
      lcd.print("       ");
      lcd.setCursor(9, 0);
      int minutes = (SelfWorkTime-i)/60;
      int sec = (SelfWorkTime-i)-minutes*60; 
      lcd.print(String(minutes)+":"+String(sec));
      TempArray[i] = Temp_Meas();
      lcd.setCursor(10, 1);
      lcd.print(TempArray[i]);
      delay(1000);
    }
    Stage = 3;
  }
  if(Stage == 3)    //Ждем пока температура в колонне стабилизируется и просим пользователя настроить отбор голов
  {
    int i;
    Temp = Temp_Meas();

    lcd.createChar(1, bukva_Ya);      // Создаем символ под номером 1
    lcd.createChar(2, bukva_B);      // Создаем символ под номером 2
    lcd.createChar(3, bukva_L);      // Создаем символ под номером 3
    lcd.createChar(4, bukva_I);      // Создаем символ под номером 4
    lcd.createChar(5, bukva_Z);      // Создаем символ под номером 5
    lcd.createChar(6, bukva_TS);      // Создаем символ под номером 6
    lcd.createChar(7, bukva_P);      // Создаем символ под номером 7

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CTA\2\4\3\4\5A\6\4\1!"); 
    lcd.setCursor(0, 1);
    lcd.print("B KO\3OHHE:");
    for (i=0; i< 90; i++) { // собираем показания температуры за 1,5 минуты
      TempArray[i] = Temp_Meas();
      lcd.setCursor(10, 1);
      lcd.print(TempArray[i]);
      delay(1000);
    }
    for (i=0; i< 90; i++) {
      if((TempArray[i]+TempDelta < Temp) or (TempArray[i]-TempDelta > Temp))
      {
        return;
      }
    }
    Stage = 4;
  }
  if(Stage == 4)
  {
    Temp = Temp_Meas();
    lcd.createChar(1, bukva_G);      // Создаем символ под номером 1
    lcd.createChar(2, bukva_B);      // Создаем символ под номером 2
    lcd.createChar(3, bukva_L);      // Создаем символ под номером 3
    lcd.createChar(4, bukva_CH);      // Создаем символ под номером 4

    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("OT\2OP \1O\3OB");
    lcd.setCursor(4, 1);
    lcd.print("HA\4AT?");
    tone(piezoPin, 1000);                   // генерируем звук с частотой 100 Гц
    delay(1000);
    noTone(piezoPin);
    delay(1000);
    if (!digitalRead(OkButtonPin)) { 
      Stage = 5;
    }
    if (!digitalRead(CancelButtonPin)) {    // Возвращаемся на начало настройки 
      Stage = 0;
      SetupStage = 0;
    }
  }
  if(Stage == 5)
  {
    int i;
    int Cancel_Stage = 0;
    lcd.createChar(1, bukva_G);      // Создаем символ под номером 1
    lcd.createChar(2, bukva_B);      // Создаем символ под номером 2
    lcd.createChar(3, bukva_L);      // Создаем символ под номером 3
    lcd.createChar(4, bukva_CH);      // Создаем символ под номером 4
    lcd.createChar(5, bukva_Mz);      // Создаем символ под номером 5
    lcd.createChar(6, bukva_I);      // Создаем символ под номером 6


    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("OT\2OP \1O\3OB!");
    for (i=0; i< HeadSamplingTime; i++) { // отбираем головы
      if(i % 10 < 5)
      {
        lcd.setCursor(0, 1);
        lcd.print("OCTA\3OC\5:        ");
        lcd.setCursor(9, 1);
        int hours = (HeadSamplingTime-i)/3600;
        int minutes = ((HeadSamplingTime-i)-(hours*3600))/60;
        int sec = (HeadSamplingTime-i)-((minutes*60)+(hours*3600)); 
        lcd.print(String(hours)+":"+String(minutes)+":"+String(sec));
      }
      else
      {
              lcd.setCursor(0, 1);
              lcd.print("B KO\3OHHE: ");
              lcd.setCursor(0, 10);
              lcd.print(Temp); 
      }

      if (!digitalRead(CancelButtonPin)) { 
        lcd.setCursor(0, 0);
        lcd.print("OTMEH\6T\5 OT\2OP?");
        Cancel_Stage = 1;
      }

      if(Cancel_Stage == 1)
      {
        if (!digitalRead(OkButtonPin)) { 
          i=HeadSamplingTime;
        }
        if (!digitalRead(CancelButtonPin)) { 
          Cancel_Stage = 0;
          lcd.setCursor(0, 0);
          lcd.print("                ");
          lcd.setCursor(0, 0);
          lcd.print("OT\2OP \1O\3OB!");
        }
      }
      delay(1000);
    }
    Temp = Temp_Meas();
    FixTemp = Temp;
    Stage = 6;
    SetupStage = 0;
    tone(piezoPin, 1000);                   // генерируем звук с частотой 100 Гц
    delay(1000);
    noTone(piezoPin);
    delay(1000);
    tone(piezoPin, 1000);                   // генерируем звук с частотой 100 Гц
    delay(1000);
    noTone(piezoPin);
    delay(1000);
    tone(piezoPin, 1000);                   // генерируем звук с частотой 100 Гц
    delay(1000);
    noTone(piezoPin);
    delay(1000);
  }
  if(Stage == 6)
  {
    Temp = Temp_Meas();
    if(SetupStage==0)
    {
      lcd.createChar(1, bukva_P);      // Создаем символ под номером 1
      lcd.createChar(2, bukva_B);      // Создаем символ под номером 2
      lcd.createChar(3, bukva_L);      // Создаем символ под номером 3
      lcd.createChar(4, bukva_CH);      // Создаем символ под номером 4
    
      lcd.setCursor(0, 0);
      lcd.print("                ");
      lcd.setCursor(2, 0);
      lcd.print("OT\2OP TE\3A");
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print("B KO\3OHHE: ");
      SetupStage = 1;
    }
    if(SetupStage==1)
    {
      lcd.setCursor(10, 1);
      lcd.print("      ");
      lcd.setCursor(10, 1);    
      lcd.print(Temp);
      if(((FixTemp+1<Temp) or (FixTemp-1>Temp)) and FixTemp>-300)
      {
        Stage = 10;
      }
    }
    
  }
  

if(Stage == 10)
  {
    Temp = Temp_Meas();
    int NeedTone = 1;
    if(SetupStage==0)
    {
      lcd.createChar(1, bukva_P);      // Создаем символ под номером 1
      lcd.createChar(2, bukva_B);      // Создаем символ под номером 2
      lcd.createChar(3, bukva_L);      // Создаем символ под номером 3
      lcd.createChar(4, bukva_CH);      // Создаем символ под номером 4
      lcd.createChar(5, bukva_IYI);      // Создаем символ под номером 5

    
      lcd.setCursor(0, 0);
      lcd.print("                ");
      lcd.setCursor(2, 0);
      lcd.print("XBOCT\5?!");
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print("B KO\3OHHE: ");
      SetupStage = 1;
    }
    if(SetupStage==1)
    {
      lcd.setCursor(10, 1);
      lcd.print("      ");
      lcd.setCursor(10, 1);    
      lcd.print(Temp); 
      if(NeedTone == 1)
      {
        tone(piezoPin, 1000);                   // генерируем звук с частотой 100 Гц
        delay(1000);
        noTone(piezoPin);
        delay(1000);
      }
      if (!digitalRead(OkButtonPin)) { 
      NeedTone = 0;
      }
      if (!digitalRead(CancelButtonPin)) { 
      Stage = 6;
      }
    }
  }
delay(100);  
}

float Temp_Meas(void) {
  uint8_t i;
  float average;
  float steinhart;
 
  for (i=0; i< NUMSAMPLES; i++) { // сводим показания в вектор с небольшой задержкой между снятием показаний
    samples[i] = analogRead(THERMISTORPIN);
    delay(10);
  }
  average = 0;                    // рассчитываем среднее значение
  for (i=0; i< NUMSAMPLES; i++) {
    average += samples[i];
  }
  average /= NUMSAMPLES;
  average = 1023 / average - 1;   // конвертируем значение в сопротивление
  average = SERIESRESISTOR / average;
  steinhart = average / THERMISTORNOMINAL; // (R/Ro)
  steinhart = log(steinhart);     // ln(R/Ro)
  steinhart /= BCOEFFICIENT;      // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;    // инвертируем
  steinhart -= 273.15;            // конвертируем в градусы по Цельсию
  return(steinhart);
}
