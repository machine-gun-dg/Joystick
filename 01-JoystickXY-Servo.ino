#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h> // Libreria per il controllo del LED RGB

// === Configurazione Hardware LED RGB (Neopixel) ===
#define LED_PIN 48     // Pin GPIO 48: standard per il LED integrato sull'ESP32-S3
#define NUM_LEDS 1     // C'è solo 1 LED integrato
// Inizializza l'oggetto NeoPixel
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// === Definizioni Dati (Identiche al Master) ===
enum Direction { NORD, EST, SUD, OVEST, INVALID_DIRECTION };
typedef struct struct_message {
    Direction direction;
} struct_message;

struct_message incomingData;
// ===============================================

// Funzione di utilità per convertire l'enum in stringa (per il Serial Monitor)
const char* directionToString(Direction dir) {
    switch (dir) {
        case NORD: return "NORD";
        case EST: return "EST";
        case SUD: return "SUD";
        case OVEST: return "OVEST";
        default: return "DIREZIONE NON VALIDA";
    }
}

// 🚦 FUNZIONE: Controlla il colore del LED (Chiamata da OnDataRecv)
void setLedColor(Direction dir) {
    uint32_t color;
    
    // Assegna un colore a ciascuna direzione
    switch (dir) {
        case NORD:
            color = strip.Color(0, 0, 255); // Blu (simbolo di Nord / freddo)
            break;
        case EST:
            color = strip.Color(255, 100, 0); // Arancione (simbolo di Est / alba)
            break;
        case SUD:
            color = strip.Color(255, 0, 0); // Rosso (simbolo di Sud / caldo)
            break;
        case OVEST:
            color = strip.Color(0, 255, 0); // Verde (simbolo di Ovest / natura)
            break;
        default:
            color = strip.Color(5, 5, 5); // Bianco (Errore/Default)
            break;
    }
    
    strip.setPixelColor(0, color);
    strip.show(); // Applica il nuovo colore al LED
}


// Callback eseguita alla ricezione dei dati (CON LA FIRMA CORRETTA)
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingDataPtr, int len) {
    const uint8_t * mac_addr = recv_info->src_addr; 
    
    // Copia il pacchetto binario nella nostra struttura
    memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
    
    const char* receivedDirectionStr = directionToString(incomingData.direction);
    
    Serial.print("\n--- NEW SIGNAL RECEIVED ---");
    Serial.print(">> RECEIVED DIRECTION: ");
    Serial.println(receivedDirectionStr);

    // *** CHIAMATA PER AGGIORNARE IL COLORE DEL LED ***
    setLedColor(incomingData.direction); 
    Serial.println("LED Color Updated.");
    Serial.println("---------------------------\n");
}
 
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);

    // Inizializza il LED RGB e lo spegne
    strip.begin(); 
    strip.setBrightness(50); // Imposta una luminosità sicura
    strip.clear(); 
    strip.show();
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    
    // Registra la funzione di callback per la ricezione
    esp_now_register_recv_cb(OnDataRecv);

    Serial.println("ESP-NOW Slave Ready. LED ready for color changes...");
}
 
void loop() {
    // La logica è gestita dalle callback, il loop è minimo
    delay(10);
}
