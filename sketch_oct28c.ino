#include <WiFi.h>
#include "time.h"

#define LED_VERTE 6
#define LED_ROUGE 7
#define LED_WIFI LED_BUILTIN  // Led interne inversée active à LOW

const char* ssid     = "NEO";
const char* password = "Nicolas76";
const char* ntpServer = "pool.ntp.org";

const int pwmChannel = 0;
const int pwmFreq = 5000;
const int pwmResolution = 8;

bool estDansPlage(int h, int m) {
  int minutesTotales = h * 60 + m;
  int debut1 = 12 * 60 + 55;
  int fin1   = 15 * 60 + 53;
  int debut2 = 1 * 60 + 55;
  int fin2   = 6 * 60 + 53;
  return ((minutesTotales >= debut1 && minutesTotales <= fin1) ||
          (minutesTotales >= debut2 && minutesTotales <= fin2));
}

int tempsAvantChangement(int h, int m) {
  int minutesTotales = h * 60 + m;
  int changements[] = {1155, 653, 1255, 1553};
  int prochain = 9999;
  for (int i = 0; i < 4; i++) {
    if (minutesTotales < changements[i]) {
      prochain = changements[i] - minutesTotales;
      break;
    }
  }
  return prochain;
}

void wifiStatusLED(bool wifiConnected, bool timeSynced) {
  if (!wifiConnected) {
    for (int i = 0; i < 256; i++) {
      ledcWriteChannel(pwmChannel, 255 - i);
      delay(10);
    }
    for (int i = 255; i >= 0; i--) {
      ledcWriteChannel(pwmChannel, 255 - i);
      delay(10);
    }
  } 
  else if (wifiConnected && !timeSynced) {
    for (int i = 0; i < 256; i++) {
      ledcWriteChannel(pwmChannel, 255 - i);
      delay(1);
    }
    for (int i = 255; i >= 0; i--) {
      ledcWriteChannel(pwmChannel, 255 - i);
      delay(1);
    }
  } 
  else {
    ledcWriteChannel(pwmChannel, 0);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_VERTE, OUTPUT);
  pinMode(LED_ROUGE, OUTPUT);
  pinMode(LED_WIFI, OUTPUT);

  digitalWrite(LED_VERTE, LOW);
  digitalWrite(LED_ROUGE, LOW);
  digitalWrite(LED_WIFI, HIGH);

  // Configure PWM avec ledcAttachChannel (API moderne)
  ledcAttachChannel(LED_WIFI, pwmFreq, pwmResolution, pwmChannel);

  Serial.println("\n============================");
  Serial.println("   ESP32-C3 SuperMini LED   ");
  Serial.println("   Gestion horaire + WiFi   ");
  Serial.println("============================\n");

  Serial.print("Connexion WiFi à ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    wifiStatusLED(false, false);
    Serial.print(".");
  }

  Serial.println("\n✅ Connecté au WiFi !");
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());

  configTime(0, 0, ntpServer);
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  Serial.println("⏳ Synchronisation NTP...");
  delay(2000);
}

void loop() {
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  struct tm timeinfo;
  bool timeSynced = getLocalTime(&timeinfo);

  wifiStatusLED(wifiConnected, timeSynced);

  if (!wifiConnected) {
    Serial.println("⚠️ WiFi non connecté — tentative de reconnexion...");
    WiFi.reconnect();
    delay(1000);
    return;
  }

  if (!timeSynced) {
    Serial.println("⚠️ Connecté au WiFi, mais échec NTP !");
    delay(1000);
    return;
  }

  int heure = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;
  int seconde = timeinfo.tm_sec;

  bool plageVerte = estDansPlage(heure, minute);
  int minutesRestantes = tempsAvantChangement(heure, minute);

  Serial.printf("[%02d:%02d:%02d] ", heure, minute, seconde);

  if (minutesRestantes <= 5) {
    int delai = map(minutesRestantes, 0, 5, 100, 500);
    Serial.printf("🟠 Approche changement (%d min restantes) → Clignotement orange (t=%d ms)\n", minutesRestantes, delai);
    digitalWrite(LED_VERTE, HIGH);
    digitalWrite(LED_ROUGE, HIGH);
    delay(delai);
    digitalWrite(LED_VERTE, LOW);
    digitalWrite(LED_ROUGE, LOW);
    delay(delai);
  } else {
    if (plageVerte) {
      Serial.println("🟢 Période active → LED VERTE allumée");
      digitalWrite(LED_VERTE, HIGH);
      digitalWrite(LED_ROUGE, LOW);
    } else {
      Serial.println("🔴 Période inactive → LED ROUGE allumée");
      digitalWrite(LED_VERTE, LOW);
      digitalWrite(LED_ROUGE, HIGH);
    }
    delay(1000);
  }
}
