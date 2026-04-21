import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@saikhurana98"]

lg_climate_ns = cg.esphome_ns.namespace("lg_climate")
LGClimate = lg_climate_ns.class_("LGClimate", climate_ir.ClimateIR)

CONF_HEADER_HIGH = "header_high"
CONF_HEADER_LOW = "header_low"
CONF_BIT_HIGH = "bit_high"
CONF_BIT_ONE_LOW = "bit_one_low"
CONF_BIT_ZERO_LOW = "bit_zero_low"

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(LGClimate).extend(
    {
        cv.Optional(CONF_HEADER_HIGH, default=3200): cv.uint32_t,
        cv.Optional(CONF_HEADER_LOW, default=9900): cv.uint32_t,
        cv.Optional(CONF_BIT_HIGH, default=500): cv.uint32_t,
        cv.Optional(CONF_BIT_ONE_LOW, default=1550): cv.uint32_t,
        cv.Optional(CONF_BIT_ZERO_LOW, default=520): cv.uint32_t,
    }
)


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)

    cg.add(var.set_header_high(config[CONF_HEADER_HIGH]))
    cg.add(var.set_header_low(config[CONF_HEADER_LOW]))
    cg.add(var.set_bit_high(config[CONF_BIT_HIGH]))
    cg.add(var.set_bit_one_low(config[CONF_BIT_ONE_LOW]))
    cg.add(var.set_bit_zero_low(config[CONF_BIT_ZERO_LOW]))
