import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, text_sensor, uart
from esphome.const import (
    CONF_ACTIVE_POWER,
    CONF_CURRENT,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_PHASE_A,
    CONF_PHASE_B,
    CONF_PHASE_C,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)

CONF_FLOW_CONTROL_PIN = "flow_control_pin"
CONF_ENERGY_PRODUCTION_DAY = "energy_production_day"
CONF_TOTAL_ENERGY_PRODUCTION = "total_energy_production"
CONF_PV1 = "pv1"
CONF_PV2 = "pv2"
CONF_PV3 = "pv3"
CONF_PV4 = "pv4"

CONF_LOADS_POWER = "loads_power"
CONF_GRID_POWER = "grid_power"
CONF_GENERATION_POWER = "generation_power"

CONF_INVERTER_STATUS = "inverter_status"
CONF_INVERTER_TEMP = "inverter_temp"
CONF_BOOST_TEMP = "boost_temp"
CONF_AMBIENT_TEMP = "ambient_temp"
CONF_EPS_VOLTAGE = "eps_voltage"
CONF_EPS_CURRENT = "eps_current"
CONF_EPS_POWER = "eps_power"
CONF_FAULT_REGISTERS = "fault_registers"
CONF_MASTER_VERSION = "master_version"
CONF_SLAVE_VERSION = "slave_version"
CONF_MANAGER_VERSION = "manager_version"
CONF_DEVICE_FACTORY = "device_factory"
CONF_DEVICE_TYPE = "device_type"
CONF_DEVICE_MODEL = "device_model"
CONF_DEVICE_CAPACITY = "device_capacity"
CONF_AFCI_VERSION = "afci_version"
CONF_PROTOCOL_VERSION = "protocol_version"
CONF_SERIAL_NUMBER = "serial_number"
CONF_FUNCTION_03_COUNTER = "function_03_counter"
CONF_FROM_GRID_YIELD_GENERATION = "from_grid_yield_generation"
CONF_FEEDIN_GENERATION_1 = "feedin_generation_1"
CONF_FEEDIN_GENERATION_2 = "feedin_generation_2"
CONF_CONSUMPTION_GENERATION_1 = "consumption_generation_1"
CONF_CONSUMPTION_GENERATION_2 = "consumption_generation_2"
CONF_LOADS_GENERATION = "loads_generation"
CONF_MASTER_STATE = "master_state"
CONF_PV_INPUT_NUMBER = "pv_input_number"

DEPENDENCIES = ["uart"]

foxess_solar_ns = cg.esphome_ns.namespace("foxess_solar")
FoxessSolar = foxess_solar_ns.class_("FoxessSolar", cg.PollingComponent, uart.UARTDevice)

PHASE_SENSORS = {
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_ACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_FREQUENCY: sensor.sensor_schema(
        unit_of_measurement=UNIT_HERTZ,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_FREQUENCY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

PV_SENSORS = {
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_ACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

PHASE_SCHEMA = cv.Schema({cv.Optional(sensor): schema for sensor, schema in PHASE_SENSORS.items()})
PV_SCHEMA = cv.Schema({cv.Optional(sensor): schema for sensor, schema in PV_SENSORS.items()})

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FoxessSolar),
            cv.Optional(CONF_FLOW_CONTROL_PIN, default="GPIO4"): pins.gpio_output_pin_schema,
            cv.Optional(CONF_PHASE_A): PHASE_SCHEMA,
            cv.Optional(CONF_PHASE_B): PHASE_SCHEMA,
            cv.Optional(CONF_PHASE_C): PHASE_SCHEMA,
            cv.Optional(CONF_PV1): PV_SCHEMA,
            cv.Optional(CONF_PV2): PV_SCHEMA,
            cv.Optional(CONF_PV3): PV_SCHEMA,
            cv.Optional(CONF_PV4): PV_SCHEMA,
            cv.Optional(CONF_INVERTER_STATUS): sensor.sensor_schema(),
            cv.Optional(CONF_FUNCTION_03_COUNTER): sensor.sensor_schema(accuracy_decimals=0),
            cv.Optional(CONF_FROM_GRID_YIELD_GENERATION): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_FEEDIN_GENERATION_1): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_FEEDIN_GENERATION_2): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_CONSUMPTION_GENERATION_1): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_CONSUMPTION_GENERATION_2): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_LOADS_GENERATION): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_MASTER_STATE): sensor.sensor_schema(accuracy_decimals=0),
            cv.Optional(CONF_PV_INPUT_NUMBER): sensor.sensor_schema(accuracy_decimals=0),
            cv.Optional(CONF_LOADS_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_GRID_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_GENERATION_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_INVERTER_TEMP): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AMBIENT_TEMP): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_EPS_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_EPS_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_EPS_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_FAULT_REGISTERS): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_MASTER_VERSION): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_SLAVE_VERSION): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_MANAGER_VERSION): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_DEVICE_FACTORY): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_DEVICE_TYPE): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_DEVICE_MODEL): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_AFCI_VERSION): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_PROTOCOL_VERSION): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_DEVICE_CAPACITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
            ),
            cv.Optional(CONF_BOOST_TEMP): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ENERGY_PRODUCTION_DAY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=1,
                icon="mdi:solar-power",
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_TOTAL_ENERGY_PRODUCTION): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=1,
                icon="mdi:solar-power",
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_FAULT_REGISTERS in config:
        fault_registers = await text_sensor.new_text_sensor(config[CONF_FAULT_REGISTERS])
        cg.add(var.set_fault_registers_sensor(fault_registers))

    for key in [
        CONF_MASTER_VERSION,
        CONF_SLAVE_VERSION,
        CONF_MANAGER_VERSION,
        CONF_DEVICE_FACTORY,
        CONF_DEVICE_TYPE,
        CONF_DEVICE_MODEL,
        CONF_AFCI_VERSION,
        CONF_PROTOCOL_VERSION,
        CONF_SERIAL_NUMBER,
    ]:
        if key in config:
            sens = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(var, f"set_{key}_sensor")(sens))

    # flow control pin
    flow_control_pin = await cg.gpio_pin_expression(config[CONF_FLOW_CONTROL_PIN])
    cg.add(var.set_fc_pin(flow_control_pin))

    # simple top-level sensors
    for key in [
        CONF_INVERTER_STATUS,
        CONF_FUNCTION_03_COUNTER,
        CONF_FROM_GRID_YIELD_GENERATION,
        CONF_FEEDIN_GENERATION_1,
        CONF_FEEDIN_GENERATION_2,
        CONF_CONSUMPTION_GENERATION_1,
        CONF_CONSUMPTION_GENERATION_2,
        CONF_LOADS_GENERATION,
        CONF_MASTER_STATE,
        CONF_PV_INPUT_NUMBER,
        CONF_LOADS_POWER,
        CONF_GRID_POWER,
        CONF_GENERATION_POWER,
        CONF_INVERTER_TEMP,
        CONF_AMBIENT_TEMP,
        CONF_EPS_VOLTAGE,
        CONF_EPS_CURRENT,
        CONF_EPS_POWER,
        CONF_DEVICE_CAPACITY,
        CONF_BOOST_TEMP,
        CONF_ENERGY_PRODUCTION_DAY,
        CONF_TOTAL_ENERGY_PRODUCTION,
    ]:
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, f"set_{key}_sensor")(sens))

    # phases (A, B, C)
    for i, phase_key in enumerate([CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C]):
        if phase_key not in config:
            continue
        phase_conf = config[phase_key]
        for sensor_type in PHASE_SENSORS:
            if sensor_type in phase_conf:
                sens = await sensor.new_sensor(phase_conf[sensor_type])
                cg.add(getattr(var, f"set_phase_{sensor_type}_sensor")(i, sens))

    # PVs (1..4)
    for i, pv_key in enumerate([CONF_PV1, CONF_PV2, CONF_PV3, CONF_PV4]):
        if pv_key not in config:
            continue
        pv_conf = config[pv_key]
        for sensor_type in PV_SENSORS:
            if sensor_type in pv_conf:
                sens = await sensor.new_sensor(pv_conf[sensor_type])
                cg.add(getattr(var, f"set_pv_{sensor_type}_sensor")(i, sens))
