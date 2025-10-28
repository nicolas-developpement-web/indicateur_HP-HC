#include <WiFi.h>
#include "time.h"

// === Broches ===
#define LED_VERTE 6
#define LED_ROUGE 7
#define LED_WIFI LED_BUILTIN  // LED intégrée (souvent GPIO8 ou GPIO2 sur ESP32-C3)

// === WiFi ===
const char* ssid     = "NEO";
const char* password = "Nicolas76";

// === Serveur NTP ===
const char* ntpServer = "pool.ntp.org";

// === Fonctions utilitaires ===
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
  int changements[] = {1155, 653, 1255, 1553}; // 1h55, 6h53, 12h55, 15h53
  int prochain = 9999;
  for (int i = 0; i < 4; i++) {
    if (minutesTotales < changements[i]) {
      prochain = changements[i] - minutesTotales;
      break;
    }
  }
  return prochain;
}

// === Indicateur LED intégrée ===
void wifiStatusLED(bool wifiConnected, bool timeSynced) {
  if (!wifiConnected) {
    // Clignotement lent : pas connecté au Wi-Fi
    digitalWrite(LED_WIFI, HIGH);
    delay(300);
    digitalWrite(LED_WIFI, LOW);
    delay(300);
  } 
  else if (wifiConnected && !timeSynced) {
    // Clignotement rapide : Wi-Fi OK mais NTP échoué
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_WIFI, HIGH);
      delay(100);
      digitalWrite(LED_WIFI, LOW);
      delay(100);
    }
  } 
  else {
    // Allumée fixe : tout va bien
    digitalWrite(LED_WIFI, HIGH);
  }
}

// === Setup ===
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_VERTE, OUTPUT);
  pinMode(LED_ROUGE, OUTPUT);
  pinMode(LED_WIFI, OUTPUT);

  digitalWrite(LED_VERTE, LOW);
  digitalWrite(LED_ROUGE, LOW);
  digitalWrite(LED_WIFI, LOW);

  Serial.println("\n============================");
  Serial.println("   ESP32-C3 SuperMini LED   ");
  Serial.println("   Gestion horaire + WiFi   ");
  Serial.println("============================\n");

  Serial.print("Connexion WiFi à ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // Clignote lentement pendant la connexion
  while (WiFi.status() != WL_CONNECTED) {
    wifiStatusLED(false, false);
    Serial.print(".");
  }

  Serial.println("\n✅ Connecté au WiFi !");
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());

  // Fuseau horaire Europe/Paris (changement été/hiver automatique)
  configTime(0, 0, ntpServer);
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  Serial.println("⏳ Synchronisation NTP...");
  delay(2000);
}

// === Boucle principale ===
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
    Serial.println("⚠️ Connecté au WiFi, mais échec de mise à jour NTP !");
    delay(1000);
    return;
  }

  int heure = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;
  int seconde = timeinfo.tm_sec;

  bool plageVerte = estDansPlage(heure, minute);
  int minutesRestantes = tempsAvantChangement(heure, minute);

  // Affichage console
  Serial.printf("[%02d:%02d:%02d] ", heure, minute, seconde);

  // Clignotement orange si changement proche
  if (minutesRestantes <= 5) {
    int delai = map(minutesRestantes, 0, 5, 100, 500);
    Serial.printf("🟠 Approche changement (%d min restantes) → Clignotement orange (t=%d ms)\n",
                  minutesRestantes, delai);
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
