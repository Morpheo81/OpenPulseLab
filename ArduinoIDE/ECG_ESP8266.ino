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
    h1, h2 { color: #333; }
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

  <h2>Dettaglio Picco QRS (Media)</h2>
  <canvas id="ecgZoomCanvas" width="800" height="300"></canvas>

  <h2>Grafico Completo (30s)</h2>
  <canvas id="ecgCanvas" width="800" height="300"></canvas>

  <p>Valore Attuale ADC (Grezzo): <span id="currentRawValue">--</span></p>
  <p>Valore Attuale ADC (Filtrato): <span id="currentFilteredValue">--</span></p>
  
  <div class="indicator-container">
    <span>LO+</span><div id="loPlusIndicator" class="indicator"></div>
    <span>LO-</span><div id="loMinusIndicator" class="indicator"></div>
  </div>

  <div class="filter-controls">
      <label for="filterX">Filtro Mediana (X):</label>
      <input type="number" id="filterX" value="2" min="1">
      <label for="filterY">Filtro Media (Y):</label>
      <input type="number" id="filterY" value="1" min="1">
  </div>

  <button id="saveDataBtn">Salva Dati ECG (Ultimi 30s)</button>
  <button id="clearDataBtn" class="clear-button">Nuova Sessione</button>

  <script>
    var ws;
    // Grafico principale (30s)
    var canvas = document.getElementById('ecgCanvas');
    var ctx = canvas.getContext('2d');
    // Grafico di zoom (media)
    var zoomCanvas = document.getElementById('ecgZoomCanvas');
    var zoomCtx = zoomCanvas.getContext('2d');

    var dataBuffer = [];
    var filteredData = [];
    var peakBuffer = [];
    var peakLocations = []; 

    // --- Configurazione Tempo e Campionamento ---
    var desiredDurationSeconds = 30;
    var samplesPerSecond = 100;
    var maxDataPoints = desiredDurationSeconds * samplesPerSecond;
    
    // --- Configurazione Grafico di Zoom (Media) ---
    var beatDurationMs = 700; // Durata stimata di un battito in ms
    var zoomMaxDataPoints = Math.ceil((beatDurationMs / 1000) * samplesPerSecond);
    var lastPeakTime = 0; 
    const peakDetectionIntervalMs = 500;
    const beatsToAverage = 10; // Numero di battiti da mediare

    // --- Parametri di Visualizzazione del Grafico ---
    var scaleFactor = 0.2;
    var yOffset = canvas.height / 2;
    var zoomYOffset = zoomCanvas.height / 2;

    // --- Elementi Indicatori Lead-Off ---
    var loPlusIndicator = document.getElementById('loPlusIndicator');
    var loMinusIndicator = document.getElementById('loMinusIndicator');

    // --- Elementi Input Filtro ---
    var filterXInput = document.getElementById('filterX');
    var filterYInput = document.getElementById('filterY');
    var filterX = parseInt(filterXInput.value);
    var filterY = parseInt(filterYInput.value);

    filterXInput.addEventListener('change', function() {
        filterX = parseInt(this.value);
        if (filterX < 1) filterX = 1;
        this.value = filterX;
        clearECGData();
    });
    filterYInput.addEventListener('change', function() {
        filterY = parseInt(this.value);
        if (filterY < 1) filterY = 1;
        this.value = filterY;
        clearECGData();
    });

    function connectWebSocket() {
      ws = new WebSocket('ws://' + location.hostname + ':81/');
      ws.onopen = function() {
        console.log('WebSocket Connesso!');
      };

      ws.onmessage = function(event) {
        try {
          var dataPacket = JSON.parse(event.data);
          var rawValue = dataPacket.ecg;
          var loPlus = dataPacket.lo_plus;
          var loMinus = dataPacket.lo_minus;

          if (!isNaN(rawValue)) {
            document.getElementById('currentRawValue').innerText = rawValue;
            
            if (loPlus === 1) {
              loPlusIndicator.classList.add('active');
            } else {
              loPlusIndicator.classList.remove('active');
            }
            if (loMinus === 1) {
              loMinusIndicator.classList.add('active');
            } else {
              loMinusIndicator.classList.remove('active');
            }

            dataBuffer.push(rawValue);
            if (dataBuffer.length > maxDataPoints) { 
              dataBuffer.shift(); 
            }
            
            let filteredVal = filterData(dataBuffer, filterX, filterY);
            if (filteredVal !== null) {
              document.getElementById('currentFilteredValue').innerText = filteredVal.toFixed(0);
              
              filteredData.push(filteredVal);
              if (filteredData.length > maxDataPoints) {
                filteredData.shift();
              }
              
              detectAndAveragePeaks(filteredVal);
              drawGraph(filteredData);
              drawZoomGraph(peakBuffer);
            }
          }
        } catch (e) {
          console.error("Errore nel parsing JSON o dati non validi:", e, event.data);
        }
      };

      ws.onclose = function() {
        console.log('WebSocket Disconnesso, tento di riconnettermi...');
        setTimeout(connectWebSocket, 3000);
      };

      ws.onerror = function(error) {
        console.error('Errore WebSocket:', error);
      };
    }

    function detectAndAveragePeaks(currentValue) {
      const threshold = 700;
      const now = Date.now();
      
      if (filteredData.length > 2) {
        const last = filteredData[filteredData.length - 1];
        const secondLast = filteredData[filteredData.length - 2];
        
        // Rileva un picco se il valore attuale è sopra la soglia e sta diminuendo (massimo locale)
        // e se è passato abbastanza tempo dall'ultimo picco rilevato
        if (last > threshold && last > secondLast && now - lastPeakTime > peakDetectionIntervalMs) {
          peakLocations.push(filteredData.length - 1);
          lastPeakTime = now;
          
          // Mantiene il buffer dei picchi con gli ultimi 11 picchi (per avere almeno 10 da mediare)
          if (peakLocations.length > beatsToAverage + 1) {
            peakLocations.shift();
          }

          if (peakLocations.length >= beatsToAverage) {
            calculateAverageBeat();
          }
        }
      }
    }

    function calculateAverageBeat() {
        if (peakLocations.length < beatsToAverage) {
            return;
        }

        let totalBeats = new Array(zoomMaxDataPoints).fill(0);
        const halfBeatSamples = Math.floor(zoomMaxDataPoints / 2);
        let validBeatsCount = 0;

        for (let i = 0; i < beatsToAverage; i++) {
            const peakIndex = peakLocations[peakLocations.length - 1 - i];
            const startIndex = peakIndex - halfBeatSamples;
            const endIndex = startIndex + zoomMaxDataPoints;
            
            if (startIndex >= 0 && endIndex < filteredData.length) {
                const currentBeat = filteredData.slice(startIndex, endIndex);
                for (let j = 0; j < zoomMaxDataPoints; j++) {
                    totalBeats[j] += currentBeat[j];
                }
                validBeatsCount++;
            }
        }
        
        if (validBeatsCount > 0) {
            peakBuffer = totalBeats.map(val => val / validBeatsCount);
        }
    }

    function filterData(buffer, x, y) {
      if (buffer.length < x) {
          return null;
      }
      let medianWindow = [];
      for(let i = 0; i < x; i++) {
          medianWindow.push(buffer[buffer.length - x + i]);
      }
      medianWindow.sort((a, b) => a - b);

      let currentMedian;
      if (medianWindow.length % 2 === 0) {
          currentMedian = (medianWindow[medianWindow.length / 2 - 1] + medianWindow[medianWindow.length / 2]) / 2;
      } else {
          currentMedian = medianWindow[Math.floor(medianWindow.length / 2)];
      }

      if (filteredData.length < y -1) {
           return currentMedian;
      }

      let sumFiltered = currentMedian;
      for (let i = 0; i < y - 1; i++) {
          if (filteredData.length - 1 - i >= 0) {
              sumFiltered += filteredData[filteredData.length - 1 - i];
          } else {
              sumFiltered += currentMedian;
          }
      }
      
      return sumFiltered / y;
    }

    function drawGraph(dataToDraw) {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      
      ctx.strokeStyle = '#cccccc';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(0, yOffset);
      ctx.lineTo(canvas.width, yOffset);
      ctx.stroke();

      ctx.beginPath();
      ctx.strokeStyle = 'blue';
      ctx.lineWidth = 2;

      var xIncrement = canvas.width / maxDataPoints;

      for (var i = 0; i < dataToDraw.length; i++) {
        var x = i * xIncrement;
        var y = yOffset - (dataToDraw[i] - 512) * scaleFactor;

        if (i === 0) {
          ctx.moveTo(x, y);
        } else {
          ctx.lineTo(x, y);
        }
      }
      ctx.stroke();
    }

    function drawZoomGraph(dataToDraw) {
      zoomCtx.clearRect(0, 0, zoomCanvas.width, zoomCanvas.height);
      
      zoomCtx.strokeStyle = '#cccccc';
      zoomCtx.lineWidth = 1;
      zoomCtx.beginPath();
      zoomCtx.moveTo(0, zoomYOffset);
      zoomCtx.lineTo(zoomCanvas.width, zoomYOffset);
      zoomCtx.stroke();

      zoomCtx.beginPath();
      zoomCtx.strokeStyle = 'red';
      zoomCtx.lineWidth = 2;

      var xIncrement = zoomCanvas.width / zoomMaxDataPoints;

      for (var i = 0; i < dataToDraw.length; i++) {
        var x = i * xIncrement;
        var y = zoomYOffset - (dataToDraw[i] - 512) * scaleFactor;

        if (i === 0) {
          zoomCtx.moveTo(x, y);
        } else {
          zoomCtx.lineTo(x, y);
        }
      }
      zoomCtx.stroke();
    }

    function saveECGData() {
      if (filteredData.length === 0) {
        alert("Nessun dato da salvare!");
        return;
      }
      
      const sampleIntervalMs = 1000 / samplesPerSecond;
      
      let csvContent = "Time_ms,ECG_Value\n";
      
      for (let i = 0; i < filteredData.length; i++) {
        const timeMs = i * sampleIntervalMs;
        csvContent += `${timeMs},${filteredData[i].toFixed(0)}\n`;
      }
      
      const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      
      const now = new Date();
      const filename = `ECG_data_with_time_${now.getFullYear()}-${(now.getMonth()+1).toString().padStart(2, '0')}-${now.getDate().toString().padStart(2, '0')}_${now.getHours().toString().padStart(2, '0')}-${now.getMinutes().toString().padStart(2, '0')}-${now.getSeconds().toString().padStart(2, '0')}.csv`;
      a.download = filename;
      
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      
      URL.revokeObjectURL(url);
    }

    function clearECGData() {
      dataBuffer = [];
      filteredData = [];
      peakBuffer = [];
      peakLocations = [];
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      zoomCtx.clearRect(0, 0, zoomCanvas.width, zoomCanvas.height);
      drawGraph([]);
      drawZoomGraph([]);
      document.getElementById('currentRawValue').innerText = '--';
      document.getElementById('currentFilteredValue').innerText = '--';
      
      loPlusIndicator.classList.remove('active');
      loMinusIndicator.classList.remove('active');
      lastPeakTime = 0;

      console.log('Dati ECG cancellati. Nuova sessione avviata.');
    }

    window.onload = function() {
      connectWebSocket();
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
