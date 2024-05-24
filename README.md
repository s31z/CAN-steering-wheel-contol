# CAN-steering-wheel-control
An Arduino-based, easy to configure CAN-to-resistance interface for translating steering wheel commands to resistance signals that most aftermarket radios understand.

My example hardware configuration is based on the following components:
- [Arduino Nano Every](https://store.arduino.cc/products/arduino-nano-every)
- Digital Potentiometer (SPI-configurable) [MCP4151-104E/P](https://www.microchip.com/en-us/product/mcp4151)
- CAN module (also SPI-configurable) [MCP2515](https://www.reichelt.de/entwicklerboards-can-modul-mcp2515-mcp2562-debo-can-modul-p239277.html) with integrated oscillator and 120 Ohm terminating resistor
- some spring clamp terminals
