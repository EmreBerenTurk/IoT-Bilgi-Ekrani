#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h> 
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h> 
#include <DHT.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

const char* ssid     = "fehim";     
const char* password = "12345678";   

// Pin Tanimlari Sensör
#define DHTPIN D4     
#define DHTTYPE DHT11 

// BUTON PINLERI (Hepsi GND tetiklemeli)
#define BTN1 D5 // Saat
#define BTN2 D6 // Sicaklik
#define BTN3 D7 // Dolar/Euro
#define BTN4 D3 // Sterlin/Riyal
#define BTN5 3  // RX Pini (GPIO3) -> Yen/Ruble
#define BTN6 1  // TX Pini (GPIO1) -> Ping

// Nesneler
LiquidCrystal_I2C lcd(0x27, 16, 2); 
DHT dht(DHTPIN, DHTTYPE);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800); // GMT+3

// Degiskenler

int currentMode = 1; 
int previousMode = 0; 
unsigned long lastUpdate = 0;
const long updateInterval = 120000; // 2 dakikada bir guncelle

// Veri Saklama Degiskenleri
String strDolar = "..";
String strEuro = "..";
String strSterlin = ".."; 
String strRiyal = "..";
String strYen = "..";   // Japon Yeni
String strRuble = ".."; // Rus Rublesi

float temp = 0.0;
float hum = 0.0;
int lastPing = 0;

byte derece[8] = {0x07,0x05,0x07,0x00,0x00,0x00,0x00,0x00}; //derece sembolü gösterimi

void setup() {
  
  // Pin Modlari 
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);
  pinMode(BTN4, INPUT_PULLUP);
  
  pinMode(BTN5, FUNCTION_3); 
  pinMode(BTN5, INPUT_PULLUP);
  
  pinMode(BTN6, FUNCTION_3); 
  pinMode(BTN6, INPUT_PULLUP);

  dht.begin();
  lcd.init();
  lcd.backlight();
  lcd.createChar(1, derece);

  // Acilis
  lcd.setCursor(0, 0);
  lcd.print("CANLI DOVIZ");
  lcd.setCursor(0, 1);
  lcd.print("Lutfen Bekleyin");
  delay(3000); 

  // Wifi Baglantisi
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wifi Baglaniyor");
  
  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid, password);

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    timeout++;
  }

  lcd.clear();
  if(WiFi.status() == WL_CONNECTED){
      lcd.setCursor(0, 0);
      lcd.print("Sunucuya");
      lcd.setCursor(0, 1);
      lcd.print("Baglandi");
      
      getDataFromNet(); 
      timeClient.begin();
      timeClient.update(); 
  } else {
      lcd.setCursor(0, 0);
      lcd.print("Baglanti Hatasi");
      delay(2000);
  }
  delay(2000); 
  lcd.clear();
}

void loop() {
  timeClient.update();
  handleButtons(); 

  if (millis() - lastUpdate > updateInterval) {
    getDataFromNet();
    lastUpdate = millis();
  }

  if (currentMode != previousMode) {
    lcd.clear();
    previousMode = currentMode;
  }
  
  refreshScreen();
  delay(50); 
}

void handleButtons() {
  // Artik hepsi LOW (GND) tetiklemeli
  if (digitalRead(BTN1) == LOW) { currentMode = 1; }
  else if (digitalRead(BTN2) == LOW) { currentMode = 2; }
  else if (digitalRead(BTN3) == LOW) { currentMode = 3; }
  else if (digitalRead(BTN4) == LOW) { currentMode = 4; }
  else if (digitalRead(BTN5) == LOW) { currentMode = 5; } 
  else if (digitalRead(BTN6) == LOW) { currentMode = 6; } 
}

void refreshScreen() {
  switch (currentMode) {
    case 1: // SAAT
      lcd.setCursor(0, 0);
      lcd.print("Tarih/Saat:     ");
      lcd.setCursor(0, 1);
      lcd.print(timeClient.getFormattedTime());
      break;

    case 2: // SICAKLIK
      temp = dht.readTemperature();
      hum = dht.readHumidity();
      lcd.setCursor(0, 0);
      if (isnan(temp)) {
        lcd.print("Sensor Hatasi!  ");
      } else {
        lcd.print("Sicaklik: " + String((int)temp) + " ");
        lcd.write(1); 
        lcd.print("C  ");
        lcd.setCursor(0, 1);
        lcd.print("Nem: %" + String((int)hum) + "     ");
      }
      break;

    case 3: // DOLAR / EURO
      lcd.setCursor(0, 0);
      lcd.print("USD: " + strDolar + " TL   ");
      lcd.setCursor(0, 1);
      lcd.print("EUR: " + strEuro + " TL   ");
      break;

    case 4: // STERLIN / RIYAL
      lcd.setCursor(0, 0);
      lcd.print("GBP: " + strSterlin + " TL   ");
      lcd.setCursor(0, 1);
      lcd.print("SAR: " + strRiyal + " TL   ");
      break;
    
    case 5: // JAPON YENI / RUBLE (YENI EKRAN)
      lcd.setCursor(0, 0);
      lcd.print("JPY: " + strYen + " TL   ");
      lcd.setCursor(0, 1);
      lcd.print("RUB: " + strRuble + " TL   ");
      break;

    case 6: // PING
       if (lastPing == 0 || millis() % 2000 < 100) { 
         calculatePing(); 
       }
      lcd.setCursor(0, 0);
      lcd.print(WiFi.status() == WL_CONNECTED ? "Wi-Fi: Bagli    " : "Wi-Fi: Kopuk    ");
      lcd.setCursor(0, 1);
      lcd.print("Ping: " + String(lastPing) + " ms     ");
      break;
  }
}

void calculatePing() {
  if(WiFi.status() == WL_CONNECTED) {
    IPAddress ip(8, 8, 8, 8); 
    unsigned long start = millis();
    WiFiClient client;
    if (client.connect(ip, 80)) {
      lastPing = millis() - start;
      client.stop();
    } else {
      lastPing = 999;
    }
  } else {
    lastPing = 0;
  }
}

void getDataFromNet() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); 
    HTTPClient http;

    // API'den verileri cek
    if (http.begin(client, "https://api.exchangerate-api.com/v4/latest/TRY")) {
      int httpCode = http.GET();
      if (httpCode > 0) {
        String payload = http.getString();
        
        DynamicJsonDocument doc(3072); // JSON boyutu artti, hafizayi arttirdik
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          float usdRate = doc["rates"]["USD"];
          float eurRate = doc["rates"]["EUR"];
          float gbpRate = doc["rates"]["GBP"]; 
          float sarRate = doc["rates"]["SAR"];
          float jpyRate = doc["rates"]["JPY"];
          float rubRate = doc["rates"]["RUB"]; 

          // Verileri String'e cevir (Sadece ilk 5 karakter)
          strDolar = String(1.0 / usdRate).substring(0, 5); 
          strEuro = String(1.0 / eurRate).substring(0, 5);
          strSterlin = String(1.0 / gbpRate).substring(0, 5);
          strRiyal = String(1.0 / sarRate).substring(0, 5);
          strYen = String(1.0 / jpyRate).substring(0, 5);
          strRuble = String(1.0 / rubRate).substring(0, 5);
        }
      }
      http.end();
    }
  }
}