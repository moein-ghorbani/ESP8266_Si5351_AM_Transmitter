#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <si5351.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>

// ==================== WiFi Settings ====================
const char* ssid = "ESP_Si5351_AP";
const char* password = "12345678";

// ==================== Oscillator Settings ====================
Si5351 si5351;
uint32_t currentFrequency = 0;
uint32_t melodyBaseFrequency = 100000000;
uint32_t lastFrequencyBeforeMelody = 100000000;
const uint32_t minFreq = 10000;
const uint32_t maxFreq = 160000000;

// ==================== Audio PWM Settings ====================
const int audioPin = 14;
const int pwmFreq = 40000;
const int pwmResolution = 10;
int currentVolume = 512;
bool audioPlaying = false;
unsigned long lastNoteTimePWM = 0;
int currentMelodyIndex = 0;

// ==================== EEPROM Settings ====================
#define EEPROM_SIZE 32
#define ADDR_FREQ 0
bool eepromSavePending = false;
unsigned long lastFreqChange = 0;
bool eepromInitialized = false;

// ==================== Melody Notes ====================
const uint16_t melodyFreq[] PROGMEM = { 
    262, 294, 330, 349, 392, 440, 494, 523, 
    494, 440, 392, 349, 330, 294, 262
};
const int melodyLength = 15;
const int noteDurationPWM = 300;

// ==================== Web Server ====================
ESP8266WebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Si5351 AM Transmitter</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            max-width: 500px;
            width: 100%;
            background: white;
            border-radius: 20px;
            padding: 30px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        }
        h1 { color: #667eea; text-align: center; margin-bottom: 10px; font-size: 24px; }
        .subtitle { text-align: center; color: #666; margin-bottom: 30px; font-size: 12px; }
        .freq-card {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border-radius: 15px;
            padding: 20px;
            text-align: center;
            margin-bottom: 25px;
        }
        .freq-value { font-size: 32px; font-weight: bold; direction: ltr; font-family: monospace; }
        .freq-unit { font-size: 16px; }
        .label { font-size: 12px; opacity: 0.8; margin-bottom: 5px; }
        .card { background: #f7f7f7; border-radius: 15px; padding: 20px; margin-bottom: 20px; }
        .card-title { font-weight: bold; color: #667eea; margin-bottom: 15px; font-size: 18px; }
        input[type="range"] {
            width: 100%;
            height: 5px;
            -webkit-appearance: none;
            background: #ddd;
            border-radius: 5px;
            outline: none;
            margin: 20px 0;
        }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 20px;
            height: 20px;
            background: #667eea;
            border-radius: 50%;
            cursor: pointer;
        }
        .range-labels { display: flex; justify-content: space-between; font-size: 12px; color: #888; margin-top: -15px; }
        .unit-buttons { display: flex; gap: 10px; margin: 15px 0; }
        .unit-btn {
            flex: 1; padding: 10px; border: 2px solid #667eea; background: white;
            color: #667eea; border-radius: 10px; cursor: pointer; text-align: center;
        }
        .unit-btn.active { background: #667eea; color: white; }
        .preset-buttons { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; margin-top: 15px; }
        .preset-btn {
            background: white; border: 1px solid #ddd; padding: 10px;
            border-radius: 10px; text-align: center; cursor: pointer;
        }
        .preset-btn:hover { background: #667eea; color: white; }
        .melody-btn {
            width: 100%; padding: 15px; border: none; border-radius: 10px;
            font-size: 18px; font-weight: bold; cursor: pointer; margin-top: 10px;
        }
        .melody-btn.on { background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%); color: white; }
        .melody-btn.off { background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%); color: white; }
        .volume-slider { margin: 15px 0; }
        .volume-slider label { display: block; margin-bottom: 8px; color: #555; }
        .status { margin-top: 15px; padding: 10px; border-radius: 10px; text-align: center; font-size: 12px; display: none; }
        .status.show { display: block; }
        .status.success { background: #d4edda; color: #155724; }
        .status.info { background: #d1ecf1; color: #0c5460; }
        .footer { text-align: center; font-size: 10px; color: #999; margin-top: 20px; }
        .warning-note { background: #fff3cd; color: #856404; padding: 8px; border-radius: 8px; font-size: 11px; margin-top: 10px; text-align: center; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔄 Si5351 AM Transmitter</h1>
        <div class="subtitle">RF Carrier + Real Audio via PWM</div>
        
        <div class="freq-card">
            <div class="label">Carrier Frequency</div>
            <div class="freq-value" id="currentFreq">---</div>
        </div>
        
        <div class="card">
            <div class="card-title">🎚️ Carrier Frequency</div>
            <input type="range" id="freqSlider" min="0" max="100" value="50">
            <div class="range-labels">
                <span>10 kHz</span>
                <span>80 MHz</span>
                <span>160 MHz</span>
            </div>
            
            <div class="card-title" style="margin-top: 20px;">📊 Fine Tuning</div>
            <div class="unit-buttons">
                <div class="unit-btn" data-unit="hz">Hz</div>
                <div class="unit-btn active" data-unit="khz">kHz</div>
                <div class="unit-btn" data-unit="mhz">MHz</div>
            </div>
            <input type="number" id="freqInput" style="width: 100%; padding: 12px; border: 2px solid #ddd; border-radius: 10px; text-align: center; font-family: monospace; font-size: 16px;">
            
            <div class="preset-buttons">
                <div class="preset-btn" data-freq="10000">10 kHz</div>
                <div class="preset-btn" data-freq="100000">100 kHz</div>
                <div class="preset-btn" data-freq="1000000">1 MHz</div>
                <div class="preset-btn" data-freq="10000000">10 MHz</div>
                <div class="preset-btn" data-freq="50000000">50 MHz</div>
                <div class="preset-btn" data-freq="100000000">100 MHz</div>
            </div>
        </div>
        
        <div class="card">
            <div class="card-title">🎵 Real Audio Playback (PWM on GPIO14/D5)</div>
            <div class="volume-slider">
                <label>🔊 Volume: <span id="volumeValue">50</span>%</label>
                <input type="range" id="volumeSlider" min="0" max="100" value="50">
            </div>
            <button id="melodyBtn" class="melody-btn off" onclick="toggleAudio()">🎵 Start Melody</button>
            <div id="melodyStatus" class="status"></div>
            <div class="warning-note">
                ℹ️ Audio output on GPIO14 (D5) | Connect to ADE-1 mixer with Si5351 output
            </div>
        </div>
        
        <div id="status" class="status"></div>
        <div class="footer">Real-time AM Modulator | Si5351 + ESP8266</div>
    </div>
    
    <script>
        let currentUnit = 'khz';
        let isAudioPlaying = false;
        let currentFreqHz = 100000000;
        let freqTimer = null;
        let sliderTimer = null;
        
        function formatFrequency(freqHz) {
            if (currentUnit === 'hz') return freqHz;
            if (currentUnit === 'khz') return freqHz / 1000;
            if (currentUnit === 'mhz') return freqHz / 1000000;
            return freqHz;
        }
        
        function updateDisplay() {
            let displayValue = formatFrequency(currentFreqHz);
            let unitText = currentUnit.toUpperCase();
            document.getElementById('currentFreq').innerHTML = displayValue.toFixed(3) + ' <span class="freq-unit">' + unitText + '</span>';
        }
        
        function setFrequencyDelayed(freqHz) {
            if (isAudioPlaying) return;
            clearTimeout(freqTimer);
            freqTimer = setTimeout(() => {
                fetch('/setfreq?freq=' + Math.round(freqHz))
                    .then(() => {
                        currentFreqHz = Math.round(freqHz);
                        updateDisplay();
                    });
            }, 150);
        }
        
        function loadStatus() {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    currentFreqHz = data.freq;
                    isAudioPlaying = data.audio;
                    updateDisplay();
                    document.getElementById('freqInput').value = formatFrequency(currentFreqHz);
                    let percent = (currentFreqHz - 10000) / 159990000;
                    document.getElementById('freqSlider').value = Math.round(percent * 100);
                    
                    if (isAudioPlaying) {
                        document.getElementById('melodyBtn').innerHTML = '⏹️ Stop';
                        document.getElementById('melodyBtn').classList.remove('off');
                        document.getElementById('melodyBtn').classList.add('on');
                        document.getElementById('melodyStatus').innerHTML = '🎶 Playing melody on GPIO14';
                        document.getElementById('melodyStatus').classList.add('show');
                    } else {
                        document.getElementById('melodyBtn').innerHTML = '🎵 Start Melody';
                        document.getElementById('melodyBtn').classList.remove('on');
                        document.getElementById('melodyBtn').classList.add('off');
                        document.getElementById('melodyStatus').classList.remove('show');
                    }
                });
        }
        
        const slider = document.getElementById('freqSlider');
        slider.addEventListener('input', function() {
            if (isAudioPlaying) return;
            clearTimeout(sliderTimer);
            sliderTimer = setTimeout(() => {
                const percent = this.value / 100;
                const freqHz = Math.round(10000 + (percent * 159990000));
                currentFreqHz = freqHz;
                document.getElementById('freqInput').value = formatFrequency(freqHz);
                setFrequencyDelayed(freqHz);
            }, 10);
        });
        
        const freqInput = document.getElementById('freqInput');
        freqInput.addEventListener('input', function() {
            if (isAudioPlaying) return;
            let value = parseFloat(this.value);
            if (isNaN(value)) return;
            
            let freqHz;
            if (currentUnit === 'hz') freqHz = value;
            else if (currentUnit === 'khz') freqHz = value * 1000;
            else freqHz = value * 1000000;
            
            if (freqHz < 10000) freqHz = 10000;
            if (freqHz > 160000000) freqHz = 160000000;
            
            let percent = (freqHz - 10000) / 159990000;
            slider.value = Math.round(percent * 100);
            setFrequencyDelayed(freqHz);
        });
        
        document.querySelectorAll('.unit-btn').forEach(btn => {
            btn.addEventListener('click', function() {
                if (isAudioPlaying) return;
                document.querySelectorAll('.unit-btn').forEach(b => b.classList.remove('active'));
                this.classList.add('active');
                currentUnit = this.dataset.unit;
                document.getElementById('freqInput').value = formatFrequency(currentFreqHz);
                updateDisplay();
            });
        });
        
        document.querySelectorAll('.preset-btn').forEach(btn => {
            btn.addEventListener('click', function() {
                if (isAudioPlaying) return;
                let freqHz = parseInt(this.dataset.freq);
                let percent = (freqHz - 10000) / 159990000;
                slider.value = Math.round(percent * 100);
                document.getElementById('freqInput').value = formatFrequency(freqHz);
                setFrequencyDelayed(freqHz);
            });
        });
        
        const volumeSlider = document.getElementById('volumeSlider');
        volumeSlider.addEventListener('input', function() {
            let volume = this.value;
            document.getElementById('volumeValue').innerText = volume;
            fetch('/setvolume?vol=' + volume);
        });
        
        function toggleAudio() {
            if (!isAudioPlaying) {
                fetch('/startaudio').then(() => loadStatus());
            } else {
                fetch('/stopaudio').then(() => loadStatus());
            }
        }
        
        loadStatus();
        setInterval(loadStatus, 5000);
    </script>
</body>
</html>
)rawliteral";

// ==================== EEPROM Functions ====================
void initEEPROM() {
    if (!eepromInitialized) {
        EEPROM.begin(EEPROM_SIZE);
        eepromInitialized = true;
    }
}

uint32_t loadFrequencyFromEEPROM() {
    initEEPROM();
    uint32_t freq;
    EEPROM.get(ADDR_FREQ, freq);
    if (freq < minFreq || freq > maxFreq) {
        freq = 100000000UL;
    }
    return freq;
}

void markFrequencyChanged() {
    lastFreqChange = millis();
    eepromSavePending = true;
}

void handleEEPROMSave() {
    if (eepromSavePending && (millis() - lastFreqChange > 5000)) {
        initEEPROM();
        EEPROM.put(ADDR_FREQ, currentFrequency);
        EEPROM.commit();
        eepromSavePending = false;
        Serial.println(F("Frequency saved to EEPROM"));
    }
}

// ==================== Si5351 Functions ====================
void setOscillatorFrequency(uint32_t freqHz, bool suppressSave = false) {
    freqHz = constrain(freqHz, minFreq, maxFreq);
    if (currentFrequency != 0 && freqHz == currentFrequency) return;
    
    si5351.set_freq((uint64_t)freqHz * 100ULL, SI5351_CLK0);
    bool wasFirstBoot = (currentFrequency == 0);
    currentFrequency = freqHz;
    
    if (!wasFirstBoot && !suppressSave) {
        markFrequencyChanged();
    }
    Serial.printf("Carrier: %lu Hz\n", currentFrequency);
}

// ==================== Audio PWM Functions ====================
void initAudio() {
    analogWriteFreq(pwmFreq);
    analogWriteRange(1 << pwmResolution);
    pinMode(audioPin, OUTPUT);
    analogWrite(audioPin, 0);
}

void setVolume(uint8_t percent) {
    currentVolume = map(percent, 0, 100, 0, (1 << pwmResolution) - 1);
}

void playNote(uint16_t freq, int duration) {
    if (freq == 0) {
        analogWrite(audioPin, 0);
        delay(duration);
        return;
    }
    
    unsigned int halfPeriod = 1000000 / (freq * 2);
    unsigned long startTime = micros();
    unsigned long endTime = startTime + (duration * 1000);
    bool state = true;
    
    while (micros() < endTime) {
        analogWrite(audioPin, state ? currentVolume : 0);
        state = !state;
        delayMicroseconds(halfPeriod);
    }
    analogWrite(audioPin, 0);
}

void playMelodyPWM() {
    if (!audioPlaying) return;
    
    if (millis() - lastNoteTimePWM >= noteDurationPWM) {
        lastNoteTimePWM = millis();
        uint16_t note = pgm_read_word(&melodyFreq[currentMelodyIndex]);
        playNote(note, noteDurationPWM);
        
        currentMelodyIndex++;
        if (currentMelodyIndex >= melodyLength) {
            currentMelodyIndex = 0;
        }
    }
}

// ==================== Web Handlers ====================
void handleRoot() { server.send_P(200, "text/html", index_html); }

void handleGetStatus() {
    String json = "{\"freq\":" + String(currentFrequency) + ",\"audio\":" + String(audioPlaying ? "true" : "false") + "}";
    server.send(200, "application/json", json);
}

void handleSetFrequency() {
    if (!server.hasArg("freq")) {
        server.send(400, "text/plain", "missing");
        return;
    }
    
    uint32_t newFreq = strtoul(server.arg("freq").c_str(), nullptr, 10);
    if (newFreq < minFreq || newFreq > maxFreq) {
        server.send(400, "text/plain", "range");
        return;
    }
    
    if (!audioPlaying) setOscillatorFrequency(newFreq, false);
    server.send(200, "text/plain", "ok");
}

void handleStartAudio() {
    if (!audioPlaying) {
        eepromSavePending = false;
        audioPlaying = true;
        currentMelodyIndex = 0;
        lastNoteTimePWM = millis();
        server.send(200, "text/plain", "ok");
    } else {
        server.send(400, "text/plain", "already playing");
    }
}

void handleStopAudio() {
    if (audioPlaying) {
        audioPlaying = false;
        analogWrite(audioPin, 0);
        server.send(200, "text/plain", "ok");
    } else {
        server.send(400, "text/plain", "not playing");
    }
}

void handleSetVolume() {
    if (server.hasArg("vol")) {
        setVolume(server.arg("vol").toInt());
        server.send(200, "text/plain", "ok");
    } else {
        server.send(400, "text/plain", "missing");
    }
}

void handleNotFound() { server.send(404, "application/json", "{\"error\":\"not found\"}"); }

// ==================== WiFi ====================
void setupWiFi() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    Serial.println(F("WiFi AP Started"));
    Serial.printf("SSID: %s | Password: %s\n", ssid, password);
    Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
}

// ==================== Setup ====================
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println(F("\nSi5351 AM Transmitter v1.0"));
    
    initAudio();
    setVolume(50);
    
    Wire.begin(4, 5);
    if (!si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0)) {
        Serial.println(F("Si5351 not detected!"));
        while (1) delay(1000);
    }
    
    si5351.set_correction(0, SI5351_PLL_INPUT_XO);
    si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
    si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA);
    
    uint32_t savedFreq = loadFrequencyFromEEPROM();
    setOscillatorFrequency(savedFreq, false);
    
    setupWiFi();
    
    server.on("/", handleRoot);
    server.on("/status", handleGetStatus);
    server.on("/setfreq", handleSetFrequency);
    server.on("/startaudio", handleStartAudio);
    server.on("/stopaudio", handleStopAudio);
    server.on("/setvolume", handleSetVolume);
    server.onNotFound(handleNotFound);
    server.begin();
    
    Serial.println(F("\nSystem Ready"));
    Serial.println(F("-----------------------------------"));
    Serial.printf("Si5351 Output: GPIO1 (TX) - RF Carrier\n");
    Serial.printf("Audio Output: GPIO%d (D5) - PWM\n", audioPin);
    Serial.printf("Web UI: http://%s\n", WiFi.softAPIP().toString().c_str());
    Serial.println(F("-----------------------------------\n"));
}

// ==================== Loop ====================
void loop() {
    server.handleClient();
    handleEEPROMSave();
    
    if (audioPlaying) {
        playMelodyPWM();
    }
    
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 15000) {
        lastPrint = millis();
        Serial.printf("Carrier: %lu Hz | Audio: %s | Heap: %lu\n", 
                     currentFrequency, 
                     audioPlaying ? "ON" : "OFF",
                     ESP.getFreeHeap());
    }
}
