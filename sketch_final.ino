#define R D3
#define G D2
#define B D1

#include <ESP8266WebServer.h>
#include <WiFiUDP.h>
#include <Color.h>
#include "FS.h"

#include "arduinoFFT.h"
#define SAMPLES 16             //Must be a power of 2
#define SAMPLING_FREQUENCY 1000 //Hz, must be less than 10000 due to ADC
arduinoFFT FFT = arduinoFFT();
unsigned int sampling_period_us;
unsigned long microseconds;
double vReal[SAMPLES];
double vImag[SAMPLES];

const char *wififilepath = "/wifi.txt";
const char *conffilepath = "/conf.txt";
const int FPS = 40;

ESP8266WebServer server(80);
WiFiUDP Udp;
bool isAP =  true;

int type = 0;
int arrayindex = 0;
int speed = 10;
String name = "Scolor";
bool soundRecognition = false;
Color *needed = new Color(),
    *start = new Color(),
    *current = new Color();

int curPeak = 0;
int topPeak = 0;
int stepPeak = 0;
int stepunitPeak = 0;
int currentMillis = millis();
int maxPeaks[10] = {0};

void connection();
void handleColor();
void handleConnection();
void handleRandom();
void handleAllColors();
void handleIntensity();
void handleSpeed();
void handleSoundReco();
void handleInfos();
void handleName();
void NewRandomize(int);
void AllColorsNext(int);

void setup() {

  SPIFFS.begin();
  Serial.begin(74880);
  
  sampling_period_us = round(1000000*(1.0/SAMPLING_FREQUENCY));
    
  pinMode(R,OUTPUT);
  pinMode(G,OUTPUT);
  pinMode(B,OUTPUT);

  if (SPIFFS.exists(conffilepath))
  {
    File conffile = SPIFFS.open(conffilepath, "r");
    name = conffile.readStringUntil('\n');
    conffile.close();
  }

  //Démarrage du serveur
  server.on("/leds", handleColor);
  server.on("/random", handleRandom);
  server.on("/allcolors", handleAllColors);
  server.on("/intensity", handleIntensity);
  server.on("/speed", handleSpeed);
  server.on("/soundreco", handleSoundReco);
  server.on("/infos", handleInfos);
  server.on("/name", handleName);
  server.on("/connection", handleConnection);
  server.begin();
  Serial.println("Serveur HTTP OK.");

  Udp.begin(32000);

  connection();
}

void loop() {

  if((millis() - currentMillis) >= (1000 / FPS * 10 / speed))
  {
    server.handleClient();
    
    int noBytes = Udp.parsePacket();
    byte packetBuffer[2];
    if (noBytes == 2) {
      Udp.read(packetBuffer,noBytes);
      if (packetBuffer[0] == 115 && packetBuffer[1] == 99) // "sc"
      {
        //On répond au broadcast seulement si l'envoi correspond à la string "sc"      
        Udp.beginPacket(Udp.remoteIP(), 32000);
        Udp.write("SColor;");
        Udp.print(name);
        Udp.write(";");
        Udp.print(isAP ? WiFi.softAPIP() : WiFi.localIP());
          
        Udp.endPacket();
      }
    }
  
    if (current->Frame < needed->Frame)
    {
      current->Frame++;
      current->R = start->R + ((needed->R - start->R) / (double)needed->Frame * (double)current->Frame);
      current->G = start->G + ((needed->G - start->G) / (double)needed->Frame * (double)current->Frame);
      current->B = start->B + ((needed->B - start->B) / (double)needed->Frame * (double)current->Frame);
      current->Intensity = start->Intensity + ((needed->Intensity - start->Intensity) / (double)needed->Frame * (double)current->Frame);

      if (!soundRecognition)
      {
        analogWrite(R, 0.95 * current->R / 255.0 * 1023.0 * current->Intensity/100);
        analogWrite(G, 0.98* current->G / 255.0 * 1023.0 * current->Intensity/100);
        analogWrite(B, current->B / 255.0 * 1023.0 * current->Intensity/100);
      }
    }
    else
    {
      if (type == 1) //Aléaoire
        NewRandomize(120);
      else if(type == 2) //Cercle chromatique
      {
        arrayindex = (arrayindex + 1) % 6;
        AllColorsNext(120);
      }
    }
    
    if (soundRecognition)
    {
      for(int i=0; i<SAMPLES; i++)
      {
          microseconds = micros();    //Overflows after around 70 minutes!
       
          vReal[i] = analogRead(A0);
          vImag[i] = 0; 
       
          while(micros() < (microseconds + sampling_period_us));
      }

      FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
      FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);
   
      int peak = 0;
      for(int i=0; i<(SAMPLES/2); i++)
          if (vReal[i] < 2000 && vReal[i] > 120 && peak < vReal[i])
              peak += vReal[i] / 2;
    
      if (peak > curPeak)
      {
        topPeak = peak;
        stepunitPeak = (topPeak - curPeak) / 3;
        stepPeak = 3;

        for(int i = 8; i >= 0; i--)
          maxPeaks[i+1] = maxPeaks[i];
        maxPeaks[0] = topPeak;
      }

      if (stepPeak-- > 0)
        curPeak += stepunitPeak;
      else if (curPeak * 0.7 <= peak)
        curPeak = peak;
      else if(curPeak * 0.7 > 0)
        curPeak *= 0.7;
      else
        curPeak = 0;

      int maxi = 0;
      for(int i = 0; i < 10; i++)
        if (maxi < maxPeaks[i])
          maxi = maxPeaks[i];

      float disp = maxi == 0 ? 0 : (float)curPeak / (float)maxi;

      analogWrite(R, 0.95 * current->R / 255.0 * 1023.0 * disp);
      analogWrite(G, 0.98* current->G / 255.0 * 1023.0 * disp);
      analogWrite(B, current->B / 255.0 * 1023.0 * disp);
    
      Serial.println(disp * 100.0, 1);
    }
    
    currentMillis = millis();
  }
}

void connection()
{
  WiFi.disconnect(true);
  
  //Lecture du fichier de conf
  if (SPIFFS.exists(wififilepath))
  {
    File wififile = SPIFFS.open(wififilepath, "r");
    String __ssid = wififile.readStringUntil('\n');
    char ssid[__ssid.length()];
    __ssid.toCharArray(ssid, __ssid.length());
    String __password = wififile.readStringUntil('\n');
    char password[__password.length()];
    __password.toCharArray(password, __password.length());
    wififile.close();
    Serial.print("Connexion au serveur connu ");
    Serial.print(ssid);
    Serial.println(".");
        
    WiFi.begin(ssid, password);

    int maxdelay = 10000;
    while (WiFi.status() != WL_CONNECTED && maxdelay > 0) {
      delay(500);
      maxdelay -= 500;
      Serial.print(".");
    }
    Serial.println("");

    if(maxdelay > 0)
    {
      isAP = false;
      Serial.println("Connexion ok.");
      return;
    }
    else
    {
      Serial.println("Serveur inaccessible.");
      Serial.print("Effacement du serveur : ");
      SPIFFS.remove(wififilepath);
      Serial.println("OK.");
    }
  }

  Serial.println("Aucun serveur connu.");
  Serial.println("Création d'un point d'accès autonome.");
  const char* __ssid = "SColor";
  const char* __password = "";
  WiFi.softAP(__ssid, __password);
  
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("IP : ");
  Serial.println(myIP);
  isAP = true;
}

void NewRandomize(int goalframe)
{
  start->ReplaceColor(*current);
  needed->RandomizeColor();
  needed->Frame = goalframe;
  current->Frame = 0;
}

void AllColorsNext(int goalframe)
{
  start->ReplaceColor(*current);
  needed->ChangeColor(arrayindex == 5 || arrayindex == 0 || arrayindex == 1 ? 255 : 0,
      arrayindex == 1 || arrayindex == 2 || arrayindex == 3 ? 255 : 0,
      arrayindex == 3 || arrayindex == 4 || arrayindex == 5 ? 255 : 0);
  needed->Frame = goalframe;
  current->Frame = 0;
}

void handleConnection()
{
  if (server.arg("ssid")== "" || server.arg("password")== "")
    server.send(200, "text/plain", "nope");
   else
   {
      server.send(200, "text/plain", "ok");
            
      File wififile = SPIFFS.open(wififilepath, "w");
      wififile.println(server.arg("ssid"));
      wififile.println(server.arg("password"));
      wififile.close();
      
      connection();
   }
}

void handleColor()
{
  if (server.arg("red")== "" || server.arg("green")== "" || server.arg("blue")== "")
    server.send(200, "text/plain", "nope");
  else
  {
    server.send(200, "text/plain", "ok");
    
    start->ReplaceColor(*current);
    needed->ChangeColor(server.arg("red").toInt(),
          server.arg("green").toInt(),
          server.arg("blue").toInt(),
          (server.arg("int")!= "" ? server.arg("int").toInt() : -1));
    needed->Frame = 80;
    current->Frame = 0;

    type = 0;
  }
}

void handleRandom()
{
  server.send(200, "text/plain", "ok");
  NewRandomize(80);
  type = 1;
}

void handleAllColors()
{
  server.send(200, "text/plain", "ok");
  
  arrayindex = 0;
  AllColorsNext(80);
  type = 2;
}

void handleIntensity()
{
  if (server.arg("int")== "")
    server.send(200, "text/plain", "nope");
  else
  {
    server.send(200, "text/plain", "ok");
    int intensity = server.arg("int").toInt() > 100 ? 100 : (server.arg("int").toInt() < 0 ? 0 : server.arg("int").toInt());

    start->ReplaceColor(*current);
    current->Frame = 0;
    needed->Frame = 80;
    needed->Intensity = intensity;
  }
}

void handleSpeed()
{
  if (server.arg("speed")== "")
    server.send(200, "text/plain", "nope");
  else
  {
    server.send(200, "text/plain", "ok");
    speed = server.arg("speed").toInt() <= 0 ? 1 : server.arg("speed").toInt();
  }
}

void handleSoundReco()
{
  if (server.arg("reco")== "")
    server.send(200, "text/plain", "nope");
  else
  {
    server.send(200, "text/plain", "ok");
    soundRecognition = server.arg("reco").toInt() == 1;

    curPeak = 0;
    topPeak = 0;
    stepPeak = 0;
    stepunitPeak = 0;
    currentMillis = millis();
    for(int i = 0; i < 10; i++) maxPeaks[i] = 0;
  }
}

void handleInfos()
{
  server.send(200, "text/plain", String("{ \"intensity\" : \"")+String(needed->Intensity)+String("\", \"speed\" : \"")+String(speed)+String("\", \"name\" : \"")+name+String("\", \"soundReco\" : \"")+String(soundRecognition)+String("\" }"));
}

void handleName()
{
  if (server.arg("name")== "")
    server.send(200, "text/plain", "nope");
  else
  {
    server.send(200, "text/plain", "ok");
    name = server.arg("name");

    File conffile = SPIFFS.open(conffilepath, "w");
    conffile.println(name);
    conffile.close();
  }
}

