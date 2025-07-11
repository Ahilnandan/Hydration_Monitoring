# Smart Hydration Monitoring Using Skin Resistance and Environmental Sensors

**Note:** Throughout our testing and calibration process, we used the **Bosch BME680** as a reference sensor for temperature and humidity. It helped us validate accuracy, drift, and other sensor behaviors in controlled conditions.

## What is Hydra Track?

Hydra Track is our attempt to build a **simple, portable, and reliable hydration monitoring system** using low-cost sensors and microcontrollers. It continuously monitors:

- **Skin impedance (GSR)** to estimate hydration trends
- **Body temperature (DS18B20)**
- **Ambient temperature and humidity (DHT22)**

The device classifies hydration levels into:

- **Hydrated**
- **Partially Hydrated**
- **Dehydrated**

The system displays this in real-time on an OLED screen and gives alerts through a buzzer. It also hosts a local website on the ESP32 for live data viewing.

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



The above graph for GSR accuracy was plotted by using a known resistance value of 2MΩ and the expected and observed GSR values were found considering the internal resistance of the Grove GSR's voltage divider and max reading of 2850 the GSR is calculated for complete conduction.

109 turns out to be the fudge factor that needs to be added to make the Observed GSR readings more accurate.

The following statistical parameters were found:

1. Standard Deviation: -10.5
2. Average: 245.5 (After Correction)
3. RMS Error: 15.93


We used a 2 MegaOhm resistor with 130 KiloOhm increments to find linearity and fit the optimal curve for comparison.



We used this novel setup for characterizing the GSR sensors. This particular setup was used to measure the linearity of the GSR sensor.

**Resolution:**
The GSR sensor can take the set of whole numbers ranging from 0 to 2850.

**Drift:**


Even after operating for 12 hours no significant drift was found.

**Precision:**
On testing out with the same resistance after a cool down period it was found the readings could be replicated.


**Hysteresis:**
It is found that the GSR sensor lacks memory, which is a good thing. We increased the resistance and then decreased it to find that it traced its own path.



## DHT22

The **DHT22** is a digital sensor used to measure **ambient temperature** and **relative humidity**. It operates using a capacitive humidity sensor and a thermistor for temperature measurement, providing calibrated digital output over a single-wire interface.

### Humidity Accuracy

**Setup:**


The above setup was used. A sealed container containing saturated table salt solution maintains a relative humidity of 75.5 percent, no matter what the ambient conditions are. Hence, we plotted the graphs for comparing the accuracy of DHT22 vs BME680.

Above 70 percent it was noticed that the DHT22 and BME680 don't follow each other but below 70 percent they follow each other. There is a fixed deviation of 4.5 percent.

DHT22 has a humidity resolution of 0.1 percent.

### Temperature Accuracy



We used a thermocol box to characterize the temperature accuracy.


DHT22 compared to the reference sensor BME680. The error is to be very minimal, around [value not specified in original].

DHT22 has a resolution of 0.1 Celsius.

## DS18B20

The **DS18B20** is a digital thermometer that provides **precise temperature readings** via a **1-Wire** interface. It is ideal for measuring **body surface temperature** in hydration or physiological monitoring systems due to its compact size, waterproof variants, and digital accuracy.



It has approximately 0.06 degrees resolution and 1.5 degrees upward drift.

## Field Trial

We wore the GSR sensor to validate the correlation between hydration and GSR trends varying ambient conditions.

As you notice in the graph below you will see a drop in the GSR after a rise. A correlation has been found between increasing GSR and increasing hydration and decreasing GSR and decreasing hydration, while keeping ambient conditions same.

The maximum in the graph is around 10 minutes after water intake.


## TRL 8 Goals

### Testing Plan (TRL-8 Readiness)

**Individual Sensor Characterization:** Test each sensor in controlled environments with known reference values, calculate mean error and standard deviation over extended periods (24-48 hours)

**Humidity Sensor Calibration:** Employ saturated salt solutions (NaCl at 25°C) to create known humidity reference points for sensor validation and drift assessment

**System Integration Testing:** Evaluate complete multi-sensor system performance under various environmental conditions, assess data fusion algorithm effectiveness sensor stability, power consumption, and system reliability

**Field Environment Simulation:** Test system performance in intended deployment conditions.

• All documentation uploaded: code, BOM, graphs, risk log
• Sensor-swap readiness statement included
• Noise & warm-up drift quantified

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
