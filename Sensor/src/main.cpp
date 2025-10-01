#include <Arduino.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ModbusIP_ESP8266.h>

#define SENSOR_PIN 26
#define RESET_PIN 33
#define UDP_PORT 1234

WiFiManager WFM;
WiFiUDP UDP;
ModbusIP MB;         

IPAddress gatewayIP;  

  
bool last_State = false; //trạng thái trước của sensor
bool real_State = false; //trạng thái chốt sau thời gian chống nhiẽu
unsigned long SensorTime_tmp = 0;
const unsigned long debounceTime = 3000; // sau 3s nếu trạng thái đúng => truetrue
bool gotGateway = false; //cờ kết nối gateway


bool Check_Timer(unsigned long &timer_tmp, uint16_t time);
bool Check_Sensor();

enum State {
  STATE_WIFI_CONFIG,
  STATE_UDP_WAIT,
  STATE_MODBUS_RUN
};
State STATE = STATE_WIFI_CONFIG;


void setup() 
{
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT);
  pinMode(RESET_PIN, INPUT_PULLUP);
}

void loop() 
{

  switch (STATE) 
  {
    case STATE_WIFI_CONFIG: 
    {
      Serial.println("=== Config WiFi (Sensor) ===");
      bool res = WFM.autoConnect("ESP_Sensor", "12345678");
      if (res) 
      {
        Serial.println("WiFi connected ==> STATE_UDP_WAIT");
        UDP.begin(UDP_PORT);   
        gotGateway = false;    
        STATE = STATE_UDP_WAIT;
      } 
      else 
      {
        Serial.println("Failed WiFi config -> restart");
        ESP.restart();
      }
      break;
    }

    case STATE_UDP_WAIT: 
    {
      if(digitalRead(RESET_PIN) == 0)
      {
        Serial.println("Button pressed => Reset Wifi");
        WFM.resetSettings();
        ESP.restart();
        STATE = STATE_WIFI_CONFIG;
      }
      
      if (gotGateway == 0) 
      {  
        int packetSize = UDP.parsePacket();

        if (packetSize) 
        {
          char buf[64];
          int len = UDP.read(buf, sizeof(buf) - 1);
          if (len > 0) buf[len] = 0;
          Serial.printf("[UDP] Received: %s\n", buf);

          if (strstr(buf, "GATEWAY_IP=")) 
          {
            String ipStr = String(buf).substring(11);
            if (gatewayIP.fromString(ipStr) == 1) //chuyển chuỗi string về dạng là IPAdress
            {
              Serial.printf("[UDP] Parsed Gateway IP: %s\n", gatewayIP.toString().c_str());
              //lấy được ip kết nối với gateway
              MB.client();
              MB.connect(gatewayIP, 502);

              UDP.beginPacket(gatewayIP, UDP_PORT);
              UDP.print("SENSOR_OK");
              UDP.endPacket();
              Serial.println("[UDP] Sent: SENSOR_OK");

              gotGateway = true;    
              STATE = STATE_MODBUS_RUN;
            }
          }
        }
      }
      break;
    }

    case STATE_MODBUS_RUN: 
    {
      if (WiFi.status() != WL_CONNECTED) 
      {
        Serial.println("WiFi lost ==> STATE_WIFI_CONFIG");
        STATE = STATE_WIFI_CONFIG;
        break;
      }
      
      if(digitalRead(RESET_PIN) == 0)
      {
        Serial.println("Button pressed => Reset Wifi");
        WFM.resetSettings();
        ESP.restart();
        STATE = STATE_WIFI_CONFIG;
      }
      MB.task();

      if (MB.isConnected(gatewayIP) == 0) 
      {
        Serial.println("Modbus disconnected -> back to UDP wait");
        gotGateway = false;
        STATE = STATE_UDP_WAIT;
        break;
      }
      if(Check_Sensor() == 0)
      {
        Serial.println("co hang");
      }
      else Serial.println("khong co hang");

      break;
    }
  }
}

bool Check_Sensor() 
{
    bool currentState = digitalRead(SENSOR_PIN); // LOW = có hàng

    if (currentState != last_State) 
    {
      last_State = currentState;
      SensorTime_tmp = millis();
    }

    if ((millis() - SensorTime_tmp >= debounceTime) && (currentState != real_State)) 
    {
      real_State = currentState;
      MB.writeHreg(gatewayIP, 0, real_State ); //0 - co, 1 - ko
      Serial.printf("[Modbus] Sent state = %d\n", real_State);
    }

  return real_State; 
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
