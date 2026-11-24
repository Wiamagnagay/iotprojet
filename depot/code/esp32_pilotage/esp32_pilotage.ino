// =====================================================================
//  ESP32 "NOEUD DE PILOTAGE" : LED + BOUTON POUSSOIR, via MQTT
//
//  Ce noeud ne decide rien : c'est Node-RED qui decide.
//   - les LED et le buzzer obeissent aux ordres recus en MQTT
//   - le bouton envoie son etat (appuye / relache) en MQTT
//
//  Topics MQTT :
//   recu   "seb/led/cmd"       : "on" / "off"  (bouton LED du dashboard)
//   recu   "seb/alarme/cmd"    : "ON" / "OFF"  (alarme decidee par Node-RED)
//   recu   "seb/led_verte/cmd" : "ON" / "OFF"  (pouls normal, decide par Node-RED)
//   envoye "seb/bouton"        : "appuye" / "relache" (a chaque changement)
//
//  Comportement :
//   - alarme active -> la LED clignote et le buzzer bipe en meme temps
//   - sinon         -> la LED suit le bouton du dashboard, buzzer muet
//   - LED verte     -> allumee quand le pouls est normal (et different de 0)
//
//  Cablage (a adapter a votre montage) :
//   - LED       : GPIO25 (D2) -> resistance 220 ohms -> patte longue de la LED,
//                 patte courte -> GND
//   - LED verte : GPIO27 (D4) -> resistance 220 ohms -> patte longue,
//                 patte courte -> GND
//   - Buzzer    : GPIO23 (MOSI) -> + du buzzer (ou broche S d'un module),
//                 - du buzzer -> GND (module 3 broches : + sur 3V3)
//   - Bouton    : GPIO26 (D3) -> bouton -> GND (resistance de pull-up interne)
//
//  Bibliotheque a installer : PubSubClient (Nick O'Leary)
// =====================================================================

#include <WiFi.h>
#include <PubSubClient.h>

// ---------- A MODIFIER ----------
const char *ssid        = "xxxx";
const char *password    = "xxxx";
const char *mqtt_server = "192.168.X.X";   // IP de la Raspberry

#define MQTT_CLIENT_ID   "ESP32_Pilotage_SEB"   // different sur chaque ESP32
#define TOPIC_LED_CMD    "seb/led/cmd"
#define TOPIC_ALARME_CMD "seb/alarme/cmd"
#define TOPIC_BOUTON     "seb/bouton"
#define TOPIC_LED_VERTE  "seb/led_verte/cmd"

// ---------- Broches ----------
#define LED_PIN        25
#define BUTTON_PIN     26
#define LED_VERTE_PIN  27
#define BUZZER_PIN     23   // MOSI

// Type de buzzer :
//  0 = buzzer ACTIF  (sonne tout seul quand on l'alimente, le plus courant)
//  1 = buzzer PASSIF (il faut lui envoyer une frequence)
//  -> si votre buzzer fait juste "clic" au lieu de sonner, mettez 1
#define BUZZER_PASSIF  0
#define FREQ_BUZZER    2000  // Hz (buzzer passif uniquement)

#define PERIODE_CLIGNOTEMENT 250   // ms

WiFiClient espClient;
PubSubClient client(espClient);

bool alarmeActive = false;   // ordre d'alarme recu de Node-RED
bool ledDemandee  = false;   // etat demande par le dashboard
bool ledClignote  = false;   // etat courant du clignotement
bool ledVerte     = false;   // pouls normal (ordre de Node-RED)
bool buzzerActif  = false;   // etat courant du buzzer

int etatBouton = HIGH;       // HIGH = relache (INPUT_PULLUP)

unsigned long dernierClignotement = 0;
unsigned long dernierEssaiMQTT    = 0;


// =====================================
// BUZZER
// =====================================

void commanderBuzzer(bool on) {

  if (on == buzzerActif) return;   // rien a changer
  buzzerActif = on;

#if BUZZER_PASSIF
  if (on) tone(BUZZER_PIN, FREQ_BUZZER);
  else    noTone(BUZZER_PIN);
#else
  digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
#endif
}


// =====================================
// LED + BUZZER
// =====================================

void mettreAJourSorties() {

  if (alarmeActive) {
    // Alarme : la LED clignote et le buzzer bipe en meme temps
    if (millis() - dernierClignotement >= PERIODE_CLIGNOTEMENT) {
      dernierClignotement = millis();
      ledClignote = !ledClignote;
      digitalWrite(LED_PIN, ledClignote ? HIGH : LOW);
      commanderBuzzer(ledClignote);
    }
  }
  else {
    // Pas d'alarme : la LED suit le dashboard, buzzer muet
    digitalWrite(LED_PIN, ledDemandee ? HIGH : LOW);
    commanderBuzzer(false);
  }

  // LED verte : pouls normal
  digitalWrite(LED_VERTE_PIN, ledVerte ? HIGH : LOW);
}


// =====================================
// BOUTON : envoie son etat a chaque changement (avec anti-rebond)
// =====================================

void publierBouton() {
  const char *etat = (etatBouton == LOW) ? "appuye" : "relache";
  client.publish(TOPIC_BOUTON, etat);
  Serial.printf("Bouton %s -> %s\n", etat, TOPIC_BOUTON);
}

void verifierBouton() {
  static unsigned long dernierChangement = 0;

  int lecture = digitalRead(BUTTON_PIN);

  if (lecture != etatBouton && millis() - dernierChangement > 50) {
    dernierChangement = millis();
    etatBouton = lecture;
    publierBouton();
  }
}


// =====================================
// MQTT
// =====================================

// Appelee a chaque message recu de Node-RED
void callback(char *topic, byte *message, unsigned int length) {

  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)message[i];
  }

  Serial.printf("Message recu sur %s : %s\n", topic, msg.c_str());

  String t = String(topic);

  if (t == TOPIC_LED_CMD) {
    if (msg == "on")  ledDemandee = true;
    if (msg == "off") ledDemandee = false;
  }
  else if (t == TOPIC_LED_VERTE) {
    if (msg == "ON")  ledVerte = true;
    if (msg == "OFF") ledVerte = false;
  }
  else if (t == TOPIC_ALARME_CMD) {
    if (msg == "ON") {
      alarmeActive = true;
      Serial.println("ALARME : frequence cardiaque anormale !");
    }
    if (msg == "OFF") {
      alarmeActive = false;
      Serial.println("Alarme arretee");
    }
  }
}

// Reconnexion sans bloquer (le bouton et la LED restent actifs)
void maintenirMQTT() {

  if (client.connected()) {
    client.loop();
    return;
  }

  if (dernierEssaiMQTT != 0 && millis() - dernierEssaiMQTT < 5000) return;
  dernierEssaiMQTT = millis();

  Serial.print("Connexion MQTT... ");
  if (client.connect(MQTT_CLIENT_ID)) {
    Serial.println("OK");
    client.subscribe(TOPIC_LED_CMD);
    client.subscribe(TOPIC_ALARME_CMD);
    client.subscribe(TOPIC_LED_VERTE);
    publierBouton();   // envoie l'etat actuel du bouton
  }
  else {
    Serial.printf("echec (rc=%d), nouvel essai dans 5 s\n", client.state());
  }
}

void setup_wifi() {
  Serial.print("Connexion a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("\nWiFi connecte, IP : ");
  Serial.println(WiFi.localIP());
}


// =====================================
// INITIALISATION
// =====================================

void setup() {

  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_VERTE_PIN, OUTPUT);
  digitalWrite(LED_VERTE_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  Serial.println("ESP32 Noeud de pilotage initialise");
}


// =====================================
// BOUCLE PRINCIPALE (rien ne bloque)
// =====================================

void loop() {
  maintenirMQTT();
  verifierBouton();
  mettreAJourSorties();
}
