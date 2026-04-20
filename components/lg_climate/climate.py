import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import CONF_ID

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@notkanishk"]

lg_climate_ns = cg.esphome_ns.namespace("lg_climate")
LGClimate = lg_climate_ns.class_("LGClimate", climate_ir.ClimateIR)

CONF_HEADER_HIGH = "header_high"
CONF_HEADER_LOW = "header_low"
CONF_BIT_HIGH = "bit_high"
CONF_BIT_ONE_LOW = "bit_one_low"
CONF_BIT_ZERO_LOW = "bit_zero_low"

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(LGClimate),
        cv.Optional(CONF_HEADER_HIGH, default=8000): cv.uint32_t,
        cv.Optional(CONF_HEADER_LOW, default=4000): cv.uint32_t,
        cv.Optional(CONF_BIT_HIGH, default=600): cv.uint32_t,
        cv.Optional(CONF_BIT_ONE_LOW, default=1600): cv.uint32_t,
        cv.Optional(CONF_BIT_ZERO_LOW, default=550): cv.uint32_t,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)

    cg.add(var.set_header_high(config[CONF_HEADER_HIGH]))
    cg.add(var.set_header_low(config[CONF_HEADER_LOW]))
    cg.add(var.set_bit_high(config[CONF_BIT_HIGH]))
    cg.add(var.set_bit_one_low(config[CONF_BIT_ONE_LOW]))
    cg.add(var.set_bit_zero_low(config[CONF_BIT_ZERO_LOW]))
