// Дисплей
#include <U8glib.h>
#include <Adafruit_NeoPixel.h>
#define COUNT_LED 3 // количество светодиодов подсветки
#define LED_PIN 12 // номер порта к которому подключен модуль подсветки

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(COUNT_LED, LED_PIN, NEO_GRB + NEO_KHZ800); //Инициализируем подсветку
U8GLIB_NHD_C12864 u8g(13, 11, 10, 9, 8);  // SPI Com: SCK = 13, MOSI = 11, CS = 10, A0 = 9, RST = 8

// Термодатчики
#include <microDS18B20.h>

MicroDS18B20<6> sensor;

// Реле клапана отбора
#define RELAY_IN 5

// Энкодер
#define LEFT_ROTATE 1 
#define RIGHT_ROTATE 2 
#define CLICK_ENCODER 3 
#define LONG_CLICK_ENCODER 4
#define BTN_LONG_PUSH 3000   // Длительность долинного нажатия кнопки

volatile uint8_t lastcomb=7, enc_state, btn_push=0;
volatile int enc_rotation=0, btn_enc_rotate=0, enc_old_rotation=0;
volatile boolean btn_press=0;
volatile uint32_t timer;

// Пищалка
int beeper = 7;
bool beeperOn = false;

///////////////////Основная программа

float TempDelta = 0.2;            // Значение гестерезиса по температуре(в градусах цельсия) при котором колонна считается стабильной
float HeadSamplingTime = 10800;   // Время отбора голов в секундах(3 часа)
float ColumnStableTime = 600;     // Время на стабилизацию колонны в секундах(10 минут)
float Temp = 0;                   // Переменная для хранения температуры
float FixTemp = -1000;            // Переменная для температурной полки
int FaultTailKeeper = 0;          // Переменная для хранения числа отклонениий температуры при отборе тела, нужна для защиты от косяков термодатчика

int Stage = 0;                    // Стадии ректификации
int activeMenuPoint = 0;          // Используем если есть меню
int changeMode = 0;               // Если что-то меняем в меню
unsigned long time_buf = 0;       // Для хранения времени работы стадии



float Temp_Meas()       
{                     
  sensor.requestTemp();
  if (sensor.readTemp()) 
    return sensor.getTemp();
  else 
    return -10000;   
}                     

ISR (PCINT2_vect) //Обработчик прерывания от пинов A1, A2, A3
{
  uint8_t comb = bitRead(PINK, 2) << 2 | bitRead( PINK, 1)<<1 | bitRead(PINK, 0); //считываем состояние пинов энкодера и кнопки
 if (comb == 3 && lastcomb == 7) btn_press = 1; //Если было нажатие кнопки, то меняем статус
 
 if (comb == 4)                         //Если было промежуточное положение энкодера, то проверяем его предыдущее состояние 
 {
    if (lastcomb == 5) enc_state = RIGHT_ROTATE; //вращение по часовой стрелке
    if (lastcomb == 6) enc_state = LEFT_ROTATE; //вращение против часовой   
  }

  if (comb == 7 && lastcomb == 3 && btn_press) //Если было отпускание кнопки, то проверяем ее предыдущее состояние 
  {
    if (millis() - timer > BTN_LONG_PUSH)         // проверяем сколько прошло миллисекунд
    {
      enc_state = LONG_CLICK_ENCODER;                              // было длинное нажатие 
    } else {
              enc_state = CLICK_ENCODER;                    // было нажатие 
            }
      btn_press = 0;                           //обнулить статус кнопки
  }
   
  timer = millis();                       //сброс таймера
  lastcomb = comb;                        //сохраняем текущее состояние энкодера
}

void setup()
{
    pixels.begin();
    pixels.show(); // Устанавливаем все светодиоды в состояние "Выключено"
    pixels.setPixelColor(0, 250, 0, 0); // Назначаем для первого светодиода цвет "Зеленый"
    pixels.setBrightness(100); // Делаем поярче
    pixels.show(); // Включаем светодиоды

    u8g.setContrast(75); // Устанавливаем вменяемый контраст
    u8g.setFont(u8g_font_unifont); // Устанавливаем шрифт
    u8g.setRot180(); // Разворачиваем экран

    // Инициализируем энкодер
    pinMode(A8,INPUT_PULLUP); // ENC-A
    pinMode(A9,INPUT_PULLUP); // ENC-B
    pinMode(A10,INPUT_PULLUP); // BUTTON

    PCICR |= (1 << PCIE2);    // enable PCINT2
    PCMSK2 |= (1 << PCINT16); // mask for bit0 of port K (A8)
    PCMSK2 |= (1 << PCINT17); // mask for bit1 of port K (A9)
    PCMSK2 |= (1 << PCINT18); // mask for bit3 of port K (A10)

    pinMode(beeper, OUTPUT); // Инициализируем зумер

    pinMode(RELAY_IN, OUTPUT); // Инициализируем реле клапана отбора
    
    Serial.begin(9600); // Инициализируем последовательный порт
}

void loop() {
  if(Stage == 0) Setup_process();
  if(Stage == 1) Heat_process();
  if(Stage == 2) Stabilization_process();
  if(Stage == 3) Stabilization_check();
  if(Stage == 4) Head_Sampling();
  if(Stage == 5) Heart_Sampling();
  if(Stage == 6) Try_Stabling();
  if(Stage == 10) Stop_Sampling();
    
    
}

void Setup_process()
{
    digitalWrite(RELAY_IN, 0); // явно закрываем реле клапана отбора
    u8g.firstPage();
    do 
    {
        drawSetup();

    } while( u8g.nextPage() );
    switch (enc_state)
    {
        case RIGHT_ROTATE:   
        {
            if(changeMode == 0)
            {
                if(activeMenuPoint<4) activeMenuPoint++;
            }
            else
            {
                if(activeMenuPoint == 0 && TempDelta < 10)
                {
                    TempDelta = TempDelta + 0.1;
                }
                else if(activeMenuPoint == 1 && HeadSamplingTime < 36000)
                {
                    HeadSamplingTime = HeadSamplingTime + 60;
                }
                else if(activeMenuPoint == 2 && ColumnStableTime < 3600)
                {
                    ColumnStableTime = ColumnStableTime + 60;
                }
            }
        }
        break;
        case LEFT_ROTATE:   
        {
            if(changeMode == 0)
            {
                if(activeMenuPoint>0) activeMenuPoint--;
            }
            else
            {
                if(activeMenuPoint == 0 && TempDelta > 0.1)
                {
                    TempDelta = TempDelta - 0.1;
                }
                else if(activeMenuPoint == 1 && HeadSamplingTime > 0)
                {
                    HeadSamplingTime = HeadSamplingTime - 60;
                }
                else if(activeMenuPoint == 2 && ColumnStableTime > 0)
                {
                    ColumnStableTime = ColumnStableTime - 60;
                }
            }
        }
        break;
        case CLICK_ENCODER:   
        {
            if(changeMode == false && activeMenuPoint != 3 && activeMenuPoint != 4)
            {
                changeMode = true;
            }
            else
            {
                changeMode = false;
            }
            if(activeMenuPoint == 4)
            {
                Stage++;
            } 
            }
            break;
    }
  enc_state = 0; //обнуляем статус энкодера  
}

void drawSetup(void) {
    u8g.setFont(u8g_font_6x12);
    u8g.setFontRefHeightExtendedText();
    u8g.setFontPosTop();
    Temp = Temp_Meas();
    String Menu[] {
        "Temp. hysteresis: ",
        "Head time:   ",
        "Time of stability:",
        "Temperature:    ",
        "         Go!         "
    };
    float* MenuData[] {
        &TempDelta,
        &HeadSamplingTime,
        &ColumnStableTime,
        &Temp
    };
    int h = u8g.getFontAscent() - u8g.getFontDescent();
    for (int i = 0; i < sizeof(Menu)/sizeof(Menu[0]); i++)
    {
        String printLine = Menu[i];
        if(i == 1)
        {
            int hours = int(trunc(*MenuData[i]))/3600;
            int minutes = (int(trunc(*MenuData[i]))-(hours*3600))/60;
            String hourZero = "";
            String minuteZero = "";
            if(hours<10) hourZero = "0";
            if(minutes<10) minuteZero = "0";
            printLine = printLine + hourZero + String(hours) + ":" + minuteZero + String(minutes) + ":00";
        }
        else if(i == 2)
        {
            int minutes = (int(trunc(*MenuData[i])))/60;
            String minuteZero = "";
            if(minutes<10) minuteZero = "0";
            printLine = printLine + minuteZero + String(minutes) + "m";
        }
        else if(i != 4)
        {
            printLine = printLine + String(*MenuData[i]);
        }
        u8g.drawStr(2, h * i+1, printLine.c_str());
        
    }
    if(changeMode == 0)
    {
        int w = 125;
        u8g.drawFrame(0, h * activeMenuPoint, w+3, h+2);
    }
    else
    {
        int w = u8g.getStrWidth(Menu[activeMenuPoint].c_str());
        u8g.drawFrame(w, h * activeMenuPoint, w+3, h+2);
    }    
}


void Heat_process()
{
    u8g.firstPage();

    do 
    {
        u8g.setFont(u8g_font_6x12);
        u8g.setFontRefHeightExtendedText();
        u8g.setFontPosTop();
        Temp = Temp_Meas();
        u8g.drawStr(2, 0, "Alembic is heating!");
        u8g.drawStr(2, 10, "Alembic temperature: ");
        u8g.setFont(u8g_font_courB24);
        u8g.drawStr(2, 45, String(Temp).c_str());
    } while( u8g.nextPage() );
    if(Temp > 60)
    {
        time_buf = millis()/1000 + ColumnStableTime;
        Stage++;
    }
}


void Stabilization_process()
{ 
    
    unsigned long nowTime = millis()/1000;
    if( nowTime < time_buf )
    {
        u8g.firstPage();
        do 
        {
            u8g.setFont(u8g_font_6x12);
            u8g.setFontRefHeightExtendedText();
            u8g.setFontPosTop();
            Temp = Temp_Meas();
            u8g.drawStr(2, 0, "Stabilize the column!");
            u8g.drawStr(2, 10, "Alembic temperature: ");
            u8g.drawStr(2, 20, String(Temp).c_str());
            u8g.drawStr(2, 30, "Time left:");
            int hours = (time_buf-nowTime)/3600;
            int minutes = ((time_buf-nowTime)-(hours*3600))/60;
            int sec = (time_buf-nowTime)-((minutes*60)+(hours*3600));
            String hourZero = "";
            String minuteZero = "";
            String secZero = "";
            if(hours<10) hourZero = "0";
            if(minutes<10) minuteZero = "0";
            if(sec<10) secZero = "0";
            String printLine = hourZero + String(hours) + ":" + minuteZero + String(minutes) + ":" + secZero + String(sec);
            u8g.drawStr(2, 40, printLine.c_str());
            u8g.drawStr(2, 50, "Hold button to skip!");
        } while( u8g.nextPage() );
        switch (enc_state)
        {
            
            case LONG_CLICK_ENCODER:
            {
                Stage++;
                digitalWrite(beeper, 1);
                delay(1000);
                digitalWrite(beeper, 0);
            }
            break;
        }
        enc_state = 0;
    }
    else
    {
        Stage++;
    }
}

void Stabilization_check()
{
    Temp = Temp_Meas();
    float TempArray[90];
    for (int i=0; i< 90; i++) { // собираем показания температуры за 1,5 минуты
        u8g.firstPage();
        do 
        {
            u8g.setFont(u8g_font_6x12);
            u8g.setFontRefHeightExtendedText();
            u8g.setFontPosTop();
            Temp = Temp_Meas();
            u8g.drawStr(2, 0, "Stability check!");
            u8g.drawStr(2, 10, "Alembic temperature: ");
            u8g.drawStr(2, 20, String(Temp).c_str());
            u8g.drawStr(2, 50, "Hold button to skip!");
        } while( u8g.nextPage() );   
        TempArray[i] = Temp_Meas();
        switch (enc_state)
        {
            
            case LONG_CLICK_ENCODER:
            {
                Stage++;
                enc_state = 0;
                time_buf = millis()/1000 + HeadSamplingTime;
                digitalWrite(beeper, 1);
                delay(1000);
                digitalWrite(beeper, 0);
                digitalWrite(RELAY_IN, 1);
                return;
            }
            break;
        }
        enc_state = 0;
        delay(1000);
    }
    for (int i=0; i< 90; i++) {
        if((TempArray[i]+TempDelta < Temp) or (TempArray[i]-TempDelta > Temp))
        {
        return;
        }
    }
    u8g.firstPage();
    do 
    {
        u8g.setFont(u8g_font_6x12);
        u8g.setFontRefHeightExtendedText();
        u8g.setFontPosTop();
        Temp = Temp_Meas();
        u8g.drawStr(2, 0, "The column is stable!");
        u8g.drawStr(2, 10, "Alembic temperature: ");
        u8g.drawStr(2, 20, String(Temp).c_str());
        u8g.drawStr(2, 50, "Press button to next!");
    } while( u8g.nextPage() );
    digitalWrite(beeper, 1);
    delay(10000);
    digitalWrite(beeper, 0);  
    while(true)
    {
        switch (enc_state)
        {
            case CLICK_ENCODER:
            {
                Stage++;
                enc_state = 0;
                time_buf = millis()/1000 + HeadSamplingTime;
                digitalWrite(RELAY_IN, 1);
                return;
            }
            break;
        }
        enc_state = 0;
    }
}

void Head_Sampling()
{
    unsigned long nowTime = millis()/1000;
    if( nowTime < time_buf )
    {
        u8g.firstPage();
        do 
        {
            u8g.setFont(u8g_font_6x12);
            u8g.setFontRefHeightExtendedText();
            u8g.setFontPosTop();
            Temp = Temp_Meas();
            u8g.drawStr(2, 0, "Head sampling!");
            u8g.drawStr(2, 10, "Alembic temperature: ");
            u8g.drawStr(2, 20, String(Temp).c_str());
            u8g.drawStr(2, 30, "Time left:");
            int hours = (time_buf-nowTime)/3600;
            int minutes = ((time_buf-nowTime)-(hours*3600))/60;
            int sec = (time_buf-nowTime)-((minutes*60)+(hours*3600));
            String hourZero = "";
            String minuteZero = "";
            String secZero = "";
            if(hours<10) hourZero = "0";
            if(minutes<10) minuteZero = "0";
            if(sec<10) secZero = "0";
            String printLine = hourZero + String(hours) + ":" + minuteZero + String(minutes) + ":" + secZero + String(sec);
            u8g.drawStr(2, 40, printLine.c_str());
            u8g.drawStr(2, 50, "Hold button to skip!");
        } while( u8g.nextPage() );
        switch (enc_state)
        {
            
            case LONG_CLICK_ENCODER:
            {
                Stage++;
                digitalWrite(beeper, 1);
                delay(1000);
                digitalWrite(beeper, 0);
                FixTemp = Temp_Meas();
                enc_state = 0;
            }
            break;
        }
        enc_state = 0;
    }
    else
    {
        Stage++;
        digitalWrite(beeper, 1);
        delay(10000);
        digitalWrite(beeper, 0);
        FixTemp = Temp_Meas();
    }

}

void Heart_Sampling()
{
    u8g.firstPage();
    do 
    {
        u8g.setFont(u8g_font_6x12);
        u8g.setFontRefHeightExtendedText();
        u8g.setFontPosTop();
        Temp = Temp_Meas();
        u8g.drawStr(2, 0, "Heart sampling!");
        u8g.drawStr(2, 10, "Alembic temperature: ");
        u8g.drawStr(2, 20, String(Temp).c_str());
    } while( u8g.nextPage() );
    if(((FixTemp+TempDelta<Temp) || (FixTemp-TempDelta>Temp)) && FixTemp>-300)
    {
        FaultTailKeeper++;
        if(FaultTailKeeper > 3)
        {
            Stage++;
            digitalWrite(RELAY_IN, 0);
            time_buf = millis()/1000 + ColumnStableTime;
            digitalWrite(beeper, 1);
            delay(1000);
            digitalWrite(beeper, 0);
            return;
        }
        delay(1000);
    }
    else
    {
        FaultTailKeeper = 0;
    }
    

}

void Try_Stabling()
{
    if( millis()/1000 > time_buf ) 
    {
        Stage = 10;
        return;
    }
    float TempArray[90];
    for (int i=0; i< 90; i++) { // собираем показания температуры за 1,5 минуты
        u8g.firstPage();
        do 
        {
            u8g.setFont(u8g_font_6x12);
            u8g.setFontRefHeightExtendedText();
            u8g.setFontPosTop();
            Temp = Temp_Meas();
            u8g.drawStr(2, 0, "Column unstable!");
            u8g.drawStr(2, 10, "Trying to stabilize!");
            u8g.drawStr(2, 20, "Alembic temperature: ");
            u8g.drawStr(2, 30, String(Temp).c_str());
            u8g.drawStr(2, 50, "Press button to stop!");
        } while( u8g.nextPage() );   
        TempArray[i] = Temp_Meas();
        switch (enc_state)
        {
            case CLICK_ENCODER:
            {
                Stage = 10;
                enc_state = 0;
                return;
            }
            break;
        }
        enc_state = 0;
        delay(1000);
    }
    for (int i=0; i< 90; i++) {
        if((TempArray[i]+TempDelta < FixTemp) or (TempArray[i]-TempDelta > FixTemp))
        {
            return;
        }
    }
    Stage = 5;
    digitalWrite(RELAY_IN, 1);

}

void Stop_Sampling()
{
    digitalWrite(RELAY_IN, 0);
    u8g.firstPage();
    do 
    {
        u8g.setFont(u8g_font_6x12);
        u8g.setFontRefHeightExtendedText();
        u8g.setFontPosTop();
        u8g.drawStr(2, 30, "Sampling is over!");
    } while( u8g.nextPage() ); 
}
