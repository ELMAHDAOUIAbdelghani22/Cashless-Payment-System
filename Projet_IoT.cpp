#include <WiFi.h>  // Inclut la bibliothèque pour utiliser le WiFi
#include <HTTPClient.h>  // Inclut la bibliothèque pour effectuer des requêtes HTTP
#include <SPI.h>  // Inclut la bibliothèque pour utiliser le protocole SPI
#include <MFRC522.h>  // Inclut la bibliothèque pour utiliser le lecteur RFID MFRC522
#include <ThingSpeak.h>  // Inclut la bibliothèque pour communiquer avec ThingSpeak
#include <ArduinoJson.h>  // Inclut la bibliothèque pour manipuler les données JSON

// Définit les broches du lecteur MFRC522
#define SS_PIN 5  // Broche de sélection du périphérique
#define RST_PIN 4  // Broche de réinitialisation
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Crée une instance de l'objet MFRC522

// Informations d'identification du WiFi
const char* ssid = "Firdaous";  // Nom du réseau WiFi
const char* password = "20202020";  // Mot de passe du réseau WiFi

// Informations d'identification de ThingSpeak
unsigned long myChannelNumber = 2575207;  // Numéro de canal ThingSpeak
const char* apiKey = "DFVD412632QINSQQ";  // Clé API pour ThingSpeak

// Adresse du serveur
const char* server = "192.168.3.106:8080/projet_IOT1";  // Adresse du serveur

WiFiClient client;  // Crée un client WiFi

int readsuccess;  // Variable pour indiquer si la lecture a réussi
byte readcard[4];  // Tableau pour stocker l'UID de la carte
char str[32] = "";  // Buffer pour convertir l'UID en chaîne de caractères
String StrUID;  // Variable pour stocker l'UID en tant que chaîne

bool newScan = false;  // Variable pour indiquer un nouvel état de scan

void setup() {
  Serial.begin(115200);  // Initialise la communication série à 115200 baud
  SPI.begin();  // Initialise le bus SPI
  mfrc522.PCD_Init();  // Initialise le lecteur MFRC522

  delay(500);  // Attend 500 millisecondes

  WiFi.begin(ssid, password);  // Se connecte au réseau WiFi
  Serial.println("");  // Imprime une ligne vide

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");  // Imprime un point pour indiquer la tentative de connexion
    delay(500);  // Attend 500 millisecondes
  }
  Serial.println("");  // Imprime une ligne vide
  Serial.print("Successfully connected to : ");
  Serial.println(ssid);  // Affiche le SSID du réseau WiFi
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  // Affiche l'adresse IP locale

  Serial.println("Please tag a card or keychain to see the UID !");
  Serial.println("");  // Imprime une ligne vide

  ThingSpeak.begin(client);  // Initialise ThingSpeak avec le client WiFi
}

void loop() {
  readsuccess = getid();  // Appelle la fonction getid() pour lire l'UID de la carte

  if (readsuccess) {
    newScan = true;  // Indique qu'un nouvel état de scan a été détecté

    HTTPClient http;  // Crée un objet HTTPClient

    String UIDresultSend, postData;
    UIDresultSend = StrUID;  // Stocke l'UID lu dans la variable UIDresultSend

    postData = "UIDresult=" + UIDresultSend;  // Prépare les données à envoyer via HTTP POST

    http.begin("http://192.168.3.106:8080/projet_IOT1/getUID.php");  // Démarre la connexion HTTP
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");  // Ajoute l'en-tête HTTP

    int httpCode = http.POST(postData);  // Envoie la requête POST avec les données
    String payload = http.getString();  // Obtient la réponse du serveur

    Serial.println(UIDresultSend);  // Affiche l'UID envoyé
    Serial.println(httpCode);  // Affiche le code de réponse HTTP
    Serial.println(payload);  // Affiche la réponse du serveur

    http.end();  // Ferme la connexion HTTP
    delay(1000);  // Attend 1000 millisecondes
  }

  if (newScan && WiFi.status() == WL_CONNECTED) {
    HTTPClient http;  // Crée un objet HTTPClient

    String url = String("http://") + server + "/fetch_transactions.php";  // Prépare l'URL pour la requête HTTP
    http.begin(url);  // Démarre la connexion HTTP

    int httpCode = http.GET();  // Envoie la requête GET
    if (httpCode > 0) {
      int total_achats = 0;  // Variable pour stocker le nombre total d'achats

      String payload = http.getString();  // Obtient la réponse du serveur
      Serial.println(payload);  // Affiche la réponse du serveur

      DynamicJsonDocument doc(1024);  // Crée un document JSON dynamique
      deserializeJson(doc, payload);  // Désérialise la réponse JSON
      String nam_achats = "";
      String nbre_achats_str = ""; 
      for (JsonObject transaction : doc.as<JsonArray>()) {
        String product_name = transaction["product_name"];  // Extrait le nom du produit
        int nbre_achats_int = transaction["nbre_achats"];  // Extrait le nombre d'achats
        total_achats += nbre_achats_int;  // Ajoute au total des achats
        nam_achats += String(product_name) + "|";  // Ajoute le nom du produit à la chaîne
        nbre_achats_str += String(nbre_achats_int) + "|";  // Ajoute le nombre d'achats à la chaîne

        Serial.print("Product: ");
        Serial.print(product_name);  // Affiche le nom du produit
        Serial.print(" - Purchases: ");
        Serial.println(nbre_achats_int);  // Affiche le nombre d'achats
      }

      Serial.print("Total Achats: ");
      Serial.println(total_achats);  // Affiche le total des achats
      ThingSpeak.setField(1, nam_achats);  // Définit le champ 1 avec les noms des produits
      ThingSpeak.setField(2, nbre_achats_str);  // Définit le champ 2 avec les nombres d'achats
      ThingSpeak.setField(3, total_achats);  // Définit le champ 3 avec le total des achats

      int x = ThingSpeak.writeFields(myChannelNumber, apiKey);  // Envoie les données à ThingSpeak
      if (x == 200) {
        Serial.println("Channel update successful.");  // Indique que la mise à jour du canal est réussie
      } else {
        Serial.println("Problem updating channel. HTTP error code " + String(x));  // Indique une erreur de mise à jour
      }
    } else {
      Serial.println("Error on HTTP request");  // Indique une erreur dans la requête HTTP
    }

    http.end();  // Ferme la connexion HTTP

    newScan = false;  // Réinitialise l'état de scan
  }

  delay(1000);  // Attend 1000 millisecondes
}

int getid() {  
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return 0;  // Si aucune nouvelle carte n'est présente, retourne 0
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return 0;  // Si la lecture de la carte échoue, retourne 0
  }

  Serial.print("THE UID OF THE SCANNED CARD IS : ");
  
  for (int i = 0; i < 4; i++) {
    readcard[i] = mfrc522.uid.uidByte[i];  // Lit chaque octet de l'UID
    array_to_string(readcard, 4, str);  // Convertit l'UID en chaîne de caractères
    StrUID = str;  // Stocke l'UID en tant que chaîne
  }
  mfrc522.PICC_HaltA();  // Arrête la communication avec la carte
  return 1;  // Retourne 1 pour indiquer que la lecture a réussi
}

void array_to_string(byte array[], unsigned int len, char buffer[]) {
  for (unsigned int i = 0; i < len; i++) {
    byte nib1 = (array[i] >> 4) & 0x0F;  // Extrait le premier nibble (4 bits) de l'octet
    byte nib2 = (array[i] >> 0) & 0x0F;  // Extrait le deuxième nibble (4 bits) de l'octet
    buffer[i*2+0] = nib1 < 0xA ? '0' + nib1 : 'A' + nib1 - 0xA;  // Convertit le premier nibble en caractère hexadécimal
    buffer[i*2+1] = nib2 < 0xA ? '0' + nib2 : 'A' + nib2 - 0xA;  // Convertit le deuxième nibble en caractère hexadécimal
  }
  buffer[len*2] = '\0';  // Ajoute le caractère nul à la fin de la chaîne
}
