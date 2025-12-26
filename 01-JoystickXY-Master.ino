#include <esp_now.h>
#include <WiFi.h>
#include "esp_wifi.h"
// !! SOSTITUISCI CON IL MAC ADDRESS REALE DEL TUO SLAVE !!
uint8_t slaveAddress[] = {0x1C, 0xDB, 0xD4, 0x5A, 0xC3, 0x68}; 
// -- Inclusione delle definizioni (Sezione 1) --
enum Direction { NORD, EST, SUD, OVEST, INVALID_DIRECTION };
typedef struct struct_message { Direction direction; } struct_message;
const char* directionNames[] = {"NORD", "EST", "SUD", "OVEST", "INVALID_DIRECTION"};
// ----------------------------------------------

struct_message myData;
esp_now_peer_info_t peerInfo;
// Non usiamo più directionIndex per cicli fixed
// int directionIndex = 0; 

// =======================================================
// CONFIGURAZIONE PIN E CALIBRAZIONE
// =======================================================
const int joyX = 1; // VRx - Analog input per l'asse X (Sterzo / Direzione)
const int joyY = 2; // VRy - Analog input per l'asse Y (Accelerazione / Marcia)
const int buttonPin = 16; // SW - Digital input per il pulsante

// Costanti ADC dell'ESP32
const int ADC_MIN = 0;
const int ADC_MAX = 4095;

// ** VALORI CALIBRATI DEL TUO JOYSTICK **
const int CENTER_Y_REAL = 1750; // Il centro Y reale (Marcia)
const int CENTER_X_REAL = 1750; // Il centro X reale (Sterzo)

// Parametri di Mappatura
const int DEADZONE = 100; // Zona morta: es. 1750 +/- 100
const int STEP_PERCENT = 10; // Arrotonda al multiplo di 10%

// Variabile per evitare di inviare lo stesso messaggio più volte
Direction lastSentDirection = INVALID_DIRECTION;

// =======================================================
// FUNZIONE DI INIZIALIZZAZIONE (ESEGUITA 1 VOLTA)
// =======================================================
void setup() {
  Serial.begin(115200);
  Serial.println("--- Joystick Master (ESP-NOW) ---");
  Serial.println("Centro Y calibrato: " + String(CENTER_Y_REAL));
  
  pinMode(buttonPin, INPUT_PULLUP);
  
  // START SERVER CONFIGURATION
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Registrazione della callback di invio corretta
  esp_now_register_send_cb(OnDataSent);
  
  // Registra il peer (lo Slave)
  memcpy(peerInfo.peer_addr, slaveAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  Serial.println("ESP-NOW Master Ready to send directions...");
}

// START SERVER CALLBACK
// Callback per notificare se il pacchetto è stato inviato con successo
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  // CORREZIONE: Usa src_addr come suggerito dal compilatore
  const uint8_t * mac_addr = tx_info->src_addr; 
  
  if (status == ESP_NOW_SEND_SUCCESS) {
    // Stampa il MAC address sorgente (Master) per conferma
    Serial.print("Packet sent by MAC: ");
    for(int i = 0; i < 6; i++) {
        Serial.printf("%02X%s", mac_addr[i], (i<5)?":":"");
    }
    Serial.println(" -> SUCCESS");
  } else {
    Serial.println("Packet failed to send.");
  }
}
// END SERVER CALLBACK

// =======================================================
// FUNZIONE DI MAPPATURA (LOGICA PRINCIPALE)
// =======================================================
/**
 * Mappa il valore analogico (0-4095) a una percentuale (-100 a +100)
 * usando un centro calibrato, arrotonda e INVERTE la direzione Y.
 */
int map_to_percentage(int value, int center, bool invert = false) {
  
  // 1. Controllo Zona Morta (Deadzone)
  if (value >= (center - DEADZONE) && value <= (center + DEADZONE)) {
    return 0; // 0% (STOP/CENTRO)
  }

  // 2. Mappatura Lineare Calibrata
  int raw_percentage;
  
  // Per l'asse Y (Marcia), la logica è invertita (spingendo in alto il valore cresce, ma vogliamo che sia negativo)
  if (invert) {
      if (value > center) {
          // Mappa i valori sopra il centro (alto) a 0 a -100 (Marcia Indietro/Freno)
          raw_percentage = map(value, center + DEADZONE, ADC_MAX, 0, -100);
      } else {
          // Mappa i valori sotto il centro (basso) a +100 a 0 (Marcia Avanti/Accelerazione)
          raw_percentage = map(value, ADC_MIN, center - DEADZONE, 100, 0); 
      }
  } else {
      // Per l'asse X (Sterzo), logica standard
      if (value > center) {
          raw_percentage = map(value, center + DEADZONE, ADC_MAX, 0, 100); // Destra
      } else {
          raw_percentage = map(value, ADC_MIN, center - DEADZONE, -100, 0); // Sinistra
      }
  }
  
  // 3. Arrotondamento al 10% più vicino
  int rounded_percentage = (int)round((float)raw_percentage / STEP_PERCENT) * STEP_PERCENT;

  // Garanzia dei limiti
  if (rounded_percentage > 100) return 100;
  if (rounded_percentage < -100) return -100;

  return rounded_percentage;
}


// =======================================================
// FUNZIONE PRINCIPALE (ESEGUITA CONTINUAMENTE)
// =======================================================
void loop() {
  int xValue = analogRead(joyX);
  int yValue = analogRead(joyY);
  int buttonState = digitalRead(buttonPin);

  // Mappa Y (Marcia) con inversione
  int yPercent = map_to_percentage(yValue, CENTER_Y_REAL, true);
  // Mappa X (Sterzo) senza inversione (Negativo=Sinistra, Positivo=Destra)
  int xPercent = map_to_percentage(xValue, CENTER_X_REAL, false);

  // 1. DETERMINAZIONE DELLA DIREZIONE DA INVIARE
  Direction currentDirection = INVALID_DIRECTION;

  // LOGICA DI PRIORITÀ: Y (Marcia/Freno) ha la precedenza
  if (yPercent > 0) { // Joystick tirato indietro (Marcia Avanti/Accelerazione)
      currentDirection = NORD; 
  } else if (yPercent < 0) { // Joystick spinto in avanti (Marcia Indietro/Freno)
      currentDirection = SUD; 
  } 
  // Se Y è neutrale (0), controlla X
  else if (xPercent > 0) { // Joystick a destra
      currentDirection = OVEST; // Utilizzo della tua mappatura OVEST (destra)
  } else if (xPercent < 0) { // Joystick a sinistra
      currentDirection = EST; // Utilizzo della tua mappatura EST (sinistra)
  } else {
      currentDirection = INVALID_DIRECTION; // Centro/Stop
  }


  // 2. CONTROLLO E INVIO DEL MESSAGGIO
  
  // Solo se la direzione è cambiata, invia il messaggio per non saturare la rete
  if (currentDirection != lastSentDirection) {
      
      myData.direction = currentDirection;
      
      Serial.print("\n--- SENDING --- Direction: ");
      Serial.print(directionNames[currentDirection]);
      Serial.print(" (Y: ");
      Serial.print(yPercent);
      Serial.print(" (");
      Serial.print(yValue);
      Serial.print(" )");
      Serial.print("%, X: ");
      Serial.print(xPercent);
      Serial.println("%)");
      Serial.print(" (");
      Serial.print(xValue);
      Serial.print(" )");
      
      // Invia il messaggio
      esp_now_send(slaveAddress, (uint8_t *) &myData, sizeof(myData));
      
      lastSentDirection = currentDirection;
      
  } else {
       // Stampa i risultati (Cleaned-up version)
        Serial.print("Y: ");
        Serial.print(yPercent);
        Serial.print("% | X: ");
        Serial.print(xPercent);
        Serial.print("% | Bottone: ");
        if (buttonState == LOW) Serial.println("**SCHIACCIATO**");
        else Serial.println("Rilasciato");
  }


  delay(100); // Ritardo per stabilizzare la lettura e l'invio
}
