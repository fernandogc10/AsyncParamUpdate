# AsyncParamUpdate Library

The `AsyncParamUpdate` library enables hot parameter updates for microcontroller projects, making it especially useful for IoT devices. It facilitates easy parameter management and updates through MQTT and a web application, ideal for various IoT applications requiring remote configuration and updates.

## Features

- Hot parameter updates without interrupting main functions.
- MQTT integration for efficient, real-time communication.
- Web aplication compatibility for easy parameter configuration. [Web-App](https://github.com/fernandogc10/RemoteDevBoardConfiguratorWebApp)
- Suitable for ESP32 and other compatible microcontrollers.

## Installation

**Arduino IDE:**
1. Download the latest release from this repository.
2. In Arduino IDE, navigate to `Sketch > Include Library > Add .ZIP Library...` and select the downloaded file.

**PlatformIO:**

For PlatformIO projects, add `AsyncParamUpdate` to your `platformio.ini` file under `lib_deps`:

```ini
lib_deps =
  https://github.com/fernandogc10/AsyncParamUpdate.git
```
## Usage

To use the AsyncParamUpdate library in your project:

1. Include the library at the top of your sketch:

    ```cpp
    #include "AsyncParamUpdate.h"
    ```

2. Create an instance of `AsyncParamUpdate` with your configuration:

    ```cpp
    AsyncParamUpdate asyncParamUpdater("YourWiFiSSID", "YourWiFiPassword", "MQTTHost", MQTTPort, "MQTTUser", "MQTTPassword", "DeviceName", true);
    ```

3. Initialize the instance in the `setup()` function and add your parameters:

    ```cpp
    void setup() {
      asyncParamUpdater.begin();
      
      int yourIntParam = 0;
      asyncParamUpdater.addParameter("yourIntParam", yourIntParam);
    }
    ```

4. Use the `loop()` function for regular operations:

    ```cpp
    void loop() {
      // Your code here
    }
    ```

See [`AsyncParamUpdateExample.cpp`](https://github.com/fernandogc10/AsyncParamUpdate/blob/main/examples/AsyncParamUpdateExample.cpp) for a complete example.

## Example

Check out the [`AsyncParamUpdateExample.cpp`](https://github.com/fernandogc10/AsyncParamUpdate/blob/main/examples/AsyncParamUpdateExample.cpp) file in the examples directory for a detailed example of how to use the AsyncParamUpdate library in a project.

## LoRa Integration

The `AsyncParamUpdate` library also supports LoRa for long-range, low-power wireless communication. This makes it suitable for scenarios where WiFi connectivity is not available or practical.

### LoRa Node Mode

To configure the `AsyncParamUpdate` library in **LoRa node mode**, where the device communicates over LoRa without using WiFi or MQTT, follow these steps:

1. **Include the library at the top of your sketch:**

    ```cpp
    #include "AsyncParamUpdate.h"
    ```

2. **Create an instance of `AsyncParamUpdate` for LoRa configuration:**

    ```cpp
    AsyncParamUpdate asyncParamUpdater("DeviceName");
    ```

   This constructor sets up the library in **LoRa node mode**, where the device will use LoRa for communication.

3. **Initialize the instance in the `setup()` function and add your parameters:**

    ```cpp
    void setup() {
      // Initialize AsyncParamUpdate for LoRa Node Mode
      asyncParamUpdater.begin();

      int yourIntParam = 0;
      asyncParamUpdater.addParameter("yourIntParam", yourIntParam);
    }
    ```
### LoRa Gateway Mode

To configure the `AsyncParamUpdate` library in **LoRa gateway mode**, where the device acts as a gateway for LoRa communication, follow these steps:

1. **Include the library at the top of your sketch:**

    ```cpp
    #include "AsyncParamUpdate.h"
    ```

2. **Create an instance of `AsyncParamUpdate` for LoRa configuration and set it as a gateway:**

    ```cpp
    AsyncParamUpdate asyncParamUpdater;
    ```

   The `setGateway()` function sets the library to operate in **LoRa gateway mode**, allowing the device to listen for incoming LoRa messages and serve as a gateway.

3. **Initialize the instance in the `setup()` function and add your parameters:**

    ```cpp
    void setup() {
      // Initialize AsyncParamUpdate for LoRa Gateway Mode
      asyncParamUpdater.setGateway("YourWiFiSSID", "YourWiFiPassword", "MQTTHost", MQTTPort, "MQTTUser", "MQTTPassword");
    }
    ```




