# Smart Hydration Monitoring Using Skin Resistance and Environmental Sensors

**Note:** Throughout our testing and calibration process, we used the **[Bosch BME680](https://www.bosch-sensortec.com/products/environmental-sensors/gas-sensors/bme680/)** as a reference sensor for temperature and humidity. It helped us validate accuracy, drift, and other sensor behaviors in controlled conditions.

## What is Hydra Track?

Hydra Track is our attempt to build a **simple, portable, and reliable hydration monitoring system** using low-cost sensors and microcontrollers. It continuously monitors:

- **Skin impedance (GSR)** to estimate hydration trends
- **Body temperature ([DS18B20](https://www.analog.com/en/products/ds18b20.html))**
- **Ambient temperature and humidity ([DHT22](https://www.adafruit.com/product/385))**

The device classifies hydration levels into:

- **Hydrated**
- **Partially Hydrated**
- **Dehydrated**

The system displays this in real-time on an OLED screen and gives alerts through a buzzer. It also hosts a local website on the [ESP32](https://www.espressif.com/en/products/socs/esp32) for live data viewing.

## Implementation And Testing

We realized that to effectively achieve our goals we carried out extensive calibration we worked out on extensive sensor characterization for the sensors to ensure that they are ready and capable of measuring hydration. We have kept the sampling rate low to ignore features and spikes due to continuous body movements and stress.

These are the main parameters which we characterized to ensure the sensors are working nominally and find max thresholds that can be allowed:

**Accuracy**
Difference between the sensor output and the actual value.

**Precision**
Consistency of repeated readings under the same condition.

**Resolution**
Smallest detectable change in the measured quantity.

**Drift**
Slow change in output over time without any change in input.

**Linearity**
How well the output follows a straight-line relationship with the input.

**Hysteresis**
Difference in output when input increases versus when it decreases.

## 1. GSR Sensor

The GSR (Galvanic Skin Response) sensor measures skin resistance, which varies with hydration and sweat levels. In this system, the baseline is set to 0 and the maximum value is capped at 2850 after drift compensation.

![GSR Accuracy Graph](https://github.com/Ahilnandan/Hydration_Monitoring/blob/main/test_logs/GSR/Accuracy.jpg)

The above graph for GSR accuracy was plotted by using a known resistance value of 2MΩ and the expected and observed GSR values were found considering the internal resistance of the Grove GSR's voltage divider and max reading of 2850 the GSR is calculated for complete conduction.

109 turns out to be the fudge factor that needs to be added to make the Observed GSR readings more accurate.

The following statistical parameters were found:

1. Standard Deviation: -10.5
2. Average: 245.5 (After Correction)
3. RMS Error: 15.93

![GSR Linearity Graph]([image3.png](https://github.com/Ahilnandan/Hydration_Monitoring/blob/main/test_logs/GSR/Linearity_Test.jpg))

We used a 2 MegaOhm resistor with 130 KiloOhm increments to find linearity and fit the optimal curve for comparison.

![GSR Characterization Setup](image4.png)

We used this novel setup for characterizing the GSR sensors. This particular setup was used to measure the linearity of the GSR sensor.

**Resolution:**
The GSR sensor can take the set of whole numbers ranging from 0 to 2850.

**Drift:**

![GSR Drift Analysis](image5.png)

Even after operating for 12 hours no significant drift was found.

**Precision:**
On testing out with the same resistance after a cool down period it was found the readings could be replicated.

![GSR Precision Test](image6.png)

**Hysteresis:**
It is found that the GSR sensor lacks memory, which is a good thing. We increased the resistance and then decreased it to find that it traced its own path.

![GSR Hysteresis Test](image7.png)

## DHT22

The **DHT22** is a digital sensor used to measure **ambient temperature** and **relative humidity**. It operates using a capacitive humidity sensor and a thermistor for temperature measurement, providing calibrated digital output over a single-wire interface.

### Humidity Accuracy

**Setup:**

![DHT22 Humidity Test Setup](image8.png)

The above setup was used. A sealed container containing saturated table salt solution maintains a relative humidity of 75.5 percent, no matter what the ambient conditions are. Hence, we plotted the graphs for comparing the accuracy of DHT22 vs BME680.

![DHT22 vs BME680 Humidity Comparison](image9.png)

Above 70 percent it was noticed that the DHT22 and BME680 don't follow each other but below 70 percent they follow each other. There is a fixed deviation of 4.5 percent.

DHT22 has a humidity resolution of 0.1 percent.

### Temperature Accuracy

![DHT22 Temperature Test Setup](imageb.png)

We used a thermocol box to characterize the temperature accuracy.

![DHT22 vs BME680 Temperature Comparison](imagea.png)

DHT22 compared to the reference sensor BME680. The error is to be very minimal.

DHT22 has a resolution of 0.1 Celsius.

## DS18B20

The **DS18B20** is a digital thermometer that provides **precise temperature readings** via a **1-Wire** interface. It is ideal for measuring **body surface temperature** in hydration or physiological monitoring systems due to its compact size, waterproof variants, and digital accuracy.

![DS18B20 Temperature Analysis](imagec.png)

It has approximately 0.06 degrees resolution and 1.5 degrees upward drift.

## Field Trial

We wore the GSR sensor to validate the correlation between hydration and GSR trends varying ambient conditions.

As you notice in the graph below you will see a drop in the GSR after a rise. A correlation has been found between increasing GSR and increasing hydration and decreasing GSR and decreasing hydration, while keeping ambient conditions same.

The maximum in the graph is around 10 minutes after water intake.

![Field Trial Results](imaged.png)

## TRL 8 Goals

### Testing Plan (TRL-8 Readiness)

**Individual Sensor Characterization:** Test each sensor in controlled environments with known reference values, calculate mean error and standard deviation over extended periods (24-48 hours)

**Humidity Sensor Calibration:** Employ saturated salt solutions (NaCl at 25°C) to create known humidity reference points for sensor validation and drift assessment

**System Integration Testing:** Evaluate complete multi-sensor system performance under various environmental conditions, assess data fusion algorithm effectiveness sensor stability, power consumption, and system reliability

**Field Environment Simulation:** Test system performance in intended deployment conditions.

• All documentation uploaded: code, BOM, graphs, risk log

• Sensor-swap readiness statement included

• Noise & warm-up drift quantified

## Additional Resources

- [Grove GSR Sensor Documentation](https://wiki.seeedstudio.com/Grove-GSR_Sensor/)
- [ESP32 Development Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [Arduino IDE Setup for ESP32](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)
- [Hydration Monitoring Research Papers](https://scholar.google.com/scholar?q=hydration+monitoring+GSR+sensors)

## Conclusion (Indian Sensor Replacement Plan)

We have detailed the steps for characterizing the sensors in a cost-effective manner. The sensor which we believe is most ideal for replacement is the GSR sensor module as it is the most expensive and crucial component on whose replacement with an Indian sensor can significantly bring prices down. It is also the easiest to characterize.

The Indian Sensor should conform to these standards to work well for this solution.

| Parameter | Target Specification | Reasoning |
|-----------|---------------------|-----------|
| Resistance range | 10 kΩ – 1 MΩ | Covers full hydration signal range |
| Voltage output | 0.1 V – 2.8 V | Must match ESP32 ADC input |
| Noise (voltage) | ≤ ±10 mV | ~5–10 ADC units in 12-bit system |
| Resolution | ≥ 8-bit usable (100+ steps) | For trend, peak detection |
| Motion artifact | ≤ ±2% signal fluctuation | Depends on electrode quality |
| Long-term drift | ≤ ±0.1 V over 2 hours | Manageable with compensation |
