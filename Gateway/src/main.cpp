#include <Arduino.h>
#include <WiFiManager.h>
#include <ModbusIP_ESP8266.h>
#include <WiFiUdp.h>
#include <LiquidCrystal_I2C.h>



#define RESET_PIN 0 
#define UDP_PORT 1234
#define UDP_TimeSend 2000
#define Time_LCD 1000 

WiFiManager WFM;
ModbusIP MB;
WiFiUDP UDP;
LiquidCrystal_I2C LCD(0x27, 16, 2);

unsigned long UDP_Timer = 0;
unsigned long LCD_Timer = 0;
unsigned long SENSOR_Send = 0;
String tmp_line1 = " ";
String tmp_line2 = " ";
bool sensor_udp = false;

bool Check_Timer(unsigned long &timer_tmp, uint16_t time);
void UpdateLCD(String line1, String line2);

enum State 
{
  STATE_WIFI_CONFIG,
  STATE_MODBUS,
  STATE_RUN
};
State STATE = STATE_WIFI_CONFIG;

void setup() 
{
  LCD.init();
  LCD.backlight();
  Serial.begin(115200);
  pinMode(RESET_PIN, INPUT_PULLUP);
}

void loop() 
{
  switch (STATE) 
  {
    case STATE_WIFI_CONFIG: 
    {
      Serial.println("=== Config WiFi ===");
      WiFi.begin();

      if (WiFi.status() == WL_CONNECTED) 
      {
        Serial.println("WiFi connected ==> STATE_MODBUS");
        UpdateLCD("STA CONFIG WIFI", "WIFI OK");
        STATE = STATE_MODBUS;
      } 

      else 
      {
        Serial.println("Start Config Portal");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP_Gateway", "12345678");

        Serial.println("AP IP: " + WiFi.softAPIP().toString());
        tmp_line1 = "";
        tmp_line2 = "";
        UpdateLCD("CONFIG PORTAL", WiFi.softAPIP().toString());
        
        WFM.startConfigPortal("ESP_Gateway", "12345678");

        if (WiFi.status() == WL_CONNECTED) 
        {
          UpdateLCD("STA CONFIG WIFI", "WIFI OK");
          STATE = STATE_MODBUS;
        } else 
        {
          UpdateLCD("STA CONFIG WIFI","FAIL => RESTART");
          ESP.restart();
        }
      }
      break;
    }

    case STATE_MODBUS: 
    {
      if (WiFi.status() == WL_CONNECTED) 
      {
        Serial.println("=== Setup Modbus TCP Server ===");
        UpdateLCD("STA MODBUS", WiFi.localIP().toString());
        MB.server(502);
        MB.addHreg(0, 0);  

        UDP.begin(1234);
        
        STATE = STATE_RUN;
      } 
      else 
      {
        Serial.println("WiFi lost ==> STATE_WIFI_CONFIG");
        UpdateLCD("WIFI LOST", "CONFIG WIFI");
        STATE = STATE_WIFI_CONFIG;
      }

      break;
    }

    case STATE_RUN: 
    {
      if (digitalRead(RESET_PIN) == LOW) 
      {
        Serial.println("Button pressed -> Reset WiFi");
        UpdateLCD("Button Reset ","STA_CONFIG_WIFI");
        WFM.resetSettings();
        ESP.restart();
        STATE = STATE_WIFI_CONFIG;
      }

      if (WiFi.status() != WL_CONNECTED) 
      {
        Serial.println("WiFi lost ==> STATE_WIFI_CONFIG");
        UpdateLCD("WIFI LOST", "CONFIG WIFI");
        STATE = STATE_WIFI_CONFIG;

      }

      if(Check_Timer(UDP_Timer, UDP_TimeSend) == 1 && sensor_udp == 0)
      {
        String ip_show_lcd = WiFi.localIP().toString();
        String ip_addr = "GATEWAY_IP=" + WiFi.localIP().toString();
        UDP.beginPacket("255.255.255.255", UDP_PORT);
        UDP.print(ip_addr);
        UDP.endPacket();
        Serial.println("UDP Broadcast: " + ip_addr);
        UpdateLCD("UDP Send", ip_show_lcd);

      }

      //check sensor udp
      int packet_size = UDP.parsePacket();
      if(packet_size)
      {
        char buf[32];
        int len = UDP.read(buf, sizeof(buf) - 1);
        if(len > 0) buf[len] = 0;
        
        if(String(buf) == "SENSOR_OK")
        {
          sensor_udp = true;
          Serial.println("Sensor receive udp");
          tmp_line1 = "";
          tmp_line2 = "";          
          UpdateLCD("UDP Send", "Sensor Received");
        }
      }

      MB.task(); 
      if(Check_Timer(SENSOR_Send, 2000))
    {
        if(MB.Hreg(0) == 1)
        {
          Serial.println("NO");
          UpdateLCD("PRODUCT", "NO");
        }
        else Serial.println("YES");
        UpdateLCD("PRODUCT", "YES");      
    }
      break;
    }
  }
}

void UpdateLCD(String line1, String line2) 
{
    if (millis() - LCD_Timer < Time_LCD) return; // tránh update quá nhanh
  LCD_Timer = millis();
  // Chỉ cập nhật khi nội dung thay đổi để tránh nhấp nháy
  if (line1 != tmp_line1 || line2 != tmp_line2) 
  {
    LCD.clear();
    LCD.setCursor(0, 0);
    LCD.print(line1);
    LCD.setCursor(0, 1);
    LCD.print(line2);

    tmp_line1 = line1;
    tmp_line2 = line2;
  }
}



bool Check_Timer(unsigned long &timer_tmp, uint16_t time) 
{
  if (millis() - timer_tmp >= time) 
  {
    timer_tmp = millis();
    return true;
  }
  return false;
}
