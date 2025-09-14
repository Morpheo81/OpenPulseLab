# OpenPulseLab
A Low-Cost, Open-Source ECG Monitor for educational and research uses

> [!NOTE]
> This project is currently under active development and is a work in progress (WIP).

![ECG ESP8266](https://github.com/Morpheo81/OpenPulseLab/blob/main/ECG_ESP8266_B.jpg?raw=true)
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

This project leverages an **ESP8266 microcontroller** to create a simple, Wi-Fi enabled ECG monitor. It processes the analog signals from an **AD8232 ECG sensor module** and serves a web page via an on-board web server. Users can access this page from any web browser to view real-time heart data and save it. The device also includes hardware for **lead-off detection** and a **buzzer/LED** for error signaling.

* **Open and Accessible:** All hardware schematics, firmware, and documentation are provided.
* **Low-Cost:** Built with readily available and inexpensive components.
* **Self-Contained:** Designed to run on a battery, ensuring full electrical isolation and safety from the mains power supply.

### How It Works

1.  **Signal Acquisition:** The AD8232 sensor module captures the electrical signals of the heart.
2.  **Signal Conversion:** The ESP8266's analog-to-digital converter (ADC) reads the output from the AD8232.
3.  **Data Transmission:** The ESP8266 acts as a web server, broadcasting the raw data via Wi-Fi and using **WebSockets** for real-time streaming.
4.  **Data Visualization:** A simple HTML/JavaScript front-end displays the data on a graph in a web browser. A software filter (median on X, moving average on Y) is applied to smooth the raw data.

**Here an example of the Web Server running on ESP8266**
![WebServer](https://github.com/Morpheo81/OpenPulseLab/blob/main/proto/WebServer.jpg?raw=true)

### Potential Applications

While not a medical tool, this project has several fascinating applications:

* **Educational:** A practical, hands-on tool for teaching electronics, programming, and biometrics.
* **Research:** A base platform for research into advanced signal processing algorithms (e.g., using AI to identify patterns in nerve impulses).
* **Human-Computer Interface (HCI):** The same principles can be used to detect neuromuscular signals, allowing the device to act as a controller for a PC or other machines.

### Getting Started

To build your prototipe device, you will need:


![Board](https://github.com/Morpheo81/OpenPulseLab/blob/main/proto/Proto_1_0.jpg)


* **Components: for Rev 1.0**
    * Buzzer (BZ1) - not mandatory
    * 0.1uF capacitors (C1, C3)
    * 100uF capacitor (C2)
    * capacitor (C5)
    * diodes (D1, D2)
    * PinSocket 1x06 (J2, J3) and 1x02 (J1) connectors
    * Molex 1x02 connector (J4)
    * LED for error (LED1)
    * LED for power (LED2)
    * 10K resistors (R1, R6)
    * 1K resistor (R2)
    * 22K resistors (R3, R4, R5)
    * 100 resistors (R8, R9, R10)
    * SW_Push button (SW1)
    * SW_DIP_x01 switch (SW2)
    * test points (TP1, TP2, TP3, TP4)
    * ESP-12F WiFi module (U1)
 NOTE: SMD resistor and ceramic capacitor are 0805
* **Tools:**
    * Soldering iron and soldering materials.
    * **AD8232 module with electrode.**
    * Serial programmer.
 
![Programming](https://github.com/Morpheo81/OpenPulseLab/blob/main/proto/Programming_Proto_1_0.jpg)

* **Firmware & Libraries:**
    * `ECG_ESP8266.ino` firmware file
    * Arduino Libraries: `ESP8266WiFi`, `ESP8266WebServer`, `WebSocketsServer` by Markus Sattler, `ArduinoJson`
* **Files:**
    * The hardware files are in the `KiCad` folder.
    * The firmware files are in the `ArduinoIDE` folder.

### Example of Electrode Placement and Important Notes


<img src="https://github.com/Morpheo81/OpenPulseLab/blob/main/img/Electrodes.jpg" alt="descrizione" width="200" height="300"/>

This device uses TENS/EMS (Transcutaneous Electrical Nerve Stimulation / Electrical Muscle Stimulation) electrodes instead of traditional, single-use ECG electrodes. While standard ECG electrodes are specifically designed for low impedance and optimal signal fidelity, they are often expensive and intended for single use due to their hydrogel-based adhesive.

Our choice to use TENS/EMS pads is a deliberate design decision to prioritize reusability and user convenience. These pads, commonly used for muscle stimulation, have a more durable adhesive, allowing for multiple uses before replacement. This makes the OpenPulseLab device more cost-effective and accessible, particularly for educational and hobbyist purposes.

It's important to note the technical trade-off involved. The impedance of TENS/EMS pads is typically higher than that of medical-grade ECG electrodes. This can result in a weaker signal from the AD8232 sensor. If you choose to use different types of electrodes, such as standard ECG electrodes, you may need to re-evaluate and adjust the amplification factors and any filtering thresholds within the firmware to achieve the best possible signal quality.

### Licensing

This project is released under the **Creative Commons Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)** license. This means you are free to share and adapt the material for any purpose, even commercially, as long as you provide appropriate attribution and distribute your contributions under the same license.

**Copyright (c) 2025 Fabio Besuzzi**
