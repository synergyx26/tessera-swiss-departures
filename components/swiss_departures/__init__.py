"""Swiss departures (SBB): a tile with the next departures of a Swiss stop, counted down on the screen's own clock.

The add-on fetches the times from transport.opendata.ch (the manifest's `fetch`); this component only draws them.
register_plugin() takes the id, version, tile memory and the screen's texts from the manifest and translations/."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display
from esphome.const import CONF_ID

# The core component every Tessera screen has; the plugin API lives in it.
DEPENDENCIES = ["smart_display"]

swiss_departures_ns = cg.esphome_ns.namespace("swiss_departures")
SwissDepartures = swiss_departures_ns.class_("SwissDepartures", cg.Component)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(SwissDepartures)}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
