/*
AHe 21.10.2020

 */

#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>


//********************Globaalit muuttujat ja oliot**************************
//Wifi-olio
WiFiMulti WiFiMulti;

//HTTP clientit, olisi voinut käyttää samaakin
HTTPClient ask;
HTTPClient weather;

// Wifi-verkon tiedot
const char* ssid = "xxxx";
const char* password = "xxxxxx";

//Asksensors api-keyt
const char* apiKeyIn = "X525tVMIoWpdEwzzOIWcnDAZpZJAv1sN";      
const char* apiKeyIn2 = "8TBEnk3VO6xE0mxj2PJX8IOMfL9bKZnv"; 

//writeInterval määrittelee kuinka usein arvot lähetetään, 1h=3 600 000ms
const unsigned int writeInterval = 3600000;
unsigned long int aika = 0;

//Muodostetaan bme-olio, jolla sensoreihin pääsee käsiksi
Adafruit_BME280 bme; 

//Määritellään PIR-sensorin ja ledin pinnit:
byte pirSensorPin = 26;
byte redPin = 16;

//PIR muutoksen tilaa kuvaava muuttuja, joka asetetaan arvoon 1 PIR-keskeytyksessä
boolean pirChanged = 0;

//Määritellään valoisuussensorin pinni
const int sensorLuxPin=34;

// ASKSENSORS API host tiedot
const char* host = "api.asksensors.com";  // API host name
const int httpPort = 80;      // port

//openweathermap.org apin host tiedot, portiksi käy httpPort-muuttuja
const char* hostOW = "api.openweathermap.org";  // API host name


//********************setup() ajetaan ohjelman käynnistyksen yhteydessä***********
void setup(){
  // alustetaan sarjalinkki
  Serial.begin(115200);
  Serial.println("*****************************************************");
  Serial.println("********** Program Start : Connect ESP32 to AskSensors.");
  Serial.println("Wait for WiFi... ");

  // kytkeydytään wifi-verkkoon
  WiFiMulti.addAP(ssid, password);
  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  // Ip-tiedot yms.. tulostetaan konsolille
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //Tarkistetaan onko bme kytketty
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  //Määritellään PIR ja led pinnien suunnat
  pinMode(pirSensorPin,INPUT);
  pinMode(redPin,OUTPUT);

  //Määritellään PIR-sensorin keskeytys
  attachInterrupt(digitalPinToInterrupt(pirSensorPin),tunnistus,CHANGE);
}

//************** loop() osiota ajetaan ohjelman loppuun asti ***************
void loop(){
  //Aika nollataan tässä
  aika=millis();
  
  // WiFiclientillä luodaan client-niminen olio, joka osaa avata tcp-yhteyksiä
  WiFiClient client;
  //Avataan TCP-istunto ja onnistumisen jälkeen jatketaan
  if (!client.connect(host, httpPort)) {   
    Serial.println("connection failed");
    return;
  }else {
    
    //** Sensor1 datan lähetys **

    //Url, johon tiedot lähetetään
    String url = "http://api.asksensors.com/write/";
    //API key
    url += apiKeyIn;
    //Lämpötila
    url += "?module1=";
    url += bme.readTemperature();
    //Lämpötila toista graafia varten
    url += "&module2=";
    url += bme.readTemperature();
    //Suhteellinen kosteus
    url += "&module3=";
    url += bme.readHumidity();
    //Ilmanpaine mbar
    int paine=bme.readPressure()/100;
    url += "&module4=";
    url += paine;
  
    Serial.print("********** Start sending data to Sensor1: ");
    Serial.println(url);                    
    
    //Lähetetään edellä koostettu url. ask-olio on määritelty globaalissa osiossa
    ask.begin(url); 
  
    //Tarkistetaan mikä on palautuskoodi
    int httpCode = ask.GET();          
 
    if (httpCode > 0) {                 
        //Vastauksena pitäisi tulla koodi 200 sekä lähetettyjen parametrien määrä
        String payload = ask.getString(); //getString metodi palauttaa payloadin
        Serial.println(httpCode);         
        Serial.println(payload);          
      } else {
      Serial.println("Error on HTTP request");
    }
    //lopetetaan http-sessio
    ask.end();
    Serial.println("********** End sending data to Sensor1  ");

    //** Sensor1 datan lähetys **
    
    //koostetaan url, johon tiedot lähetetään
    String url2 = "http://api.asksensors.com/write/";
    
    url2 += apiKeyIn2;
    //Valoisuus
    url2 += "?module1=";
    url2 += analogRead(sensorLuxPin);
    //module4 tiedot haetaan aliohjelmasta updateOutsideTemp()
    url2 += "&module4=";
    url2 += updateOutsideTemp();                              
    //Huom! module2 ja 3 tiedot lähetetään sendPir() aliohjelmasta         
  
    Serial.print("********** Start sending data to Sensor2: ");
    Serial.println(url2); 
    //lähetetään sensor2 tiedot  
    ask.begin(url2); 
    int httpCode2 = ask.GET();          
 
    if (httpCode > 0) {                   
        String payload2 = ask.getString(); 
        Serial.println(httpCode2);         
        Serial.println(payload2);          
      } else {
      Serial.println("Error on HTTP request");
    }
    ask.end(); //End 
    Serial.println("********** End sending data to Sensor2  ");
 
    //Lopuksi katkaistaan tcp-istunto
    client.stop();                            

  }
  
  //Ajetaan seuraavaa while-looppia, writeInterval ajan verran, 
  //jonka jälkeen palataan void loop() alkuun ja lähetetään jälleen muut arvot
  //while-loopin sisällä tutkitaan onko globaali-muuttuja pirChanged arvossa 1
  //pirChanged on 1 jos ohjelman ajon aikana on tullut PIR-sensorin aiheuttama keskeytys
  //Suurin osa ohjelman ajallisesta kulusta ollaan tässä while-loopissa
  while(millis()<aika+writeInterval){
    if(pirChanged){
      sendPIR();
      pirChanged = 0;
      delay(1000);                          
    }
  }
 }

//****************** tunnistus() funktio ajetaan, jos tulee pir-sensorin keskeytys
void tunnistus(){
  pirChanged = 1;
}

//****************** sensorPIR() lähettää pir-sensorin tilan pilveen
//Käytetyt menetelmät ovat samat kuin edellä
void sendPIR(){
  
  WiFiClient client2;
  byte state = digitalRead(pirSensorPin);
  digitalWrite(redPin,state);
  Serial.print("PIR-tila:");
  Serial.println(state);
  if (!client2.connect(host, httpPort)) {   
    Serial.println("connection failed");
    return;
  }else {
  
    String url = "http://api.asksensors.com/write/";
    url += apiKeyIn2;                                          
    url += "?module2=";
    url += state;
    url += "&module3=";
    url += state;
    ask.begin(url); 
    int httpCode = ask.GET();          
    if (httpCode > 0) {                   
 
        String payload = ask.getString(); 
        Serial.println(httpCode);         
        Serial.println(payload);          
      } else {
      Serial.println("Error on HTTP request");
    }
    ask.end(); //End 
  }
  client2.stop();
       
}

//**************** float updateOutsideTemp() ohjelmaa kutsutaan jos halutaan tietää ulkoilman lämpötila
//funktio on rajoittunut hakemaan Kokkolan lämpötilan, mutta toki sitä voidaan muuttaa vaihtamalla
//apikey ja paikkakunnan nimi
//funktio palauttaa float-tyyppisenä celsius lämpötilan
float updateOutsideTemp(){
  //Haetaan tiedot sääpalvelusta
  WiFiClient client;
  if (!client.connect(hostOW, httpPort)) {   //Avataan tcp-yhteys ja onnistumisen jälkeen jatketaan
     Serial.println("Openweather.org connection failed");
     return NULL;
  }else{
    Serial.println("Openweathermap.org tcp-yhteys avattu");
    //Haetaan JSON data. Url on tarjottu valmiina, joten sitä ei tarvitse rakennella
    String url = "http://api.openweathermap.org/data/2.5/weather?q=Kokkola&appid=3ad47f513f5e580203d5790e72d26e16";
    weather.begin(url); 
    int httpCode = weather.GET();          
    String payload=weather.getString();
    Serial.println(payload);
    Serial.println(httpCode);

    //Luodaan Json-dokumentti ja määritellään sen koko
    //capacity muuttujan tiedot on haettu https://arduinojson.org/v6/assistant/
    //palvelusta.
    const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(13) + 260;
    DynamicJsonDocument doc(capacity);
    
    // Parseroidaan JSON
    //desrialisointi muodostaa string-muotoisesta jsonista (muuttuja payload) taulukon,
    //jonka jälkeen kenttiin pääsee viittaamaan
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      //palautetaan tyhjä jos tulee virhe
      return NULL;
      }
      //Deserialisoinnin onnistuessa:
      else{
        //Nämä muuttujat voi hakea suoraan
        //https://arduinojson.org/v6/assistant/ antaman koodin perusteella
        JsonObject main = doc["main"];
        float main_temp = main["temp"]; 
        Serial.println(main_temp);
        //muutetaan kelvinit celsiuksiksi
        float return_value=main_temp-273.15;
        //palautetaan lämpötilan arvo
        return return_value;
      }
    client.stop();                            //Lopetetaan tcp-yhteys
    }
  }
