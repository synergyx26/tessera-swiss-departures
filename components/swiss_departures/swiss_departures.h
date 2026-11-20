#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::swiss_departures {

// What kind of vehicle a line is, for the colours by transport.
enum Kind : uint8_t { RAIL, TRAM, BUS, BOAT, CABLE };

// How a tile and its card look: Tessera's own blue, SBB's red line badges, or a dark station board.
enum class Look : uint8_t { TESSERA, RED, BOARD };

// One departure or connection as the add-on maps it (tessera-plugin.yaml, fetch "departures" or "connections"), in
// fixed fields: a list of them is a tessera::List, one block in PSRAM (docs/FIRMWARE_API.md, "Memory").
struct Departure {
  char badge[20] = {};   // what the line's badge says: "S6", "IC5", "14", "m2", "BAT"
  char to[52] = {};      // where it goes; for a connection, when it arrives and how many changes
  char track[20] = {};   // the platform or stand it leaves from, the new one when it changed
  bool moved = false;    // the platform changed
  uint8_t kind = RAIL;
  uint32_t plan = 0;     // when it leaves by the timetable, seconds since 1970
  uint32_t at = 0;       // when it leaves: the timetable's time plus the delay
  uint32_t arrive = 0;   // a connection: when it arrives by the timetable
  int late = 0;          // minutes late
  int changes = 0;       // a connection: how many changes
  int load = 0;          // a connection: how full second class is, 1 (seats free) to 3 (very busy), 0 unknown
};

class ListCard;

// The plugin: two tile types, "next" (the departures of a stop) and "trip" (the connections from one stop to another),
// and one card, "list", that a tap on either opens: every departure, or every connection with its legs.
class SwissDepartures : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void on_message(JsonObjectConst message) override;

  // What the tile a finger tapped hands the card it opens (a card gets the tile's data, not its options).
  bool trip = false;                  // connections, or departures
  char from_id[16] = {}, to_id[16] = {};
  uint32_t pick = 0;                  // the connection tapped on the tile (its timetable time), 0 for the list
  int walk = 0;
  bool late = true, track = true, direct = false, kinds = false;
  Look look = Look::TESSERA;
  tessera::List<Departure> shown;     // the tile's departures or connections when it was tapped
  ListCard *card = nullptr;           // the card on the glass, for the answers of the app
};

}  // namespace esphome::swiss_departures
