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
  <title>ECG Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
    canvas { border: 1px solid black; background-color: #f0f0f0; }
    h1 { color: #333; }
    p { font-size: 1.2em; color: #555; }
    button {
      padding: 10px 20px;
      font-size: 1em;
      margin-top: 20px;
      cursor: pointer;
      background-color: #4CAF50;
      color: white;
      border: none;
      border-radius: 5px;
      margin: 5px; /* Spaziatura tra i bottoni */
    }
    button.clear-button {
      background-color: #f44336; /* Rosso per il reset */
    }
    button.clear-button:hover {
      background-color: #da190b;
    }
    button:hover {
      background-color: #45a049;
    }
    .indicator-container {
      margin-top: 20px;
      display: flex;
      justify-content: center;
      gap: 20px;
      align-items: center;
    }
    .indicator {
      width: 25px;
      height: 25px;
      border-radius: 50%;
      background-color: gray; /* Spento */
      border: 2px solid #333;
    }
    .indicator.active {
      background-color: red; /* Acceso */
    }
    .filter-controls {
        margin-top: 20px;
        background-color: #eee;
        padding: 15px;
        border-radius: 8px;
        display: inline-block;
    }
    .filter-controls label {
        margin-right: 10px;
        font-weight: bold;
    }
    .filter-controls input[type="number"] {
        width: 60px;
        padding: 5px;
        border-radius: 4px;
        border: 1px solid #ccc;
    }
  </style>
</head>
<body>
  <h1>ECG Live Stream</h1>
  <canvas id="ecgCanvas" width="800" height="300"></canvas>
  <p>Valore Attuale ADC (Grezzo): <span id="currentRawValue">--</span></p>
  <p>Valore Attuale ADC (Filtrato): <span id="currentFilteredValue">--</span></p>
  
  <div class="indicator-container">
    <span>LO+</span><div id="loPlusIndicator" class="indicator"></div>
    <span>LO-</span><div id="loMinusIndicator" class="indicator"></div>
  </div>

  <div class="filter-controls">
      <label for="filterX">Filtro Mediana (X):</label>
      <input type="number" id="filterX" value="1" min="1">
      <label for="filterY">Filtro Media (Y):</label>
      <input type="number" id="filterY" value="1" min="1">
  </div>

  <button id="saveDataBtn">Salva Dati ECG (Ultimi 30s)</button>
  <button id="clearDataBtn" class="clear-button">Nuova Sessione</button>

  <script>
    var ws;
    var canvas = document.getElementById('ecgCanvas');
    var ctx = canvas.getContext('2d');
    var dataBuffer = []; // Buffer per i dati grezzi ricevuti (per filtro e grafico)
    var filteredData = []; // Buffer per i dati filtrati visualizzati nel grafico

    // --- Configurazione Tempo e Campionamento ---
    var desiredDurationSeconds = 30; // Durata dei dati da mantenere (30 secondi)
    var samplesPerSecond = 100; // Frequenza di campionamento (campioni al secondo) - DEVE CORRISPONDERE AL DELAY LATO ESP!
    var maxDataPoints = desiredDurationSeconds * samplesPerSecond; // Numero massimo di punti da mantenere nell'array

    // --- Parametri di Visualizzazione del Grafico ---
    var scaleFactor = 0.2; // Fattore di scala per i dati (regola in base ai valori ADC 0-1023)
    var yOffset = canvas.height / 2; // Centro del grafico (per il segnale centrato)

    // --- Elementi Indicatori Lead-Off ---
    var loPlusIndicator = document.getElementById('loPlusIndicator');
    var loMinusIndicator = document.getElementById('loMinusIndicator');

    // --- Elementi Input Filtro ---
    var filterXInput = document.getElementById('filterX');
    var filterYInput = document.getElementById('filterY');
    var filterX = parseInt(filterXInput.value);
    var filterY = parseInt(filterYInput.value);

    // Event Listeners per i cambiamenti dei valori del filtro
    filterXInput.addEventListener('change', function() {
        filterX = parseInt(this.value);
        if (filterX < 1) filterX = 1; // Minimo 1
        this.value = filterX;
        clearECGData(); // Reset dei dati filtrati quando i parametri cambiano
    });
    filterYInput.addEventListener('change', function() {
        filterY = parseInt(this.value);
        if (filterY < 1) filterY = 1; // Minimo 1
        this.value = filterY;
        clearECGData(); // Reset dei dati filtrati quando i parametri cambiano
    });


    // --- Funzione per Connettersi al WebSocket ---
    function connectWebSocket() {
      ws = new WebSocket('ws://' + location.hostname + ':81/');

      ws.onopen = function() {
        console.log('WebSocket Connesso!');
      };

      ws.onmessage = function(event) {
        // I dati arrivano come stringa JSON: {"ecg": 512, "lo_plus": 0, "lo_minus": 0}
        try {
          var dataPacket = JSON.parse(event.data);
          var rawValue = dataPacket.ecg;
          var loPlus = dataPacket.lo_plus;
          var loMinus = dataPacket.lo_minus;

          if (!isNaN(rawValue)) {
            document.getElementById('currentRawValue').innerText = rawValue;
            
            // --- AGGIORNAMENTO STATI LEAD-OFF ---
            // L'AD8232 in modalità DC Lead-off con OUTPUT_HIGH_WHEN_NO_LEAD
            // invia HIGH (1) quando il lead è scollegato, LOW (0) quando è connesso.
            // Se la tua implementazione o modulo è diverso, inverti la logica qui.
            if (loPlus === 1) { // 1 = problema (Lead Off), 0 = OK
              loPlusIndicator.classList.add('active'); // Accende la spia rossa
            } else {
              loPlusIndicator.classList.remove('active'); // Spegne la spia
            }
            if (loMinus === 1) {
              loMinusIndicator.classList.add('active');
            } else {
              loMinusIndicator.classList.remove('active');
            }

            dataBuffer.push(rawValue); // Aggiungi il valore grezzo al buffer
            // Mantiene il buffer dei dati grezzi limitato (per il filtro)
            if (dataBuffer.length > maxDataPoints) { 
              dataBuffer.shift(); 
            }
            
            // --- FILTRAGGIO SOFTWARE (Mediana su X, Media su Y) ---
            let filteredVal = filterData(dataBuffer, filterX, filterY);
            if (filteredVal !== null) {
                document.getElementById('currentFilteredValue').innerText = filteredVal.toFixed(0); // Mostra il valore filtrato
                
                // Mantiene il buffer dei dati filtrati limitato
                // Questo buffer viene utilizzato per la visualizzazione e il salvataggio
                filteredData.push(filteredVal);
                if (filteredData.length > maxDataPoints) {
                    filteredData.shift();
                }
                
                drawGraph(filteredData); // Disegna il grafico con i dati filtrati
            } else {
                // Se non ci sono abbastanza dati per il filtro (inizio sessione),
                // puoi scegliere di non disegnare nulla o disegnare il grezzo
                // Per ora, non disegniamo nulla finché non ci sono abbastanza dati filtrati
                // drawGraph(dataBuffer); // Opzionale: disegna il grezzo all'inizio
            }
          }
        } catch (e) {
          console.error("Errore nel parsing JSON o dati non validi:", e, event.data);
        }
      };

      ws.onclose = function() {
        console.log('WebSocket Disconnesso, tento di riconnettermi...');
        setTimeout(connectWebSocket, 3000); // Tenta di riconnettersi dopo 3 secondi
      };

      ws.onerror = function(error) {
        console.error('Errore WebSocket:', error);
      };
    }

    // --- Funzione per il Filtraggio (Mediana su X, Media su Y) ---
    // Questa funzione calcola un singolo punto filtrato per ogni nuovo dato grezzo
    // Combinando un filtro mediana e un filtro media mobile.
    function filterData(buffer, x, y) {
        // La mediana richiede almeno X campioni
        if (buffer.length < x) {
            return null; // Non ci sono abbastanza dati per la mediana
        }

        // 1. Applica il filtro Mediana sugli ultimi X valori grezzi
        // Crea una copia temporanea degli ultimi X valori per non modificare l'originale
        let medianWindow = [];
        for(let i = 0; i < x; i++) {
            medianWindow.push(buffer[buffer.length - x + i]);
        }
        medianWindow.sort((a, b) => a - b); // Ordina per trovare la mediana

        let currentMedian;
        if (medianWindow.length % 2 === 0) {
            currentMedian = (medianWindow[medianWindow.length / 2 - 1] + medianWindow[medianWindow.length / 2]) / 2;
        } else {
            currentMedian = medianWindow[Math.floor(medianWindow.length / 2)];
        }

        // Se non ci sono ancora abbastanza dati filtrati per la media mobile su Y,
        // restituisci solo la mediana corrente.
        if (filteredData.length < y -1) { // -1 perché il currentMedian si aggiungerà
             return currentMedian;
        }

        // 2. Applica il filtro Media (Media Mobile) sugli ultimi Y valori filtrati (inclusa la mediana corrente)
        // Somma degli ultimi Y valori filtrati (compreso il valore medianato corrente, ma lo aggiungiamo dopo)
        let sumFiltered = currentMedian; // Inizia con la mediana appena calcolata
        for (let i = 0; i < y - 1; i++) { // Somma i precedenti (Y-1) valori già filtrati nel buffer
            // Si assicura di non andare oltre l'inizio dell'array filteredData
            if (filteredData.length - 1 - i >= 0) {
                 sumFiltered += filteredData[filteredData.length - 1 - i];
            } else {
                // Se non ci sono abbastanza dati precedenti per la media su Y, usa la mediana
                sumFiltered += currentMedian; // Ripete la mediana
            }
        }
        
        return sumFiltered / y; // Ritorna il valore finalmente filtrato
    }


    // --- Funzione per Disegnare il Grafico ECG sul Canvas ---
    function drawGraph(dataToDraw) { // Accetta l'array di dati da disegnare
      ctx.clearRect(0, 0, canvas.width, canvas.height); // Pulisci l'intero canvas
      
      // Disegna la linea centrale (riferimento, circa 1.65V per un ADC 0-1023)
      ctx.strokeStyle = '#cccccc'; // Grigio chiaro
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(0, yOffset);
      ctx.lineTo(canvas.width, yOffset);
      ctx.stroke();

      // Disegna il segnale ECG
      ctx.beginPath();
      ctx.strokeStyle = 'blue';
      ctx.lineWidth = 2;

      // Calcola l'incremento X per disegnare i punti distribuiti sulla larghezza del canvas
      var xIncrement = canvas.width / maxDataPoints; 

      for (var i = 0; i < dataToDraw.length; i++) {
        var x = i * xIncrement;
        // Normalizza e centra il valore ADC (0-1023) attorno all'offset Y del canvas
        // 512 è il punto medio per un ADC a 10 bit (1023 max)
        var y = yOffset - (dataToDraw[i] - 512) * scaleFactor;

        if (i === 0) {
          ctx.moveTo(x, y); // Inizia il percorso per il primo punto
        } else {
          ctx.lineTo(x, y); // Continua il percorso per i punti successivi
        }
      }
      ctx.stroke(); // Disegna la linea del grafico
    }

    // --- Funzione per Salvare i Dati ECG Attuali in un File CSV ---
    function saveECGData() {
      if (filteredData.length === 0) { // Salviamo i dati filtrati
        alert("Nessun dato da salvare!");
        return;
      }
      
      // Calcola l'intervallo di tempo per ogni campione
      const sampleIntervalMs = 1000 / samplesPerSecond; // Millisecondi per campione
      
      let csvContent = "Time_ms,ECG_Value\n"; // Nuova intestazione con colonna del tempo
      
      // Itera sui dati filtrati e aggiungi il tempo
      for (let i = 0; i < filteredData.length; i++) {
        const timeMs = i * sampleIntervalMs;
        csvContent += `${timeMs},${filteredData[i].toFixed(0)}\n`; // .toFixed(0) per arrotondare i valori filtrati all'intero
      }
      
      // Crea un oggetto Blob (Binary Large Object)
      const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
      
      // Crea un URL temporaneo per il Blob
      const url = URL.createObjectURL(blob);
      
      // Crea un elemento <a> (link) nascosto
      const a = document.createElement('a');
      a.href = url;
      
      // Imposta il nome del file suggerito
      const now = new Date();
      const filename = `ECG_data_with_time_${now.getFullYear()}-${(now.getMonth()+1).toString().padStart(2, '0')}-${now.getDate().toString().padStart(2, '0')}_${now.getHours().toString().padStart(2, '0')}-${now.getMinutes().toString().padStart(2, '0')}-${now.getSeconds().toString().padStart(2, '0')}.csv`;
      a.download = filename;
      
      // Aggiungi l'elemento al DOM, cliccalo, e poi rimuovilo
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      
      // Rilascia l'URL del Blob
      URL.revokeObjectURL(url);
    }

    // --- Funzione per Cancellare i Dati e Iniziare una Nuova Sessione ---
    function clearECGData() {
      dataBuffer = []; // Svuota l'array dei dati grezzi
      filteredData = []; // Svuota l'array dei dati filtrati
      ctx.clearRect(0, 0, canvas.width, canvas.height); // Pulisci il canvas
      drawGraph([]); // Ridiseegna il grafico (ora vuoto, con solo la linea centrale)
      document.getElementById('currentRawValue').innerText = '--'; // Resetta i valori visualizzati
      document.getElementById('currentFilteredValue').innerText = '--';
      
      // Spegni le spie Lead-Off
      loPlusIndicator.classList.remove('active');
      loMinusIndicator.classList.remove('active');

      console.log('Dati ECG cancellati. Nuova sessione avviata.');
    }

    // --- Inizializzazione all'apertura della Pagina ---
    window.onload = function() {
      connectWebSocket(); // Avvia la connessione WebSocket
      // Collega le funzioni ai click dei rispettivi pulsanti
      document.getElementById('saveDataBtn').addEventListener('click', saveECGData);
      document.getElementById('clearDataBtn').addEventListener('click', clearECGData); 
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

// --- Funzione Loop (Eseguita continuamente dopo il setup) ---
void loop() {
  // Gestisce le richieste del server HTTP (per servire la pagina web)
  server.handleClient();
  // Gestisce gli eventi del server WebSockets (nuove connessioni, messaggi, ecc.)
  webSocket.loop();

  // Legge il valore analogico dal pin A0 (uscita dell'AD8232)
  int ecgValue = (analogRead(A0) + analogRead(A0))/2;
  

  // Legge lo stato dei pin di Lead-Off Detection
  // L'AD8232 tipicamente invia HIGH (1) quando il lead è scollegato (problema),
  // e LOW (0) quando è collegato (OK).
  int loPlusStatus = digitalRead(LO_PLUS_PIN);
  int loMinusStatus = digitalRead(LO_MINUS_PIN);

  // Prepara un oggetto JSON con i dati ECG e gli stati di Lead-Off
  StaticJsonDocument<64> doc; // Dimensione adatta per un piccolo JSON (64 byte sono abbondanti)
  doc["ecg"] = ecgValue;
  doc["lo_plus"] = loPlusStatus;
  doc["lo_minus"] = loMinusStatus;

  // Serializza l'oggetto JSON in una stringa
  String jsonString;
  serializeJson(doc, jsonString);

  // Invia la stringa JSON a tutti i client WebSockets connessi
  webSocket.broadcastTXT(jsonString.c_str());

  // Aggiungi un piccolo ritardo per controllare la frequenza di campionamento.
  // Un delay di 10ms corrisponde a 100 campioni al secondo (100 Hz),
  // il che deve corrispondere alla variabile 'samplesPerSecond' nel JavaScript.
  int ciclo;
  for(ciclo=0;ciclo<4;ciclo++) //Aggiunsto il numero di cicli per avere esattamente 30s
  {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1);
    if(ecgValue>760){
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, HIGH);
    } 
    delay(1);
  }
  digitalWrite(LED_PIN, LOW);

}