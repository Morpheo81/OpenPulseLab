#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h> // Libreria WebSockets di Markus Sattler
#include <ArduinoJson.h>      // Libreria per la gestione di JSON (installala!)

// --- Configurazione Wi-Fi Access Point ---
const char *ssid = "ECG_Monitor";     // Nome della rete Wi-Fi che l'ESP creerà
const char *password = "password123"; // Password della rete (puoi lasciarla vuota: const char *password = ""; per nessuna password)

// --- Inizializzazione dei Server ---
ESP8266WebServer server(80);         // Server HTTP sulla porta 80
WebSocketsServer webSocket = WebSocketsServer(81); // Server WebSockets sulla porta 81

// --- Configurazione Pin Lead-Off Detection (AD8232) ---
// Collega LO+ del AD8232 a D2 (GPIO4) e LO- a D1 (GPIO5) sul tuo NodeMCU/Wemos.
// Verifica la tua board per i pin GPIO effettivi associati a D1, D2, ecc.
// L'AD8232 invia HIGH quando il lead è scollegato, LOW quando è connesso.
// Se il tuo modulo ha logica invertita, potresti dover invertire i 0/1 nel JS.
const int LO_PLUS_PIN = 4;  // Esempio: pin D2 (GPIO4) collegato a LO+ del AD8232
const int LO_MINUS_PIN = 5; // Esempio: pin D1 (GPIO5) collegato a LO- del AD8232

// Pin del LED aggiuntivo da utilizzare per segnalazioni di errori o stato di funzionamento (GPIO12)
const int LED_PIN = 12;

// Pin del BUZZER da utilizzare per allarmi (GPIO13)
const int BUZZER_PIN = 13;


// --- Pagina HTML da servire al Browser ---
// Contiene HTML, CSS e JavaScript per la visualizzazione e le funzionalità
const char* htmlPage = R"rawliteral(

<!DOCTYPE html>
<html>
<head>
    <title>EMG Muscolare Differenziale</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
        /* Il canvas non è più necessario ma manteniamo lo stile per consistenza, nascondendolo o rimuovendolo */
        canvas { display: none; } 
        h1 { color: #333; }
        p { font-size: 1.2em; color: #555; }
        /* Stile per i parametri di controllo */
        .control-params {
            margin-top: 20px;
            background-color: #eee;
            padding: 15px;
            border-radius: 8px;
            display: inline-block;
            text-align: left;
        }
        .control-params label {
            display: block;
            margin-top: 10px;
            font-weight: bold;
        }
        .control-params input[type="number"] {
            width: 80px;
            padding: 5px;
            border-radius: 4px;
            border: 1px solid #ccc;
        }
        /* Stile per gli indicatori */
        .indicator-container {
            margin-top: 30px;
            margin-bottom: 30px;
            display: flex;
            justify-content: center;
            gap: 40px;
            align-items: center;
        }
        .indicator-group {
            text-align: center;
        }
        .indicator-label {
            font-size: 1.5em;
            font-weight: bold;
            color: #333;
            margin-bottom: 10px;
        }
        .indicator {
            width: 40px;
            height: 40px;
            border-radius: 50%;
            background-color: gray; /* Spento */
            border: 3px solid #333;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.2);
            transition: background-color 0.1s;
        }
        .indicator.active-muscle-1 {
            background-color: #2196F3; /* Blu per Muscolo 1 */
            box-shadow: 0 0 20px #2196F3, 0 0 30px #2196F3;
        }
        .indicator.active-muscle-2 {
            background-color: #FFC107; /* Giallo/Arancio per Muscolo 2 */
            box-shadow: 0 0 20px #FFC107, 0 0 30px #FFC107;
        }
    </style>
</head>
<body>
    <h1>Muscle Activations</h1>

    <div class="indicator-container">
        <div class="indicator-group">
            <div class="indicator-label">MUSCLE 1</div>
            <div id="muscle1Indicator" class="indicator"></div>
        </div>
        <div class="indicator-group">
            <div class="indicator-label">MUSCLE 2</div>
            <div id="muscle2Indicator" class="indicator"></div>
        </div>
    </div>
    
    <p>DATA: ROW: <span id="currentActivityValue">--</span></p>
    <p>DATA: Filtered: <span id="currentNormalizedValue">--</span></p>

    <div class="control-params">
        <h3>Sensibility</h3>
        
        <label for="activationThreshold">Treshold (0-512):</label>
        <input type="number" id="activationThreshold" value="50" min="1" max="512">
        
        <label for="sensitivityFactor">Amplification:</label>
        <input type="number" id="sensitivityFactor" value="1.0" min="0.1" step="0.1">
    </div>

    <canvas id="ecgCanvas" width="1" height="1"></canvas> 
    
    <script>
        var ws;
        var adcMidPoint = 512; // Punto medio per un ADC a 10 bit (0-1023)
        
        // --- Elementi UI ---
        var muscle1Indicator = document.getElementById('muscle1Indicator');
        var muscle2Indicator = document.getElementById('muscle2Indicator');
        var thresholdInput = document.getElementById('activationThreshold');
        var sensitivityInput = document.getElementById('sensitivityFactor');
        
        // --- Parametri di Controllo ---
        var threshold = parseFloat(thresholdInput.value);
        var sensitivity = parseFloat(sensitivityInput.value);

        // Event Listeners per i cambiamenti dei valori
        thresholdInput.addEventListener('change', function() {
            threshold = parseFloat(this.value);
            console.log('Soglia aggiornata:', threshold);
        });
        sensitivityInput.addEventListener('change', function() {
            sensitivity = parseFloat(this.value);
            console.log('Sensibilità aggiornata:', sensitivity);
        });

        // --- Funzione Principale di Attivazione Muscolare ---
        function checkMuscleActivation(rawValue) {
            // 1. Calcola lo scostamento dal punto medio (segnala quale muscolo è attivo)
            // Se rawValue > 512, lo scostamento è positivo (Muscolo 1, es. Flessore)
            // Se rawValue < 512, lo scostamento è negativo (Muscolo 2, es. Estensore)
            var deviation = rawValue - adcMidPoint;

            // 2. Calcola l'intensità (attività) in valore assoluto
            var absoluteActivity = Math.abs(deviation);
            
            // 3. Applica il fattore di sensibilità (amplificazione)
            var normalizedActivity = absoluteActivity * sensitivity;

            // 4. Aggiorna i valori visualizzati
            document.getElementById('currentActivityValue').innerText = deviation.toFixed(0);
            document.getElementById('currentNormalizedValue').innerText = normalizedActivity.toFixed(0);

            // 5. Logica di Attivazione
            if (normalizedActivity > threshold) {
                // L'attività è abbastanza forte
                if (deviation > 0) {
                    // Muscolo 1 è dominante (scostamento positivo)
                    muscle1Indicator.classList.add('active-muscle-1');
                    muscle2Indicator.classList.remove('active-muscle-2');
                    console.log("Muscolo 1 ATTIVO (Flessione)");
                } else {
                    // Muscolo 2 è dominante (scostamento negativo)
                    muscle2Indicator.classList.add('active-muscle-2');
                    muscle1Indicator.classList.remove('active-muscle-1');
                    console.log("Muscolo 2 ATTIVO (Estensione)");
                }
            } else {
                // Nessuna attività sufficiente
                muscle1Indicator.classList.remove('active-muscle-1');
                muscle2Indicator.classList.remove('active-muscle-2');
            }
        }

        // --- Funzione per Connettersi al WebSocket ---
        function connectWebSocket() {
            ws = new WebSocket('ws://' + location.hostname + ':81/');

            ws.onopen = function() {
                console.log('WebSocket Connesso!');
            };

            ws.onmessage = function(event) {
                // Assumiamo che l'ESP8266 invii ancora un JSON con il valore ADC
                try {
                    var dataPacket = JSON.parse(event.data);
                    var rawValue = dataPacket.ecg; // Riusiamo la chiave 'ecg' ma ora rappresenta l'EMG
                    
                    if (!isNaN(rawValue)) {
                        checkMuscleActivation(rawValue);
                    }
                } catch (e) {
                    console.error("Errore nel parsing JSON o dati non validi:", e, event.data);
                }
            };

            ws.onclose = function() {
                console.log('WebSocket Disconnesso, tento di riconnettermi...');
                // Spegne le spie in caso di disconnessione
                muscle1Indicator.classList.remove('active-muscle-1');
                muscle2Indicator.classList.remove('active-muscle-2');
                setTimeout(connectWebSocket, 3000); // Tenta di riconnettersi dopo 3 secondi
            };

            ws.onerror = function(error) {
                console.error('Errore WebSocket:', error);
            };
        }

        // --- Inizializzazione all'apertura della Pagina ---
        window.onload = function() {
            connectWebSocket(); // Avvia la connessione WebSocket
        };
    </script>
</body>
</html>

)rawliteral";

// --- Gestore Eventi WebSockets ---
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Client Disconnesso!\n", num);
      break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Client Connesso da %d.%d.%d.%d URL: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      webSocket.sendTXT(num, "Benvenuto al monitor ECG!"); // Invia un messaggio di benvenuto al nuovo client
    }
    break;
    case WStype_TEXT:
      Serial.printf("[%u] Ricevuto testo: %s\n", num, payload);
      // Puoi aggiungere logica qui se il browser invia comandi all'ESP
      break;
    case WStype_BIN:
    case WStype_PING:
    case WStype_PONG:
    case WStype_ERROR:
      // Ignora questi tipi di messaggi per questo progetto
      break;
  }
}

// --- Funzione di Setup (Eseguita una sola volta all'avvio dell'ESP) ---
void setup() {
  Serial.begin(115200); // Inizializza la comunicazione seriale per il debug
  Serial.println("\n");
  Serial.println("Avvio ECG Monitor...");

  // Configura i pin per la Lead-Off Detection come input
  // L'AD8232 tipicamente ha uscite Lead-Off che sono HIGH (pull-up interno)
  // quando l'elettrodo è scollegato, e LOW quando è collegato.
  // Quindi, INPUT sarà sufficiente. Se hai problemi, prova INPUT_PULLUP.
  pinMode(LO_PLUS_PIN, INPUT);
  pinMode(LO_MINUS_PIN, INPUT);

  //Configurazione dei pin del LED e del BUZZER 
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  //Impostazione del valore di default: il LED è attivo alto mentre il BUZZER è attivo basso
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH);


  // Configura l'ESP come Access Point Wi-Fi
  WiFi.softAP(ssid, password);
  Serial.print("Access Point \"");
  Serial.print(ssid);
  Serial.println("\" creato.");
  Serial.print("Indirizzo IP per la connessione: ");
  Serial.println(WiFi.softAPIP()); // Stampa l'IP dell'ESP (solitamente 192.168.4.1)

  // Configura il server HTTP per servire la pagina web
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", htmlPage); // Invia la pagina HTML al browser
  });
  server.begin();
  Serial.println("Server HTTP avviato sulla porta 80.");

  // Avvia il server WebSockets e registra la funzione di callback per gli eventi
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("Server WebSockets avviato sulla porta 81.");

  // Inizializza il pin analogico A0
  // La risoluzione ADC è nativamente a 10 bit sull'ESP8266, quindi analogRead() fornirà valori 0-1023.
  Serial.println("ADC configurato nativamente a 10 bit.");
  Serial.println("Pronto per lo streaming ECG.");
}

int ecgValue;

// --- Funzione Loop (Eseguita continuamente dopo il setup) ---
void loop() {
  // Gestisce le richieste del server HTTP (per servire la pagina web)
  server.handleClient();
  // Gestisce gli eventi del server WebSockets (nuove connessioni, messaggi, ecc.)
  webSocket.loop();


  // Legge lo stato dei pin di Lead-Off Detection
  // L'AD8232 tipicamente invia HIGH (1) quando il lead è scollegato (problema),
  // e LOW (0) quando è collegato (OK).
  int loPlusStatus = digitalRead(LO_PLUS_PIN);
  int loMinusStatus = digitalRead(LO_MINUS_PIN);
int temp_ECG[9];
digitalWrite(LED_PIN, LOW);
  int c1;
  int c2;
  int c3;
  for(c1=0;c1<5;c1++) temp_ECG[c1] = 0;
  for(c1=0;c1<5;c1++)
  {
    ecgValue = analogRead(A0);
    digitalWrite(BUZZER_PIN, HIGH);
    if(ecgValue>760){
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, HIGH);
    }
    for(c2=0;c2<5-1;c2++)
    {    
      if(ecgValue>temp_ECG[c2])
      {
        for(c3=c2;c3<5-1;c3++)
        {
          temp_ECG[c3+1]=temp_ECG[c3];
        }
        temp_ECG[c2] = ecgValue;
        c2=1000;
      }
    }
  }
  ecgValue = temp_ECG[3];

delay(8);

  // Prepara un oggetto JSON con i dati ECG e gli stati di Lead-Off
  StaticJsonDocument<64> doc; // Dimensione adatta per un piccolo JSON (64 byte sono abbondanti)
  doc["ecg"] = (ecgValue-512)*2;
  doc["lo_plus"] = loPlusStatus;
  doc["lo_minus"] = loMinusStatus;

  // Serializza l'oggetto JSON in una stringa
  String jsonString;
  serializeJson(doc, jsonString);

  // Invia la stringa JSON a tutti i client WebSockets connessi
  webSocket.broadcastTXT(jsonString.c_str());


  // Legge il valore analogico dal pin A0 (uscita dell'AD8232)
  //int ecgValue = (analogRead(A0) + analogRead(A0))/2;
  /*int temp_ECG[9];
  
  
  digitalWrite(LED_PIN, LOW);
  int c1;
  int c2;
  int c3;
  for(c1=0;c1<9;c1++) temp_ECG[c1] = 0;
  for(c1=0;c1<9;c1++)
  {
    ecgValue = analogRead(A0);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1);
    if(ecgValue>760){
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, HIGH);
    }
    for(c2=0;c2<9-1;c2++)
    {    
      if(ecgValue>temp_ECG[c2])
      {
        for(c3=c2;c3<9-1;c3++)
        {
          temp_ECG[c3+1]=temp_ECG[c3];
        }
        temp_ECG[c2] = ecgValue;
        c2=1000;
      }
    }
  }
  ecgValue = temp_ECG[5];
 */



 //Serial.println(ecgValue);
 delay(1);
}