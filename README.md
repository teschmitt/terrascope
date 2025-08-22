# terrascope

A modular and efficient firmware for LoRa-enabled mesh sensor nodes running on Zephyr RTOS.

This project creates a scalable, low-power mesh of sensor nodes that collect environmental data and reliably forward it to gateway nodes, which in turn upload it to cloud services. Designed for robust rural, industrial, or remote environmental monitoring.

Features:

📡 LoRa Mesh Networking – Multi-hop routing and relay of sensor data across unreliable or sparse networks.

🌡 Sensor Support – Easy integration with common environmental sensors (e.g., temperature, humidity, air quality).

🔋 Low Power – Sleep modes and data batching for extended battery life.

🎯 Gateway Bridging – Select nodes act as gateways, relaying multi-node data to the cloud via Wi-Fi, Ethernet, or Cellular.

🛠 Modular Architecture – Clear separation of mesh, sensor, application logic, and gateway uplink.

## Architecture

Sensor nodes self-organize into a mesh network using LoRa radios. Data is collected from onboard sensors and sent through one or more hops via peer nodes, reaching a gateway node. Gateways forward aggregated data to a cloud endpoint for visualization and storage.
