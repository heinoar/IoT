/*********
Ari Heinonen

*********/


// Load Wi-Fi library
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>


Adafruit_BME280 bme; // I2C

// WiFi asetukset
const char* ssid = "xxxxxx";
const char* password = "xxxxx";
// web serveri kuuntelee porttia TCP/80
//luodaan server-niminen olio, joka käynnistetään pääohjelmassa
WiFiServer server(80);

// http request tallennetaan tähän
String header;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;
//Määritellään PIR-sensorin pinnit:
  byte pirSensorPin = 26;
  byte redPin = 16; 
//Määritellään valoisuussensorin pinni
  const int sensorLuxPin=34;

//Pääohjelma
void setup() {
  Serial.begin(115200);
  bool status;

  //Tarkistetaan onko bme kytketty
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  //Määritellään PIR-sensorin ja ledin pinni liikennöintisuunnat:
  pinMode(pirSensorPin,INPUT);
  pinMode(redPin,OUTPUT); 

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Tulostetaan konsolille ip-asetukset ja käynnistetään web-serveri.
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop(){
  WiFiClient client = server.available();   // 
  if (client) {                             // Jos käyttäjä kytkeytyy
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // sarjaporttiin tulostus
    String currentLine = "";                // luetaan käyttäjältä tuleva rivi
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // Luetaan käyttäjän dataa niin kauan kun se on kytkeytyneenä
      currentTime = millis();
      if (client.available()) {             // luetaan dataa jos istunto päällä
        char c = client.read();             // luetaan tavu kerrallaan
        Serial.write(c);                    // tulostetaan konsolille
        header += c;
        if (c == '\n') {                    // jos tulee rivin vaihto odotetaan toista rivin vaihtoa
                                            //toinen peräkkäinen rivin vaihto tarkoittaa requestin loppua
          if (currentLine.length() == 0) {
           
            // Vastataan käyttäjälle palautuskoodilla 200 ja laitetaan sensoriarvot mukaan
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type: text/html; charset=UTF-8"); 
            client.println("Connection: close");
            client.println();
            
            // Muodostetaan HTML5 sivu:
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");

            //Sivun auto-refresh 2 sekunnin välein. Käytin tätä debuggauksessa. 
            //client.println("<head><meta http-equiv=\"refresh\" content=\"2\">");
            
            // CSS tyylit, oletustyyli sekä sensor. 
            client.println("<style>body { text-align: center; font-family: \"Trebuchet MS\", Arial;}");
            client.println("table { text-align: left; width:35%; margin-left:auto; margin-right:auto; }");
            client.println("th { padding: 12px; background-color: #0043af; color: white; }");
            client.println("tr { font-weight: bold; border: 1px solid #ddd; padding: 12px; }");
            client.println("tr:hover { background-color: #bcbcbc; }");
            client.println("td { border: none; padding: 12px; }");
            client.println(".sensor { color:black; font-weight: bold; padding: 1px; }");
            client.println("</style></head>");
            
            // Sivun sisältö
            client.println("<body><h1>ESP32 Mittaukset</h1>");
            //client.println("<table><tr><th>--------</th><th>-------</th></tr>");
            //Taulukon aloitus
            client.println("<table>");

            //Lämpötila
            client.println("<tr><td>Lämpötila</td><td><span class=\"sensor\">");
            client.println(bme.readTemperature());
            client.println(" &#176;C</span></td></tr>");
              
            //Kosteus
            client.println("<tr><td>Kosteus</td><td><span class=\"sensor\">");
            client.println(bme.readHumidity());
            client.println(" %</span></td></tr>"); 
            
            //Paine
            //Otetaan paineen arvo int-muuttujaan vaikka se taitaa olla float, poistan tällä desimaalit
            int paine=bme.readPressure();
            client.println("<tr><td>Paine</td><td><span class=\"sensor\">");
            client.println(paine / 100);
            client.println(" mbar</span></td></tr>");
            
            //Valoisuus
            client.println("<tr><td>Valoisuus</td><td><span class=\"sensor\">");
            client.println(analogRead(sensorLuxPin));
            client.println(" Lux</span></td></tr>");

            //Liikesensori
            byte state = digitalRead(pirSensorPin);
            digitalWrite(redPin,state);
            String stateString ="";
            if(state == 1) stateString="KYLLÄ";
            else if(state == 0) stateString="EI";
            client.println("<tr><td>Paikalla?</td><td><span class=\"sensor\">");
            client.println(stateString);
            client.println("</span></td></tr>");

            //Taulukon loppu
            client.println("</table>");
           
            //refresh nappi
            client.println("<button type=\"submit\" onClick=\"window.location.reload()\">Lataa tiedot</button>");
             
            client.println("</body></html>");
           
            // Vastauksen lopetus
            client.println();
            
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
