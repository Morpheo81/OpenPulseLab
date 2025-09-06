# OpenPulseLab
A Low-Cost, Open-Source ECG Monitor for educational and research uses

![ECG ESP8266](https://github.com/Morpheo81/OpenPulseLab/ECG_ESP8266_B.jpg?raw=true)

---

### A Note on Project Purpose and Responsibility

**This device is an educational and research prototype. It is NOT a certified medical device and should NEVER be used for diagnostic purposes.**

The primary goal of this project is to create an accessible, affordable, and open-source platform for learning about biometrics, signal processing, and IoT. Its simplicity is a feature, designed to lower the barrier to entry for students, hobbyists, and developers in emerging economies. The data quality is intentionally limited in this version, but can be improved with advanced software. By using this project, you acknowledge and agree to the full disclaimer below.

---

## ⚠️ Legal Disclaimer ⚠️

This project is provided "as is," without any warranty of any kind, either express or implied, including, but not limited to, the implied warranties of merchantability, fitness for a particular purpose, or non-infringement. In no event shall the authors or copyright holders be liable for any claim, damages, or other liability, whether in an action of contract, tort, or otherwise, arising from, out of, or in connection with the software, hardware, or the use or other dealings in the project.

**Do not use this device for medical diagnosis, treatment, or any clinical application. Consult a certified medical professional for any health-related concerns.**

---

### About the Project

[cite_start]This project leverages an **ESP8266 microcontroller** to create a simple, Wi-Fi enabled ECG monitor[cite: 2]. [cite_start]It processes the analog signals from an **AD8232 ECG sensor module** and serves a web page via an on-board web server[cite: 2]. [cite_start]Users can access this page from any web browser to view real-time heart data and save it[cite: 2]. [cite_start]The device also includes hardware for **lead-off detection** and a **buzzer/LED** for error signaling[cite: 2].

* **Open and Accessible:** All hardware schematics, firmware, and documentation are provided.
* **Low-Cost:** Built with readily available and inexpensive components.
* **Self-Contained:** Designed to run on a battery, ensuring full electrical isolation and safety from the mains power supply.

### How It Works

1.  [cite_start]**Signal Acquisition:** The AD8232 sensor module captures the electrical signals of the heart[cite: 2].
2.  [cite_start]**Signal Conversion:** The ESP8266's analog-to-digital converter (ADC) reads the output from the AD8232[cite: 2].
3.  [cite_start]**Data Transmission:** The ESP8266 acts as a web server, broadcasting the raw data via Wi-Fi and using **WebSockets** for real-time streaming[cite: 2].
4.  [cite_start]**Data Visualization:** A simple HTML/JavaScript front-end displays the data on a graph in a web browser[cite: 2]. [cite_start]A software filter (median on X, moving average on Y) is applied to smooth the raw data[cite: 2].

### Potential Applications

While not a medical tool, this project has several fascinating applications:

* **Educational:** A practical, hands-on tool for teaching electronics, programming, and biometrics.
* **Research:** A base platform for research into advanced signal processing algorithms (e.g., using AI to identify patterns in nerve impulses).
* **Human-Computer Interface (HCI):** The same principles can be used to detect neuromuscular signals, allowing the device to act as a controller for a PC or other machines.

### Getting Started

To build your own device, you will need:

* **Components:**
    * [cite_start]Buzzer (BZ1) [cite: 1]
    * [cite_start]2 0.1uF capacitors (C1, C3) [cite: 1]
    * [cite_start]1 100uF capacitor (C2) [cite: 1]
    * [cite_start]1 capacitor (C5) [cite: 1]
    * [cite_start]2 diodes (D1, D2) [cite: 1]
    * [cite_start]3 PinSocket 1x06 (J2, J3) and 1x02 (J1) connectors [cite: 1]
    * [cite_start]1 Molex 1x02 connector (J4) [cite: 1]
    * [cite_start]1 LED for error (LED1) [cite: 1]
    * [cite_start]1 LED for power (LED2) [cite: 1]
    * [cite_start]2 10K resistors (R1, R6) [cite: 1]
    * [cite_start]1 1K resistor (R2) [cite: 1]
    * [cite_start]3 22K resistors (R3, R4, R5) [cite: 1]
    * [cite_start]3 100 resistors (R8, R9, R10) [cite: 1]
    * [cite_start]1 SW_Push button (SW1) [cite: 1]
    * [cite_start]1 SW_DIP_x01 switch (SW2) [cite: 1]
    * [cite_start]4 test points (TP1, TP2, TP3, TP4) [cite: 1]
    * [cite_start]ESP-12F WiFi module (U1) [cite: 1]
* **Tools:**
    * Soldering iron and soldering materials.
* **Firmware & Libraries:**
    * [cite_start]`ECG_ESP8266.ino` firmware file [cite: 2]
    * [cite_start]Arduino Libraries: `ESP8266WiFi`, `ESP8266WebServer`, `WebSocketsServer` by Markus Sattler, `ArduinoJson` [cite: 2]
* **Files:**
    * The Bill of Materials (BOM) is in `ECG_ESP8266_BOM.csv`.
    * The firmware files are in the `firmware/` folder.

### How to Contribute

Your contributions are highly welcome! We're looking for help in several areas:
* **Firmware:** Optimizing the code, improving data acquisition, and adding new features.
* **Software:** Creating more advanced visualization tools, improving the web interface, or building a dedicated application.
* **Hardware:** Improving the PCB design for better signal quality or different form factors.
* **Documentation:** Expanding the guides and tutorials to make the project even more accessible.

Please check the [CONTRIBUTING.md](CONTRIBUTING.md) file for more details.

### Licensing

This project is released under the **Creative Commons Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)** license. This means you are free to share and adapt the material for any purpose, even commercially, as long as you provide appropriate attribution and distribute your contributions under the same license.

**Copyright (c) 2025 Fabio Besuzzi**
