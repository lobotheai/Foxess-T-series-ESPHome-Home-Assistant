# FoxESS Solar Esphome
Read out FoxESS Inverters to Home Assistant using ESPHome.

![FoxESS](resources/images/foxess.png)

Designed for FoxESS T-series. The upstream project also lists F-series, but this repository has not independently verified those models.

Tested with:
- FoxESS T6-G3

Other models may use different firmware or protocol layouts. Their compatibility has not been verified by this project.

## Protocol support

| Function code | Data | Support |
|---|---|---|
| `0x01` | Device attributes and firmware versions | Yes |
| `0x02` | Realtime telemetry, energy counters, fault registers, master state, PV input count | Yes; reserved fields remain undocumented |
| `0x03` | 16-bit value at payload offset 36 (observed on T6-G3) | Optional raw counter; meaning not confirmed; other observed payload bytes are zero |
| `0x06` | Protocol version and inverter serial number | Yes |

Frames are checked for their declared length, checksum, and footer before parsing. Valid frames with unsupported function codes are logged at `DEBUG` and skipped.
The protocol version from heartbeat frames is retained and logged at `DEBUG` when it changes. Unknown versions are still accepted, using the current parser layout; version-specific offsets are not inferred automatically.
The `0x02` parser accepts the documented 152-byte layout and ignores other payload lengths. Frame timestamps are logged as raw 32-bit values. At `DEBUG`, the component also compares reported PV power with the voltage × current estimate and logs changes in the undocumented `Rev.1`–`Rev.10` fields and `0x03` payload.

```yaml
uart:
  tx_pin: GPIO16
  rx_pin: GPIO17
  baud_rate: 9600

sensor:
   - platform: foxess_solar
    phase_a:
      voltage:
        name: "FoxESS Phase A Voltage"
      current:
        name: "FoxESS Phase A Current"
      active_power:
        name: "FoxESS Phase A Power"
      frequency:
        name: "FoxESS Phase A Frequency"

    pv1:
      voltage:
        name: "FoxESS PV1 Voltage"
      current:
        name: "FoxESS PV1 Current"
      active_power:
        name: "FoxESS PV1 Power"

    total_energy_production:
      name: "Total Energy Production"
    energy_production_day:
      name: "Today Energy Production"

    generation_power:
      name: "FoxESS Generation Power"
    grid_power:
      name: "FoxESS Grid Power"
    loads_power:
      name: "FoxESS Loads Power"

    inverter_status:
      name: "FoxESS Status Code"

    inverter_temp:
      name: "FoxESS Inverter Temp"
    ambient_temp:
      name: "FoxESS Ambient Temp"
    boost_temp:
      name: "FoxESS Boost Temp"
```

## Instructions
- Copy the example YAML files to your ESPHome directory in Home Assistant
- Fill out individual data in the secrets.yaml file (SSID, password, etc.)
- Optionally comment sensors you don't need or uncomment sensors you do need

## Configuration variables:
- **uart_id** (*Optional*, ID): Manually specify the ID of the UART hub.
- **flow_control_pin** (*Optional*, Pin): The pin used to switch the direction of the MAX485 transceiver. Defaults to GPIO4.
- **inverter_status** (*Optional*): Status code of the inverter (0: offline, 1: online, 2: error, 99: waiting for response)
- **function_03_counter** (*Optional*): Raw unsigned 16-bit value from function `0x03`; observed to advance once per second on a T6-G3, but its origin and meaning are not confirmed. No unit or Home Assistant state class is assigned.
- **from_grid_yield_generation**, **feedin_generation_1**, **feedin_generation_2**, **consumption_generation_1**, **consumption_generation_2**, **loads_generation** (*Optional*): Additional cumulative energy counters from the `0x02` frame (0.1 kWh per raw count, published in kWh).
- **master_state**, **pv_input_number** (*Optional*): Raw unsigned values from the end of the `0x02` frame. The protocol lists these fields but does not document the master-state enum.
- **phase_a** (*Optional*): Sensors related to first phase of the inverter
  - **current** (*Optional*): Current flowing to the grid (A)
  - **voltage** (*Optional*): Grid voltage (V)
  - **active_power** (*Optional*): Power delivered to the grid (W)
  - **frequency** (*Optional*): Grid frequency (Hz)
- **phase_b** (*Optional*): Sensors related to second phase of the inverter
  - See **phase_a**
- **phase_c** (*Optional*): Sensors related to third phase of the inverter
  - See **phase_a**

- **pv1** (*Optional*): Sensors related to the first group PV cells
  - **current** (*Optional*): Current flowing from PV1 (A)
  - **voltage** (*Optional*): PV1 Voltage (V)
  - **active_power** (*Optional*): Estimated PV1 power, calculated as voltage × current (W)
- **pv2** (*Optional*): Sensors related to the second group PV cells
  - See **pv1**
- **pv3** (*Optional*): Sensors related to the third group PV cells
  - See **pv1**
- **pv4** (*Optional*): Sensors related to the fourth group PV cells
  - See **pv1**

- **total_energy_production** (*Optional*): Total energy produced during lifetime of inverter (kWh)
- **energy_production_day** (*Optional*): Total energy produced today (kWh)
- **generation_power** (*Optional*): Current total power generation (W)
- **grid_power** (*Optional*): Current export power to grid. Required to have a meter connected to the inverter (SDM230). (W)
- **loads_power** (*Optional*): Current loads power (W)
- **inverter_temp** (*Optional*): Inverter temperature (°C)
- **boost_temp** (*Optional*): Boost temperature (°C)
- **ambient_temp** (*Optional*): Ambient temperature (°C)
- **eps_voltage**, **eps_current**, **eps_power** (*Optional*): EPS output voltage (V), current (A), and power (W)
- **fault_registers** (*Optional*): Text summary of the eight raw 32-bit fault registers. Values are not decoded into fault names.
- **master_version**, **slave_version**, **manager_version**, **afg_version** (*Optional*): Firmware version strings reported by the inverter
- **device_factory**, **device_type**, **device_model** (*Optional*): Device identification fields. Known factory IDs are exposed as names; unknown IDs retain their numeric value in the text.
- **device_capacity** (*Optional*): Rated device capacity (W)
- **protocol_version** (*Optional*): FoxESS protocol version from heartbeat frames
- **serial_number** (*Optional*): Inverter serial number from heartbeat frames. Only configure this if you want to expose it as an entity in Home Assistant. ESPHome `VERBOSE` logging may also print published sensor values; use `DEBUG` for routine operation.

New entities are optional. Add only the keys you need under `sensor: - platform: foxess_solar`; see [foxess-inverter.yaml.example](foxess-inverter.yaml.example) for a complete example.

## Troubleshooting

- **No realtime values:** Check RS485 A/B wiring, the transceiver direction pin, UART pins, and the inverter's communication settings. The component expects 9600 baud by default in the example configuration.
- **`checksum mismatch` or `bad footer`:** These indicate an invalid/corrupted frame. Check the wiring, grounding, baud rate, and electrical noise on the RS485 bus.
- **`Unsupported FoxESS function code`:** The frame passed basic validation but its data type is not decoded yet. It is skipped so subsequent frames can still be processed.
- **No `0x01` or `0x06` entities:** Configure the corresponding optional text sensors and confirm that frames with those function codes appear in the debug log.
- **Inverter shows `Error`:** Check the `fault_registers` text sensor for raw register values. Their bits are not currently mapped to human-readable fault names.

For a short diagnostic session, set `logger.level: DEBUG` (or `VERBOSE` to include sensor state changes). Avoid leaving `VERBOSE` enabled during routine operation because it can affect ESP8266 performance and may log configured text sensor values such as the serial number.

## Hardware setup
The hardware setup including a wiring diagram can be found in the [Wiki](https://github.com/assembly12/Foxess-T-series-ESPHome-Home-Assistant/wiki/Hardware-setup).

Designing a custom pcb and enclosure is next on my to do list. I'll update here with the corresponding gerber and stl files when done.

Other protocol fields may be present but are not exposed until their meaning and scaling are confirmed.

Some basic electronics skills (like soldering) are needed to realize this project. I do not take any responsibility for the use of this custom component or anything that it written down in this repository. Use at your own risk.
