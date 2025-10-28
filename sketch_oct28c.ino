#include <WiFi.h>
#include <ESP32WebServer.h>
#include <Preferences.h>
#include <time.h>

// ---------------- CONFIGURATION ----------------
#define LED_VERTE 6
#define LED_ROUGE 7
#define LED_WIFI LED_BUILTIN  // LED interne inversée (LOW = allumée)

const int pwmChannel = 0;
const int pwmFreq = 5000;
const int pwmResolution = 8;

Preferences preferences;
ESP32WebServer server(80);

// Variables de configuration sauvegardées
String ssid_saved, password_saved, ntpServer_saved;
int plage1_debutH, plage1_debutM, plage1_finH, plage1_finM;
int plage2_debutH, plage2_debutM, plage2_finH, plage2_finM;

// ---------------- FONCTIONS UTILITAIRES ----------------
bool estDansPlage(int h, int m) {
  int minutes = h * 60 + m;
  int debut1 = plage1_debutH * 60 + plage1_debutM;
  int fin1   = plage1_finH * 60 + plage1_finM;
  int debut2 = plage2_debutH * 60 + plage2_debutM;
  int fin2   = plage2_finH * 60 + plage2_finM;

  bool resultat = ((minutes >= debut1 && minutes <= fin1) ||
                   (minutes >= debut2 && minutes <= fin2));

  Serial.printf("   ⏱ estDansPlage(): %02d:%02d → %s\n", h, m, resultat ? "OUI" : "NON");
  return resultat;
}

int tempsAvantChangement(int h, int m) {
  int minutes = h * 60 + m;
  int points[] = {
    plage1_debutH * 60 + plage1_debutM,
    plage1_finH * 60 + plage1_finM,
    plage2_debutH * 60 + plage2_debutM,
    plage2_finH * 60 + plage2_finM
  };
  int prochain = 9999;
  for (int i = 0; i < 4; i++) {
    if (minutes < points[i]) {
      prochain = points[i] - minutes;
      break;
    }
  }
  Serial.printf("   ⏱ tempsAvantChangement(): %d min restantes\n", prochain);
  return prochain;
}

void wifiStatusLED(bool wifiConnected, bool timeSynced) {
  if (!wifiConnected) {
    ledcWrite(pwmChannel, 128);
    Serial.println("💡 LED WiFi : clignote lentement (non connecté)");
    delay(200);
    ledcWrite(pwmChannel, 0);
  } else if (wifiConnected && !timeSynced) {
    ledcWrite(pwmChannel, 64);
    Serial.println("💡 LED WiFi : clignote rapide (connecté, NTP non sync)");
    delay(100);
    ledcWrite(pwmChannel, 0);
  } else {
    ledcWrite(pwmChannel, 0);
  }
}

// ---------------- INTERFACE WEB ----------------
void handleRoot() {
  Serial.println("🌐 Requête / : page de configuration envoyée");
  String page = "<html><head><meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<h2>Configuration WiFi / NTP / Heures creuses</h2>";
  page += "<form action='/save' method='POST'>";

  page += "SSID : <select id='ssid' name='ssid'></select><br>";
  page += "Mot de passe : <input name='password' type='password'><br>";
  page += "Serveur NTP : <input name='ntp' value='" + ntpServer_saved + "'><br>";

  page += "<h3>Plage 1</h3>";
  page += "Début : <input type='number' name='p1dH' min='0' max='23' value='" + String(plage1_debutH) + "'>h ";
  page += "<input type='number' name='p1dM' min='0' max='59' value='" + String(plage1_debutM) + "'>m<br>";
  page += "Fin : <input type='number' name='p1fH' min='0' max='23' value='" + String(plage1_finH) + "'>h ";
  page += "<input type='number' name='p1fM' min='0' max='59' value='" + String(plage1_finM) + "'>m<br>";

  page += "<h3>Plage 2</h3>";
  page += "Début : <input type='number' name='p2dH' min='0' max='23' value='" + String(plage2_debutH) + "'>h ";
  page += "<input type='number' name='p2dM' min='0' max='59' value='" + String(plage2_debutM) + "'>m<br>";
  page += "Fin : <input type='number' name='p2fH' min='0' max='23' value='" + String(plage2_finH) + "'>h ";
  page += "<input type='number' name='p2fM' min='0' max='59' value='" + String(plage2_finM) + "'>m<br>";

  page += "<input type='submit' value='Sauvegarder'>";
  page += "</form>";

  page += "<script>";
  page += "function scanWiFi(){fetch('/scan').then(r=>r.json()).then(data=>{";
  page += "let sel=document.getElementById('ssid'); sel.innerHTML='';";
  page += "data.forEach(s=>{sel.innerHTML+='<option value=\"'+s+'\">'+s+'</option>';});";
  page += "});}";
  page += "scanWiFi(); setInterval(scanWiFi,5000);";
  page += "</script></body></html>";

  server.send(200, "text/html", page);
}

void handleScan() {
  Serial.println("🌐 Scan des réseaux WiFi...");
  int n = WiFi.scanNetworks();
  String ssids = "[";
  for (int i = 0; i < n; i++) {
    ssids += "\"" + WiFi.SSID(i) + "\"";
    if (i < n - 1) ssids += ",";
    Serial.printf("   📶 Réseau détecté : %s (%ddBm)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }
  ssids += "]";
  server.send(200, "application/json", ssids);
}

void handleSave() {
  Serial.println("💾 Sauvegarde de la configuration via portail web...");
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

  Serial.println("✅ Configuration sauvegardée en mémoire !");
  server.send(200, "text/html", "<h3>✅ Configuration sauvegardée ! Redémarrage...</h3>");
  delay(2000);
  ESP.restart();
}

// ---------------- FONCTIONS RÉSEAU ----------------
bool connectSavedWiFi() {
  ssid_saved = preferences.getString("ssid", "");
  password_saved = preferences.getString("password", "");
  if (ssid_saved == "") return false;

  Serial.printf("🌐 Connexion au WiFi : %s\n", ssid_saved.c_str());
  WiFi.begin(ssid_saved.c_str(), password_saved.c_str());
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connecté !");
    Serial.print("   IP locale : ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("\n❌ Échec connexion WiFi");
  return false;
}

void startAP() {
  Serial.println("📡 Mode Point d'accès activé : SSID = ESP32_Config");
  WiFi.softAP("ESP32_Config");
  Serial.print("   IP d'accès : ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/save", handleSave);
  server.begin();

  Serial.println("🌍 Serveur web prêt !");
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_VERTE, OUTPUT);
  pinMode(LED_ROUGE, OUTPUT);
  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(LED_VERTE, LOW);
  digitalWrite(LED_ROUGE, LOW);
  digitalWrite(LED_WIFI, HIGH);

  ledcAttachChannel(LED_WIFI, pwmFreq, pwmResolution, pwmChannel);
  preferences.begin("esp_config", false);

  plage1_debutH = preferences.getInt("p1dH", 12);
  plage1_debutM = preferences.getInt("p1dM", 55);
  plage1_finH = preferences.getInt("p1fH", 15);
  plage1_finM = preferences.getInt("p1fM", 53);
  plage2_debutH = preferences.getInt("p2dH", 1);
  plage2_debutM = preferences.getInt("p2dM", 55);
  plage2_finH = preferences.getInt("p2fH", 6);
  plage2_finM = preferences.getInt("p2fM", 53);
  ntpServer_saved = preferences.getString("ntp", "pool.ntp.org");

  Serial.println("\n============================");
  Serial.println("   🚀 ESP32-C3 SUPERMINI   ");
  Serial.println("   Heures creuses + WiFi   ");
  Serial.println("============================\n");

  if (!connectSavedWiFi()) {
    startAP();
    while (true) server.handleClient();
  } else {
    Serial.printf("⏳ Synchronisation NTP sur %s...\n", ntpServer_saved.c_str());
    configTime(0, 0, ntpServer_saved.c_str());
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
  }
}

// ---------------- LOOP ----------------
void loop() {
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  struct tm timeinfo;
  bool timeSynced = getLocalTime(&timeinfo);

  wifiStatusLED(wifiConnected, timeSynced);

  if (!wifiConnected) {
    Serial.println("⚠️ WiFi déconnecté — reconnexion...");
    WiFi.reconnect();
    delay(1000);
    return;
  }

  if (!timeSynced) {
    Serial.println("⚠️ Connecté mais NTP non synchronisé");
    delay(1000);
    return;
  }

  int h = timeinfo.tm_hour;
  int m = timeinfo.tm_min;
  int s = timeinfo.tm_sec;

  bool active = estDansPlage(h, m);
  int minutesRestantes = tempsAvantChangement(h, m);

  Serial.printf("[%02d:%02d:%02d] Plage active=%d | Minutes avant changement=%d\n", h, m, s, active, minutesRestantes);

  if (minutesRestantes <= 5) {
    int delai = map(minutesRestantes, 0, 5, 100, 500);
    Serial.printf("🟠 Approche changement (%d min) → Clignotement orange t=%dms\n", minutesRestantes, delai);
    digitalWrite(LED_VERTE, HIGH); digitalWrite(LED_ROUGE, HIGH);
    delay(delai);
    digitalWrite(LED_VERTE, LOW); digitalWrite(LED_ROUGE, LOW);
    delay(delai);
  } else {
    if (active) {
      Serial.println("🟢 Période active → LED verte allumée");
      digitalWrite(LED_VERTE, HIGH);
      digitalWrite(LED_ROUGE, LOW);
    } else {
      Serial.println("🔴 Période inactive → LED rouge allumée");
      digitalWrite(LED_VERTE, LOW);
      digitalWrite(LED_ROUGE, HIGH);
    }
    delay(1000);
  }
}
