#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "time.h"

#define LED_VERTE 6
#define LED_ROUGE 7
#define LED_WIFI LED_BUILTIN

const int pwmChannel = 0;
const int pwmFreq = 5000;
const int pwmResolution = 8;

WebServer server(80);
Preferences preferences;

// Paramètres sauvegardés
String ssid_saved;
String password_saved;
String ntpServer_saved = "pool.ntp.org";

int plage1_debutH, plage1_debutM, plage1_finH, plage1_finM;
int plage2_debutH, plage2_debutM, plage2_finH, plage2_finM;

// ====================== FONCTIONS LED ======================
void wifiStatusLED(bool wifiConnected, bool timeSynced) {
  if (!wifiConnected) {
    Serial.println("LED WIFI: WiFi NON connecté - clignotement lent");
    for (int i = 0; i < 256; i++) { ledcWriteChannel(pwmChannel, 255 - i); delay(10); }
    for (int i = 255; i >= 0; i--) { ledcWriteChannel(pwmChannel, 255 - i); delay(10); }
  } else if (wifiConnected && !timeSynced) {
    Serial.println("LED WIFI: WiFi connecté mais NTP NON synchronisé - clignotement rapide");
    for (int i = 0; i < 256; i++) { ledcWriteChannel(pwmChannel, 255 - i); delay(1); }
    for (int i = 255; i >= 0; i--) { ledcWriteChannel(pwmChannel, 255 - i); delay(1); }
  } else {
    Serial.println("LED WIFI: WiFi et NTP OK - LED éteinte");
    ledcWriteChannel(pwmChannel, 0);
  }
}

// ====================== FONCTIONS HORAIRES ======================
bool estDansPlage(int h, int m) {
  int minutesTotales = h * 60 + m;
  int debut1 = plage1_debutH*60 + plage1_debutM;
  int fin1   = plage1_finH*60 + plage1_finM;
  int debut2 = plage2_debutH*60 + plage2_debutM;
  int fin2   = plage2_finH*60 + plage2_finM;
  return ((minutesTotales >= debut1 && minutesTotales <= fin1) ||
          (minutesTotales >= debut2 && minutesTotales <= fin2));
}

int tempsAvantChangement(int h, int m) {
  int minutesTotales = h * 60 + m;
  int changements[] = {
    plage1_debutH*60 + plage1_debutM,
    plage1_finH*60 + plage1_finM,
    plage2_debutH*60 + plage2_debutM,
    plage2_finH*60 + plage2_finM
  };
  int prochain = 9999;
  for (int i = 0; i < 4; i++) {
    if (minutesTotales < changements[i]) {
      prochain = changements[i] - minutesTotales;
      break;
    }
  }
  return prochain;
}

// ====================== PORTAIL WEB ======================
void startAP() {
  Serial.println("🔌 Pas de WiFi, démarrage du point d'accès...");
  WiFi.softAP("ESP_Config");
  Serial.print("IP de l'AP: "); Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, [](){
    int n = WiFi.scanNetworks();
    String options = "";
    for (int i = 0; i < n; ++i) {
      options += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
    }

    String page = "<html><body>";
    page += "<h2>Configuration WiFi et NTP</h2>";
    page += "<form action='/save' method='POST'>";
    page += "SSID: <select name='ssid'>" + options + "</select><br>";
    page += "Mot de passe: <input name='password' type='password'><br>";
    page += "NTP Server: <input name='ntp' value='" + ntpServer_saved + "'><br>";

    page += "<h3>Plage 1</h3>";
    page += "Début: <input type='number' name='p1dH' min='0' max='23' placeholder='HH' style='width:50px;'> : ";
    page += "<input type='number' name='p1dM' min='0' max='59' placeholder='MM' style='width:50px;'><br>";
    page += "Fin: <input type='number' name='p1fH' min='0' max='23' placeholder='HH' style='width:50px;'> : ";
    page += "<input type='number' name='p1fM' min='0' max='59' placeholder='MM' style='width:50px;'><br>";

    page += "<h3>Plage 2</h3>";
    page += "Début: <input type='number' name='p2dH' min='0' max='23' placeholder='HH' style='width:50px;'> : ";
    page += "<input type='number' name='p2dM' min='0' max='59' placeholder='MM' style='width:50px;'><br>";
    page += "Fin: <input type='number' name='p2fH' min='0' max='23' placeholder='HH' style='width:50px;'> : ";
    page += "<input type='number' name='p2fM' min='0' max='59' placeholder='MM' style='width:50px;'><br>";

    page += "<input type='submit' value='Sauvegarder'>";
    page += "</form></body></html>";
    server.send(200, "text/html", page);
  });

  server.on("/save", HTTP_POST, [](){
    ssid_saved = server.arg("ssid");
    password_saved = server.arg("password");
    ntpServer_saved = server.arg("ntp");

    plage1_debutH = server.arg("p1dH").toInt();
    plage1_debutM = server.arg("p1dM").toInt();
    plage1_finH = server.arg("p1fH").toInt();
    plage1_finM = server.arg("p1fM").toInt();
    plage2_debutH = server.arg("p2dH").toInt();
    plage2_debutM = server.arg("p2dM").toInt();
    plage2_finH = server.arg("p2fH").toInt();
    plage2_finM = server.arg("p2fM").toInt();

    Serial.println("💾 Configuration sauvegardée :");
    Serial.printf("SSID: %s, NTP: %s\n", ssid_saved.c_str(), ntpServer_saved.c_str());
    Serial.printf("Plage1: %02d:%02d -> %02d:%02d\n", plage1_debutH, plage1_debutM, plage1_finH, plage1_finM);
    Serial.printf("Plage2: %02d:%02d -> %02d:%02d\n", plage2_debutH, plage2_debutM, plage2_finH, plage2_finM);

    preferences.putString("ssid", ssid_saved);
    preferences.putString("password", password_saved);
    preferences.putString("ntp", ntpServer_saved);
    preferences.putInt("p1dH", plage1_debutH);
    preferences.putInt("p1dM", plage1_debutM);
    preferences.putInt("p1fH", plage1_finH);
    preferences.putInt("p1fM", plage1_finM);
    preferences.putInt("p2dH", plage2_debutH);
    preferences.putInt("p2dM", plage2_debutM);
    preferences.putInt("p2fH", plage2_finH);
    preferences.putInt("p2fM", plage2_finM);

    server.send(200, "text/html", "<h2>✅ Sauvegardé ! Redémarrage...</h2>");
    delay(2000);
    ESP.restart();
  });

  server.begin();
}

// ====================== CONNEXION WI-FI ======================
bool connectSavedWiFi() {
  ssid_saved = preferences.getString("ssid", "");
  password_saved = preferences.getString("password", "");
  if (ssid_saved == "") return false;

  WiFi.begin(ssid_saved.c_str(), password_saved.c_str());
  Serial.print("Connexion au WiFi "); Serial.println(ssid_saved);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connecté au WiFi !");
    Serial.print("Adresse IP : "); Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\n❌ Échec connexion WiFi");
    return false;
  }
}

// ====================== SETUP ======================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_VERTE, OUTPUT);
  pinMode(LED_ROUGE, OUTPUT);
  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(LED_VERTE, LOW);
  digitalWrite(LED_ROUGE, LOW);
  digitalWrite(LED_WIFI, HIGH);

  // PWM LED_WIFI avec ledcAttachChannel
  ledcAttachChannel(LED_WIFI, pwmFreq, pwmResolution, pwmChannel);

  preferences.begin("esp_config", false);

  // Charger valeurs sauvegardées
  plage1_debutH = preferences.getInt("p1dH", 12);
  plage1_debutM = preferences.getInt("p1dM", 55);
  plage1_finH = preferences.getInt("p1fH", 15);
  plage1_finM = preferences.getInt("p1fM", 53);
  plage2_debutH = preferences.getInt("p2dH", 1);
  plage2_debutM = preferences.getInt("p2dM", 55);
  plage2_finH = preferences.getInt("p2fH", 6);
  plage2_finM = preferences.getInt("p2fM", 53);
  ntpServer_saved = preferences.getString("ntp", "pool.ntp.org");

  if (!connectSavedWiFi()) {
    startAP();
    while (true) server.handleClient();
  } else {
    Serial.println("⏳ Synchronisation NTP...");
    configTime(0, 0, ntpServer_saved.c_str());
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
  }
}

// ====================== LOOP ======================
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

  int h = timeinfo.tm_hour;
  int m = timeinfo.tm_min;
  int s = timeinfo.tm_sec;

  bool plageVerte = estDansPlage(h, m);
  int minutesRestantes = tempsAvantChangement(h, m);

  Serial.printf("[%02d:%02d:%02d] PlageVerte=%d, minutesRestantes=%d\n", h, m, s, plageVerte, minutesRestantes);

  if (minutesRestantes <= 5) {
    int delai = map(minutesRestantes, 0, 5, 100, 500);
    Serial.printf("🟠 Approche changement (%d min restantes) → Clignotement orange (t=%d ms)\n", minutesRestantes, delai);
    digitalWrite(LED_VERTE, HIGH); digitalWrite(LED_ROUGE, HIGH);
    delay(delai);
    digitalWrite(LED_VERTE, LOW); digitalWrite(LED_ROUGE, LOW);
    delay(delai);
  } else {
    if (plageVerte) { 
      Serial.println("🟢 Période active → LED VERTE allumée");
      digitalWrite(LED_VERTE, HIGH); 
      digitalWrite(LED_ROUGE, LOW); 
    }
    else { 
      Serial.println("🔴 Période inactive → LED ROUGE allumée");
      digitalWrite(LED_VERTE, LOW); 
      digitalWrite(LED_ROUGE, HIGH); 
    }
    delay(1000);
  }
}
