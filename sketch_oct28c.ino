#include <WiFi.h>
#include <WebServer.h>
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
WebServer server(80);

// Variables de configuration sauvegardées
String ssid_saved, password_saved, ntpServer_saved;
int plage1_debutH, plage1_debutM, plage1_finH, plage1_finM;
int plage2_debutH, plage2_debutM, plage2_finH, plage2_finM;

// ---------------- Variables LED non bloquantes ----------------
unsigned long previousBlinkMillis = 0;
bool blinkState = false;
unsigned long currentMillis = 0;

// ---------------- FONCTIONS UTILITAIRES ----------------
bool estDansPlage(int h, int m) {
  int minutes = h * 60 + m;
  int debut1 = plage1_debutH * 60 + plage1_debutM;
  int fin1   = plage1_finH * 60 + plage1_finM;
  int debut2 = plage2_debutH * 60 + plage2_debutM;
  int fin2   = plage2_finH * 60 + plage2_finM;

  bool dans1 = (debut1 < fin1) ? (minutes >= debut1 && minutes < fin1)
                               : (minutes >= debut1 || minutes < fin1);
  bool dans2 = (debut2 < fin2) ? (minutes >= debut2 && minutes < fin2)
                               : (minutes >= debut2 || minutes < fin2);

  bool resultat = (dans1 || dans2);
  Serial.printf("   ⏱ %02d:%02d est dans la plage (%02d:%02d/%02d:%02d ou %02d:%02d/%02d:%02d) → %s\n", h, m, plage1_debutH, plage1_debutM, plage1_finH, plage1_finM, plage2_debutH, plage2_debutM, plage2_finH, plage2_finM, resultat ? "OUI 🟢" : "NON 🔴");
  return resultat;
}

int tempsAvantChangement(int h, int m) {
  int minutes = h * 60 + m;

  int debuts[2] = { plage1_debutH * 60 + plage1_debutM,
                    plage2_debutH * 60 + plage2_debutM };
  int fins[2]   = { plage1_finH * 60 + plage1_finM,
                    plage2_finH * 60 + plage2_finM };

  // Liste des points où l’état change (début ou fin)
  int points[4] = { debuts[0], fins[0], debuts[1], fins[1] };

  // On cherche le prochain changement en minutes
  int prochain = 24 * 60; // valeur max = 1 jour
  bool found = false;
  for (int i = 0; i < 4; i++) {
    int diff = points[i] - minutes;
    if (diff <= 0) diff += 24 * 60; // si déjà passé aujourd'hui → demain
    if (diff < prochain) {
      prochain = diff;
      found = true;
    }
  }

  if (!found) prochain = 9999;
  Serial.printf("   ⏱ temps avant changement: %d min restantes\n", prochain);
  return prochain;
}

void wifiStatusLED(bool wifiConnected, bool timeSynced, bool wifiAp) {
  if (wifiAp) {
    digitalWrite(LED_VERTE, LOW);
    digitalWrite(LED_ROUGE, LOW);
    ledcWrite(pwmChannel, 150);
    Serial.println("💡 LED WiFi : clignote lentement (en mode AP)");
    delay(200);
    ledcWrite(pwmChannel, 10);
  } else if (!wifiConnected) {
    digitalWrite(LED_VERTE, LOW);
    digitalWrite(LED_ROUGE, LOW);
    ledcWrite(pwmChannel, 128);
    Serial.println("💡 LED WiFi : clignote lentement (non connecté)");
    delay(200);
    ledcWrite(pwmChannel, 0);
  } else if (wifiConnected && !timeSynced) {
    digitalWrite(LED_VERTE, LOW);
    digitalWrite(LED_ROUGE, LOW);
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
  String page = "";
  page += "<!DOCTYPE HTML><html><head><title>Indication Heures Pleines-Creuses</title>";
  page += "<meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'>";

  // --- Style moderne & responsive ---
  page += "<style>";
  page += "body {font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f4f4f4; color: #333;}";
  page += "h1, h2{text-align: center;}";
  page += "h4 {margin-bottom: 5px;}";
  page += "form {max-width: 420px; margin: 20px auto; background: #fff; padding: 20px; border-radius: 12px; box-shadow: 0 2px 6px rgba(0,0,0,0.1);}";
  page += "label {display: block; margin-top: 10px; font-weight: bold;}";
  page += "input, select {width: 100%; padding: 8px; margin-top: 5px; border: 1px solid #ccc; border-radius: 8px; font-size: 16px; box-sizing: border-box;}";
  page += "input[type='number'] {width: 48%; display: inline-block;}";
  page += "input[type='submit'] {background-color: #0078d7; color: white; border: none; cursor: pointer; margin-top: 15px; font-size: 16px; border-radius: 8px; padding: 10px;}";
  page += "input[type='submit']:hover {background-color: #005fa3;}";
  page += "@media (max-width: 500px) {form {padding: 15px;} input[type='number'] {width: 15%;}}";
  page += "@media (prefers-color-scheme: dark) {body {background: #121212; color: #eee;} form {background: #1e1e1e; box-shadow: 0 2px 6px rgba(255,255,255,0.1);} input, select {background:#2a2a2a; color:#eee; border:1px solid #555;} input[type='submit'] {background:#2196f3;} input[type='submit']:hover {background:#1976d2;}}";
  page += "</style>";

  page += "</head><body>";

  page += "<h1>Configuration</h1>";
  page += "<form action='/save' method='POST'>";

  // --- Sélection du WiFi ---
  page += "<label for='ssidSelect'>SSID disponible :</label>";
  page += "<select id='ssidSelect'></select>";

  page += "<label for='ssid'>SSID choisi :</label>";
  page += "<input id='ssid' name='ssid' readonly>";

  page += "<label for='password'>Mot de passe :</label>";
  page += "<input name='password' id='password' type='password'>";

  page += "<label for='ntp'>Serveur NTP :</label>";
  page += "<input name='ntp' id='ntp' value='" + ntpServer_saved + "'>";
  page += "<br>";

  page += "<h2>Choix des heures creuses</h2>";

  page += "<h4>Plage 1</h4>";
  page += "Début : <input type='number' name='p1dH' min='0' max='23' value='" + String(plage1_debutH) + "'> h ";
  page += "<input type='number' name='p1dM' min='0' max='59' value='" + String(plage1_debutM) + "'> m<br>";
  page += "Fin : <input type='number' name='p1fH' min='0' max='23' value='" + String(plage1_finH) + "'> h ";
  page += "<input type='number' name='p1fM' min='0' max='59' value='" + String(plage1_finM) + "'> m<br>";

  page += "<h4>Plage 2</h4>";
  page += "Début : <input type='number' name='p2dH' min='0' max='23' value='" + String(plage2_debutH) + "'> h ";
  page += "<input type='number' name='p2dM' min='0' max='59' value='" + String(plage2_debutM) + "'> m<br>";
  page += "Fin : <input type='number' name='p2fH' min='0' max='23' value='" + String(plage2_finH) + "'> h ";
  page += "<input type='number' name='p2fM' min='0' max='59' value='" + String(plage2_finM) + "'> m<br><br>";

  page += "<input type='submit' value='Sauvegarder'>";
  page += "</form>";

  // --- Script WiFi dynamique ---
  page += "<script>";
  page += "document.addEventListener('DOMContentLoaded', function() {";
  page += "  const sel = document.getElementById('ssidSelect');";
  page += "  const ssidInput = document.getElementById('ssid');";
  page += "  let userSelectedSSID = null;";
  page += "";
  page += "  function setOptions(list){";
  page += "    sel.innerHTML='';";
  page += "    list.forEach(s=>{";
  page += "      const opt=document.createElement('option');";
  page += "      opt.value=s; opt.textContent=s;";
  page += "      sel.appendChild(opt);";
  page += "    });";
  page += "  }";
  page += "";
  page += "  async function fetchScan(){";
  page += "    try {";
  page += "      const resp = await fetch('/scan', {cache:'no-store'});";
  page += "      try {";
  page += "        const data = await resp.json();";
  page += "        if(Array.isArray(data)) return data.map(String);";
  page += "        throw new Error('JSON invalide');";
  page += "      } catch(eJson){";
  page += "        const txt = await resp.text();";
  page += "        const parts = txt.split(/\\r?\\n|,/).map(s=>s.trim()).filter(Boolean);";
  page += "        return parts;";
  page += "      }";
  page += "    } catch(e){ console.error('Erreur fetch /scan:', e); return null; }";
  page += "  }";
  page += "";
  page += "  async function scanWiFi(){";
  page += "    const list = await fetchScan();";
  page += "    if(!list){ sel.innerHTML='<option>(Erreur scan)</option>'; if(ssidInput) ssidInput.value=''; return; }";
  page += "    const previous = sel.value;";
  page += "    setOptions(list);";
  page += "    if(userSelectedSSID && list.includes(userSelectedSSID)) sel.value=userSelectedSSID;";
  page += "    else if(previous && list.includes(previous)) sel.value=previous;";
  page += "    else if(list.length>0) sel.selectedIndex=0;";
  page += "    if(ssidInput) ssidInput.value=sel.value||'';";
  page += "  }";
  page += "";
  page += "  sel.addEventListener('change', ()=>{";
  page += "    userSelectedSSID = sel.value;";
  page += "    if(ssidInput) ssidInput.value = userSelectedSSID;";
  page += "  });";
  page += "";
  page += "  scanWiFi();";
  page += "  setInterval(scanWiFi, 10000);";
  page += "});";
  page += "</script>";
  page += "</body></html>";
  server.send(200, "text/html", page);
}

void handleScan() {
  Serial.println("🌐 Scan des réseaux WiFi...");
  // Lance le scan (bloquant, renvoie nombre de réseaux)
  int n = WiFi.scanNetworks();
  // Commence le JSON array
  String ssids = "[";
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    ssid.replace("\"", "\\\""); // échappe les guillemets
    ssids += "\"" + ssid + "\"";
    if (i < n - 1) ssids += ","; 
    Serial.printf("   📶 Réseau détecté : %s (%ddBm)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }
  ssids += "]";
  // Renvoie JSON au client
  server.sendHeader("Content-Type", "application/json");
  server.send(200, "application/json", ssids);
  // Nettoie les anciens résultats pour le prochain scan
  WiFi.scanDelete();
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
  server.send(200, "text/html; charset=utf-8", "<h3>✅ Configuration sauvegardée ! Redémarrage...</h3>");
  delay(2000);
  ESP.restart();
}

// ---------------- FONCTIONS RÉSEAU ----------------
bool connectSavedWiFi() {
  ssid_saved = preferences.getString("ssid", "");
  password_saved = preferences.getString("password", "");
  if (ssid_saved == "") return false;

  Serial.printf("🌐 Connexion au WiFi : %s\n", ssid_saved.c_str());
   // --- Récupération et création du nom d’hôte ---
  String mac = WiFi.macAddress();           // ex: "24:6F:28:BB:2E:E8"
  mac.replace(":", "");
  String macSuffix = mac.substring(mac.length() - 6);
  String hostname = "HeuresCreuses_" + macSuffix;

  // --- Configuration WiFi ---
  WiFi.mode(WIFI_STA);                      // Mode station (client)
  WiFi.setHostname(hostname.c_str());       // ⚠️ Avant WiFi.begin() sur ESP8266
  
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
  Serial.println("📡 Mode Point d'accès activé : SSID = HeuresCreuses");
  WiFi.softAP("HeuresCreuses");
  Serial.print("   IP d'accès : http://");
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
  digitalWrite(LED_WIFI, HIGH); // car active sur LOW

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
  bool wifiAp = (WiFi.getMode() & WIFI_AP) != 0;

  wifiStatusLED(wifiConnected, timeSynced, wifiAp);

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

  if (wifiAp) {
    Serial.println("⚠️ En mode AP");
    delay(1000);
    return;
  }

  int h = timeinfo.tm_hour;
  int m = timeinfo.tm_min;
  int s = timeinfo.tm_sec;

  bool active = estDansPlage(h, m);
  int minutesRestantes = tempsAvantChangement(h, m);


  // 🔹 Clignotement alterné rouge/vert
  int blinkDelay = 500; // vitesse du clignotement
  unsigned long currentMillis = millis();

  if (minutesRestantes <= 5) {
    float x = (float)minutesRestantes / 5.0;
    blinkDelay = 100 + (int)(400 * (x * x));
  }

  // --- Gestion des LEDs ---
  if (currentMillis - previousBlinkMillis >= blinkDelay) {
    previousBlinkMillis = currentMillis;
    blinkState = !blinkState;

    if (active) {
      // 🔹 LED verte fixe, LED rouge clignote
      digitalWrite(LED_VERTE, HIGH);
      digitalWrite(LED_ROUGE, blinkState ? HIGH : LOW);
      Serial.printf("🟢 Verte ON / 🔴 Rouge Clignote - Delay %d ms\n", blinkDelay);
    } else {
      // 🔹 LED rouge fixe, LED verte clignote
      digitalWrite(LED_ROUGE, HIGH);
      digitalWrite(LED_VERTE, blinkState ? HIGH : LOW);
      Serial.printf("🔴 Rouge ON / 🟢 Verte Clignote - Delay %d ms\n", blinkDelay);
    }
  }
}
