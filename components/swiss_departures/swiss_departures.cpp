#include "swiss_departures.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace esphome::swiss_departures {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

static constexpr uint32_t ARROW_RIGHT = 0xF0054;   // mdi:arrow-right, of Tessera's icon set
static constexpr int MAX_LEGS = 5;                 // the legs a connection's card shows at most

static bool one_of(const char *cat, std::initializer_list<const char *> list) {
  for (const char *c : list)
    if (std::strcmp(cat, c) == 0) return true;
  return false;
}

// transport.opendata.ch's categories by kind of vehicle. Everything else is a train.
static Kind kind_of(const char *cat) {
  if (one_of(cat, {"T", "TRAM", "M", "NFT", "TN"})) return TRAM;
  if (one_of(cat, {"B", "BUS", "NFB", "NFO", "BN", "EXB", "KB", "EV", "N"})) return BUS;
  if (one_of(cat, {"BAT", "FAE"})) return BOAT;
  if (one_of(cat, {"FUN", "PB", "GB", "SL", "LB", "ASC"})) return CABLE;
  return RAIL;
}

// A number of four digits or more is a journey's, not a line's (a lake boat's "3602"): the badge says the category.
static bool journey_number(const char *num) {
  const size_t n = std::strlen(num);
  if (n < 4) return false;
  for (size_t i = 0; i < n; ++i)
    if (num[i] < '0' || num[i] > '9') return false;
  return true;
}

// What a line's badge says: a train its category and number ("S" and "6" is "S6"), anything else its number ("14").
static void make_badge(char *out, size_t size, const char *cat, const char *num) {
  if (!*num || journey_number(num)) snprintf(out, size, "%s", cat);
  else if (kind_of(cat) != RAIL || std::strncmp(num, cat, std::strlen(cat)) == 0) snprintf(out, size, "%s", num);
  else snprintf(out, size, "%s%s", cat, num);
}

static std::string joined(const std::string &a, const std::string &b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  return a + " · " + b;
}

static Look look_of(const char *look) {
  if (std::strcmp(look, "red") == 0) return Look::RED;
  if (std::strcmp(look, "board") == 0) return Look::BOARD;
  return Look::TESSERA;
}

// A pick of a choice found by searching is {"value", "label"}; an option nobody filled in is "".
static std::string pick_value(JsonVariantConst v) { return v.is<JsonObjectConst>() ? std::string(v["value"] | "") : ""; }
static std::string pick_label(JsonVariantConst v) { return v.is<JsonObjectConst>() ? std::string(v["label"] | "") : ""; }

// One item of the departures or connections fetch.
static bool parse(JsonObjectConst item, bool trip, const tessera::Plugin *plugin, Departure &d) {
  const char *cat = item["cat"] | "", *line = item["line"] | "";
  if (!*cat && !*line) {   // a connection that starts with a walk: the badge of its first train
    cat = item["cat2"] | "";
    line = item["line2"] | "";
  }
  make_badge(d.badge, sizeof d.badge, cat, line);
  d.kind = kind_of(cat);
  const char *track = item["track"] | "";
  const char *moved = item["moved"] | "";
  d.moved = *moved && std::strcmp(moved, track) != 0;
  snprintf(d.track, sizeof d.track, "%s", d.moved ? moved : track);
  d.plan = item["plan"] | 0u;
  d.late = std::max(0, static_cast<int>(item["delay"] | 0.0f));
  d.at = d.plan + static_cast<uint32_t>(d.late) * 60;
  if (trip) {
    d.changes = item["changes"] | 0;
    d.arrive = item["arrive"] | 0u;
    d.load = item["load"] | 0;
    std::string text = d.arrive ? tessera::fill(plugin->text("arrives"), "t", tessera::clock_text(d.arrive)) : "";
    if (d.changes > 0) text = joined(text, tessera::format(plugin->text("changes"), static_cast<long>(d.changes)));
    snprintf(d.to, sizeof d.to, "%s", text.c_str());
  } else {
    snprintf(d.to, sizeof d.to, "%s", item["to"] | "");
  }
  return d.plan != 0;
}

static void parse_all(JsonObjectConst data, bool trip, const tessera::Plugin *plugin, tessera::List<Departure> &out) {
  out.clear();
  for (JsonObjectConst item : data["items"].as<JsonArrayConst>()) {
    Departure d;
    if (parse(item, trip, plugin, d)) out.push_back(d);
  }
  std::sort(out.data(), out.data() + out.size(), [](const Departure &a, const Departure &b) { return a.at < b.at; });
}

// The rows of departures a tile's list and the card draw alike: the line's badge, where it goes, its platform and the
// countdown, each part only as wide as what it says. And the look they share: which roles the words take and what
// colour each badge is.
class Rows {
 public:
  explicit Rows(SwissDepartures *owner) : owner_(owner) {}

 protected:
  struct Row {
    lv_obj_t *badge{}, *number{}, *to{}, *track{}, *when{};
    int y = 0;
    bool list = true;
  };

  void metrics(int width) {
    width_ = width;
    head_ = std::max(ui::line_height(Font::TITLE), ui::line_height(Font::BODY)) + ui::px(4);
    gap_ = ui::px(ui::large() ? 8 : 5);
  }

  // The words: white on the station board's dark ground, the screen's ink elsewhere. Their quieter kind is slate on a
  // card coloured for "Leave now", where muted would be too soft.
  theme::Role ink() const { return look_ == Look::BOARD ? theme::INSTRUMENT_INK : theme::INK; }
  theme::Role muted() const {
    return look_ == Look::BOARD ? theme::INSTRUMENT_MUTED : tinted_ ? theme::SLATE : theme::MUTED;
  }

  // A badge in its look's colour, or its transport's: trains red, trams and metros blue, buses yellow (as the
  // PostAuto), boats purple, cable cars green. Dark words on the light fills, white on the others.
  void paint_badge(lv_obj_t *badge, lv_obj_t *number, uint8_t kind) {
    if (kinds_) {
      switch (kind) {
        case TRAM: return fill(badge, number, Tone::NORMAL, theme::MARK_BLUE, theme::ON_ACCENT);
        case BUS: return fill(badge, number, Tone::NORMAL, theme::MARK_AMBER, theme::INSTRUMENT);
        case BOAT: return fill(badge, number, Tone::NORMAL, theme::MARK_PURPLE, theme::ON_ACCENT);
        case CABLE: return fill(badge, number, Tone::NORMAL, theme::MARK_GREEN, theme::INSTRUMENT);
        default: return fill(badge, number, Tone::ALERT, theme::ACCENT, theme::ON_ACCENT);
      }
    }
    switch (look_) {
      case Look::RED: return fill(badge, number, Tone::ALERT, theme::ACCENT, theme::ON_ACCENT);
      case Look::BOARD: return fill(badge, number, Tone::NORMAL, theme::INSTRUMENT_INK, theme::INSTRUMENT);
      default: return fill(badge, number, Tone::NORMAL, theme::ACCENT, theme::ON_ACCENT);
    }
  }

  static void fill(lv_obj_t *badge, lv_obj_t *number, Tone tone, theme::Role role, theme::Role words) {
    ui::set_fill(badge, tone, role);
    ui::set_color(number, words);
  }

  void make_badge_parts(lv_obj_t *parent, lv_obj_t *&badge, lv_obj_t *&number) {
    badge = ui::block(parent, theme::ACCENT);
    number = ui::label(badge, Font::TITLE, theme::ON_ACCENT);
    lv_label_set_long_mode(number, LV_LABEL_LONG_CLIP);
    lv_obj_center(number);
  }

  // The badge as wide as its line's name at (x, y), in its colour; its width, 0 when there is no name.
  int place_badge(lv_obj_t *badge, lv_obj_t *number, const std::string &line, int x, int y, uint8_t kind = RAIL) {
    const bool shown = !line.empty();
    lv_obj_set_style_opa(badge, shown ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    if (shown) paint_badge(badge, number, kind);
    ui::set_text(number, line);
    const int pad = ui::px(ui::large() ? 8 : 5);
    const int w = shown ? std::min(width_ / 2, ui::text_width(line, Font::TITLE) + 2 * pad) : 0;
    lv_obj_set_size(badge, std::max(1, w), head_);
    lv_obj_set_pos(badge, x, y);
    lv_obj_center(number);
    return w;
  }

  void make_row(Row &r, lv_obj_t *parent, int y, bool list) {
    r.y = y;
    r.list = list;
    make_badge_parts(parent, r.badge, r.number);
    r.to = ui::label(parent, Font::BODY, list ? ink() : muted());
    if (list) {
      r.track = ui::label(parent, Font::BODY, muted());
      r.when = ui::label(parent, Font::TITLE, ink());
      lv_obj_set_style_text_align(r.when, LV_TEXT_ALIGN_RIGHT, 0);
    }
  }

  // The badge, the countdown at the right, the platform left of it, and where it goes in between. A part with nothing
  // to say takes no room.
  void place_row(Row &r, const std::string &line, const std::string &to, const std::string &track, bool moved,
                 const std::string &when, uint8_t kind = RAIL) {
    const int badge_w = place_badge(r.badge, r.number, line, 0, r.y, kind);
    const int x = badge_w ? badge_w + ui::px(6) : 0;
    int right = width_;
    if (r.when) {
      const int when_w = when.empty() ? 0 : ui::text_width(when, Font::TITLE) + ui::px(2);
      ui::set_text(r.when, when);
      ui::set_color(r.when, ink());
      lv_obj_set_size(r.when, std::max(1, when_w), ui::line_height(Font::TITLE));
      lv_obj_set_pos(r.when, width_ - when_w, r.y + (head_ - ui::line_height(Font::TITLE)) / 2);
      right = width_ - when_w - (when_w ? ui::px(6) : 0);
    }
    if (r.track) {
      // The platform only where it leaves the destination room for a few letters.
      const int track_w = track.empty() ? 0 : ui::text_width(track, Font::BODY) + ui::px(2);
      const bool fits = track_w && right - x - track_w - ui::px(6) >= width_ / 4;
      ui::set_text(r.track, fits ? track : std::string());
      ui::set_color(r.track, moved ? ink() : muted());   // a new platform in full ink: it stands out
      lv_obj_set_size(r.track, std::max(1, fits ? track_w : 0), ui::line_height(Font::BODY));
      lv_obj_set_pos(r.track, right - (fits ? track_w : 0), r.y + (head_ - ui::line_height(Font::BODY)) / 2);
      if (fits) right -= track_w + ui::px(6);
    }
    ui::set_text(r.to, to);
    ui::set_color(r.to, r.list ? ink() : muted());
    lv_obj_set_size(r.to, std::max(1, right - x), ui::line_height(Font::BODY));
    lv_obj_set_pos(r.to, x, r.y + (head_ - ui::line_height(Font::BODY)) / 2);
  }

  std::string late_text(int late) const {
    if (!late_ || late < 1) return "";
    return tessera::format(owner_->text("late"), static_cast<long>(late));
  }

  // Where it goes, and how late it runs when that is a minute or more: "Baden  +2'".
  std::string with_delay(const Departure &d) const {
    const std::string late = late_text(d.late);
    return late.empty() ? std::string(d.to) : std::string(d.to) + "  " + late;
  }

  std::string track_text(const char *track) const {
    if (!show_track_ || !track[0]) return "";
    return tessera::fill(owner_->text("track"), "p", track);
  }

  // How long until a departure leaves. `exact`: to the second (12:05, 0:42), for the next one; else whole minutes. An
  // hour or more away is a time of day.
  std::string when_text(uint32_t at, uint32_t now, bool exact = false) const {
    if (exact) {
      if (at <= now) return owner_->text("now");
      const uint32_t left = at - now;
      if (left >= 3600) return tessera::clock_text(at);
      char text[8];
      snprintf(text, sizeof text, "%u:%02u", static_cast<unsigned>(left / 60), static_cast<unsigned>(left % 60));
      return text;
    }
    if (at <= now + 59) return owner_->text("now");
    const uint32_t minutes = (at - now) / 60;
    if (minutes >= 60) return tessera::clock_text(at);
    return tessera::format(owner_->text("minutes"), static_cast<long>(minutes));
  }

  // "57 min", "1 h 22 min".
  std::string duration_text(uint32_t from, uint32_t to) const {
    if (!from || to <= from) return "";
    const long minutes = static_cast<long>((to - from) / 60);
    if (minutes < 60) return tessera::format(owner_->text("minutes"), minutes);
    const std::string hours = tessera::format(owner_->text("hours"), minutes / 60);
    return tessera::fill(hours.c_str(), "m", std::to_string(minutes % 60));
  }

  // The first of `options` that fits `width` on one line in `font`, "" when none does.
  static std::string first_fitting(const std::vector<std::string> &options, int width, Font font = Font::BODY) {
    for (const auto &o : options)
      if (!o.empty() && ui::text_width(o, font) <= width) return o;
    return "";
  }

  SwissDepartures *owner_;
  int width_ = 0, head_ = 0, gap_ = 0;
  bool late_ = true, show_track_ = true, kinds_ = false, tinted_ = false;
  Look look_ = Look::TESSERA;
};

// The tile. A single cell: the next departure big, with its line and destination above it, and under it when to leave
// (with a walk to the stop) or the platform and the two after it. Two columns or more: a row per departure, as many as
// fit, each with its platform and countdown. The add-on sends new times once a minute; the countdown runs every second
// on the screen's own clock, and only a text that changes is set again.
// The same tile shows connections (`trip`): the trains from one stop to another, under a title "From -> To" (the arrow
// an icon), each with when it arrives and its changes where a departure says where it goes.
// A tap opens the card with every departure or connection; on a list of connections, a tap on a row opens that one.
// Its look: Tessera's blue, SBB's red badges, or a dark station board (the card's instrument colour, white words);
// badges by transport if asked; and the card orange when it is time to leave, if asked.
class NextTile : public tessera::Tile, public Rows {
 public:
  NextTile(SwissDepartures *owner, bool trip) : Rows(owner), trip_(trip) {}

  void create(const tessera::TileContext &c) override {
    metrics(c.width);
    height_ = c.height;
    tile_ = c.tile;
    walk_ = c.options["walk"] | 0;
    late_ = c.options["late"] | true;
    show_track_ = c.options["track"] | true;
    direct_ = c.options["direct"] | false;
    kinds_ = c.options["kinds"] | false;
    alarm_ = c.options["alarm"] | true;
    look_ = look_of(c.options["look"] | "tessera");
    if (look_ == Look::BOARD) ui::set_card_color(tessera::CardColor::INSTRUMENT);
    list_ = c.columns >= 2;
    snprintf(from_id_, sizeof from_id_, "%s", pick_value(c.options["stop"]).c_str());
    snprintf(to_id_, sizeof to_id_, "%s", pick_value(c.options["dest"]).c_str());
    const std::string name = c.name ? c.name : "";
    const std::string from = pick_label(c.options["stop"]), to = pick_label(c.options["dest"]);
    // The card's title: the connection's two stops ("Solothurn – Zürich HB"), or the stop of a board.
    const std::string title = trip_ ? (from.empty() || to.empty() ? from + to : from + " – " + to) : from;
    card_title_ = title.c_str();
    const int foot = ui::line_height(Font::BODY);
    // The title only where it leaves the departures their room: a row on a list, a row and a time on a single cell.
    if (trip_) {
      const int title_h = std::max(ui::line_height(Font::BODY), ui::line_height(Font::ICON_SMALL)) + ui::px(4);
      const int needs = head_ + (walk_ > 0 ? foot : 0) + (list_ ? 0 : ui::line_height(Font::TITLE));
      if (height_ - title_h >= needs) make_title(c.parent, name, from, to);
    }
    if (list_) {
      const int row = head_ + gap_;
      // With a walk to the stop the bottom line always says when to leave: the rows leave it room.
      const int room = height_ - top_ - (walk_ > 0 ? foot : 0);
      const int count = std::max(1, std::min(8, (room + gap_) / row));
      for (int i = 0; i < count; ++i) {
        Row r;
        make_row(r, c.parent, top_ + i * row, true);
        rows_.push_back(r);
      }
      note_ = ui::label(c.parent, Font::BODY, muted());
      lv_obj_set_size(note_, width_, foot);   // one line high, so a long text never wraps under the card
      lv_obj_set_pos(note_, 0, height_ - foot);
      note_room_ = top_ + count * row + foot <= height_ + gap_;
      leave_room_ = top_ + count * row - gap_ + foot <= height_;
    } else {
      Row r;
      make_row(r, c.parent, top_, false);
      rows_.push_back(r);
      big_ = ui::label(c.parent, Font::VALUE, ink());
      lv_obj_set_width(big_, width_);
      later_ = ui::label(c.parent, Font::BODY, muted());
      lv_obj_set_size(later_, width_, foot);
      lv_obj_set_pos(later_, 0, height_ - foot);
    }
  }

  void on_state(JsonObjectConst data) override {
    parse_all(data, trip_, owner_, departures_);
    if (trip_ && direct_) {
      tessera::List<Departure> direct;
      for (size_t i = 0; i < departures_.size(); ++i)
        if (departures_[i].changes == 0) direct.push_back(departures_[i]);
      departures_ = direct;
    }
    stale_ = data["stale"] | false;
    wait_ = data["wait"] | "";
  }

  void on_tick(uint32_t now) override {
    // The departures you can still make: not gone, and not leaving before you could walk there.
    std::vector<const Departure *> next;
    for (size_t i = 0; i < departures_.size(); ++i) {
      const Departure &d = departures_[i];
      if (now && d.at + 30 >= now + static_cast<uint32_t>(walk_) * 60) next.push_back(&d);
    }
    const char *why = wait_ == "not_filled"   ? (trip_ ? "choose_trip" : "choose_stop")
                      : !now                  ? "waiting"
                      : wait_ == "asking"     ? "waiting"
                      : !wait_.empty()        ? "failed"
                      : next.empty()          ? "none"
                                              : nullptr;
    // The card's colour: the station board's always; orange in the last minute before you have to leave.
    std::string leave;
    const bool go = !why && walk_ > 0 && leave_text(*next[0], now, leave);
    if (look_ == Look::BOARD) {
      ui::set_card_color(tessera::CardColor::INSTRUMENT);
    } else if (alarm_ && walk_ > 0) {
      tinted_ = go;
      ui::set_card_color(go ? tessera::CardColor::ORANGE : tessera::CardColor::NONE);
    }
    if (list_) paint_list(next, why, now);
    else paint_single(next, why, now);
    paint_title();
  }

  void on_theme() override {
    for (size_t i = 0; i < rows_.size(); ++i) {
      Row &r = rows_[i];
      ui::set_color(r.to, r.list ? ink() : muted());
      if (r.track) ui::set_color(r.track, muted());
      if (r.when) ui::set_color(r.when, ink());
    }
    if (big_) ui::set_color(big_, ink());
    if (later_) ui::set_color(later_, muted());
    if (note_) ui::set_color(note_, muted());
    paint_title();
  }

  // Every tap opens the card. On a list of connections a tap on a row opens that connection's legs at once.
  void on_tap() override {
    owner_->trip = trip_;
    snprintf(owner_->from_id, sizeof owner_->from_id, "%s", from_id_);
    snprintf(owner_->to_id, sizeof owner_->to_id, "%s", to_id_);
    owner_->walk = walk_;
    owner_->late = late_;
    owner_->track = show_track_;
    owner_->direct = direct_;
    owner_->kinds = kinds_;
    owner_->look = look_;
    owner_->shown = departures_;
    owner_->pick = 0;
    if (trip_ && list_) {
      const tessera::Point p = tessera::tap_point();
      const int row = head_ + gap_;
      if (p.y >= top_ && row > 0) {
        const size_t i = static_cast<size_t>((p.y - top_) / row);
        if (i < rows_.size() && i < listed_.size()) owner_->pick = listed_[i];
      }
    }
    tessera::open_card(owner_->plugin_id(), "list", "", tile_, std::string(card_title_.c_str()));
  }

 private:
  // The title of a tile of connections: "From -> To" with the arrow an icon of the set; while a stop is not chosen, the
  // tile's name from the editor or the one stop there is.
  void make_title(lv_obj_t *parent, const std::string &name, const std::string &from, const std::string &to) {
    if (name.empty() && from.empty() && to.empty()) return;
    const int body = ui::line_height(Font::BODY), icon = ui::line_height(Font::ICON_SMALL);
    const int line = std::max(body, icon);
    top_ = line + ui::px(4);
    title_from_ = ui::label(parent, Font::BODY, muted());
    if (from.empty() || to.empty()) {
      const std::string alone = !name.empty() ? name : from + to;
      ui::set_text(title_from_, alone);
      lv_obj_set_size(title_from_, width_, body);
      lv_obj_set_pos(title_from_, 0, (line - body) / 2);
      return;
    }
    title_arrow_ = ui::label(parent, Font::ICON_SMALL, muted());
    title_to_ = ui::label(parent, Font::BODY, muted());
    const std::string arrow = ui::icon(ARROW_RIGHT);
    ui::set_text(title_arrow_, arrow);
    ui::set_text(title_from_, from);
    ui::set_text(title_to_, to);
    // Each name as wide as it is; when the two do not fit, each keeps at least half of the room (cut with dots).
    const int arrow_w = ui::text_width(arrow, Font::ICON_SMALL), pad = ui::px(4);
    const int room = std::max(2, width_ - arrow_w - 2 * pad);
    int from_w = ui::text_width(from, Font::BODY) + 1, to_w = ui::text_width(to, Font::BODY) + 1;
    if (from_w + to_w > room) {
      if (from_w > room / 2 && to_w > room / 2) from_w = to_w = room / 2;
      else if (from_w > room / 2) from_w = room - to_w;
      else to_w = room - from_w;
    }
    lv_obj_set_size(title_from_, std::max(1, from_w), body);
    lv_obj_set_pos(title_from_, 0, (line - body) / 2);
    lv_obj_set_size(title_arrow_, arrow_w, icon);
    lv_obj_set_pos(title_arrow_, from_w + pad, (line - icon) / 2);
    lv_obj_set_size(title_to_, std::max(1, to_w), body);
    lv_obj_set_pos(title_to_, from_w + pad + arrow_w + pad, (line - body) / 2);
  }

  void paint_title() {
    for (lv_obj_t *t : {title_from_, title_arrow_, title_to_})
      if (t) ui::set_color(t, muted());
  }

  // With a walk to the stop: when to set off for the next departure you can make. "Leave now" in the last minute.
  bool leave_text(const Departure &d, uint32_t now, std::string &text) const {
    const int64_t left = static_cast<int64_t>(d.at) - static_cast<int64_t>(walk_) * 60 - static_cast<int64_t>(now);
    if (left < 60) {
      text = owner_->text("leave_now");
      return true;
    }
    text = tessera::format(owner_->text("leave_in"), static_cast<long>(left / 60));
    return false;
  }

  // "Leave now": in red, or in full ink on a card that is orange for it already.
  void paint_leave(lv_obj_t *label, bool go) {
    if (go && tinted_) ui::set_color(label, ink());
    else ui::set_tone(label, go ? Tone::ALERT : Tone::NORMAL, muted());
  }

  void paint_single(const std::vector<const Departure *> &next, const char *why, uint32_t now) {
    Row &r = rows_[0];
    if (why) {
      place_row(r, "", "", "", false, "");
      big_text(owner_->text(why), Font::BODY_LARGE, muted());
      ui::set_text(later_, "");
      return;
    }
    const Departure &d = *next[0];
    place_row(r, d.badge, with_delay(d), "", false, "", d.kind);
    // The time takes the largest face that fits the card. The line under it stays only where it costs the time
    // nothing, except the line that says when to leave, which stays wherever it fits.
    const std::string when = when_text(d.at, now, true);
    const std::string shape = digits_as_zero(when);   // the face follows the shape, so it never jumps as seconds pass
    const int room = height_ - top_ - head_;
    const Font alone = fitting(shape, room);
    const Font with_line = fitting(shape, room - ui::line_height(Font::BODY));
    const bool roomy = room - ui::line_height(Font::BODY) >= ui::line_height(with_line);
    const bool with_later = roomy && (walk_ > 0 || with_line == alone);
    big_text(when, with_later ? with_line : alone, ink(), with_later);
    if (!with_later) {
      ui::set_text(later_, "");
      return;
    }
    const std::string track = track_text(d.track);
    if (walk_ > 0) {
      std::string leave;
      const bool go = leave_text(d, now, leave);
      ui::set_text(later_, first_fitting({joined(leave, track), leave}, width_));
      paint_leave(later_, go);
      return;
    }
    ui::set_tone(later_, Tone::NORMAL, muted());
    if (stale_) {
      ui::set_text(later_, first_fitting({owner_->text("old")}, width_));
      return;
    }
    std::string two, one;
    for (size_t i = 1; i < next.size() && i < 3; ++i) {
      const std::string t = when_text(next[i]->at, now);
      if (i == 1) one = t;
      two = two.empty() ? t : two + " · " + t;
    }
    const std::string then2 = two.empty() ? "" : tessera::fill(owner_->text("then"), "list", two);
    const std::string then1 = one.empty() ? "" : tessera::fill(owner_->text("then"), "list", one);
    ui::set_text(later_, first_fitting({joined(track, then2), joined(track, then1), then2, then1, track}, width_));
  }

  static std::string digits_as_zero(std::string text) {
    for (char &c : text)
      if (c >= '1' && c <= '9') c = '0';
    return text;
  }

  // The largest of the big faces `text` fits in, `room` high and the card wide; BODY_LARGE when none does.
  Font fitting(const std::string &text, int room) const {
    for (Font f : {Font::VALUE, Font::HEADLINE, Font::TITLE})
      if (ui::text_width(text, f) <= width_ && ui::line_height(f) <= room) return f;
    return Font::BODY_LARGE;
  }

  void big_text(const std::string &text, Font face, theme::Role role, bool with_later = true) {
    ui::set_font(big_, face);
    ui::set_color(big_, role);
    ui::set_text(big_, text);
    const int h = ui::line_height(face);
    lv_obj_set_height(big_, h);
    // Between the head and the line under it (or the card's foot without that line), in the middle.
    const int top = top_ + head_, bottom = height_ - (with_later ? ui::line_height(Font::BODY) : 0);
    lv_obj_set_pos(big_, 0, top + std::max(0, (bottom - top - h) / 2));
  }

  void paint_list(const std::vector<const Departure *> &next, const char *why, uint32_t now) {
    listed_.clear();
    for (size_t i = 0; i < rows_.size(); ++i) {
      if (i == 0 && why) {
        place_row(rows_[0], "", owner_->text(why), "", false, "");
        continue;
      }
      if (why || i >= next.size()) {
        place_row(rows_[i], "", "", "", false, "");
        continue;
      }
      const Departure &d = *next[i];
      place_row(rows_[i], d.badge, with_delay(d), track_text(d.track), d.moved, when_text(d.at, now, i == 0), d.kind);
      listed_.push_back(d.plan);
    }
    // The bottom line: when to leave for the first one (with a walk to the stop), or that the times may be old.
    std::string note;
    bool go = false;
    if (!why && walk_ > 0 && leave_room_) go = leave_text(*next[0], now, note);
    else if (!why && stale_ && note_room_) note = owner_->text("old");
    ui::set_text(note_, first_fitting({note}, width_));
    paint_leave(note_, go);
  }

  bool trip_ = false, direct_ = false, alarm_ = true;
  int height_ = 0, top_ = 0, walk_ = 0, tile_ = -1;
  bool list_ = false, stale_ = false, note_room_ = false, leave_room_ = false;
  char from_id_[16] = {}, to_id_[16] = {};
  tessera::Text wait_, card_title_;
  tessera::List<Row> rows_;
  tessera::List<uint32_t> listed_;   // the timetable times of the rows shown, so a tap knows its connection
  lv_obj_t *big_{}, *later_{}, *note_{}, *title_from_{}, *title_arrow_{}, *title_to_{};
  tessera::List<Departure> departures_;
};

// One leg of a connection as the add-on maps it (fetch "legs", which the card asks for): a ride, or a walk.
struct Leg {
  char badge[20] = {};
  char dir[52] = {};     // where the train goes
  char from[52] = {};
  char to[52] = {};
  char ftrack[16] = {};  // the platform it leaves from, the new one when it changed
  char ttrack[16] = {};  // the platform it arrives at
  bool moved = false;
  uint8_t kind = RAIL;
  uint32_t dep = 0, arr = 0;
  int late = 0;
  int walk = 0;          // seconds on foot, 0 for a ride
};

// The card a tap on a tile opens. For a board of departures: every departure, a row each. For connections: every
// connection, two lines each (when it leaves and arrives and the countdown; how long it takes, its changes, its
// platform and how full it is); a tap on one asks the app for its legs and shows them: each train with where it goes,
// where and when you board and leave it and at which platform, and the minutes you have to change. Back goes from the
// legs to the list, and from the list closes the card. It wears its tile's colours.
class ListCard : public tessera::Card, public Rows {
 public:
  explicit ListCard(SwissDepartures *owner) : Rows(owner) {}
  ~ListCard() override {
    if (owner_->card == this) owner_->card = nullptr;
  }

  void open(const tessera::CardContext &c) override {
    metrics(c.width);
    height_ = c.height;
    owner_->card = this;
    trip_ = owner_->trip;
    late_ = owner_->late;
    show_track_ = owner_->track;
    walk_ = owner_->walk;
    direct_ = owner_->direct;
    kinds_ = owner_->kinds;
    // The card is drawn on Tessera's own card: a station board's dark face is the tile's alone, so its card takes the
    // Swiss red badges, the board's nearest look on a light or dark card.
    look_ = owner_->look == Look::BOARD ? Look::RED : owner_->look;
    items_ = owner_->shown;
    snprintf(from_id_, sizeof from_id_, "%s", owner_->from_id);
    snprintf(to_id_, sizeof to_id_, "%s", owner_->to_id);
    lv_obj_t *parent = c.parent;
    const int body = ui::line_height(Font::BODY);
    // The list.
    if (trip_) {
      row_h_ = head_ + body + gap_;
      const int count = std::max(1, std::min(12, (height_ + gap_) / row_h_));
      for (int i = 0; i < count; ++i) {
        TripRow r;
        r.y = i * row_h_;
        make_badge_parts(parent, r.badge, r.number);
        r.main = ui::label(parent, Font::BODY_LARGE, ink());
        r.when = ui::label(parent, Font::TITLE, ink());
        lv_obj_set_style_text_align(r.when, LV_TEXT_ALIGN_RIGHT, 0);
        r.sub = ui::label(parent, Font::BODY, muted());
        trips_.push_back(r);
      }
    } else {
      row_h_ = head_ + gap_;
      const int count = std::max(1, std::min(12, (height_ + gap_) / row_h_));
      for (int i = 0; i < count; ++i) {
        Row r;
        make_row(r, parent, i * row_h_, true);
        rows_.push_back(r);
      }
    }
    note_ = ui::label(parent, Font::BODY_LARGE, muted());
    lv_obj_set_size(note_, width_, ui::line_height(Font::BODY_LARGE));
    lv_obj_set_pos(note_, 0, 0);
    // The legs of one connection, made once and filled when it is chosen.
    if (trip_) {
      head_main_ = ui::label(parent, Font::TITLE, ink());
      head_when_ = ui::label(parent, Font::TITLE, ink());
      lv_obj_set_style_text_align(head_when_, LV_TEXT_ALIGN_RIGHT, 0);
      head_sub_ = ui::label(parent, Font::BODY, muted());
      status_ = ui::label(parent, Font::BODY_LARGE, muted());
      for (int i = 0; i < MAX_LEGS; ++i) {
        LegParts p;
        make_badge_parts(parent, p.badge, p.number);
        p.dir = ui::label(parent, Font::BODY, muted());
        p.dep = ui::label(parent, Font::BODY, ink());
        p.dtrack = ui::label(parent, Font::BODY, muted());
        p.arr = ui::label(parent, Font::BODY, ink());
        p.atrack = ui::label(parent, Font::BODY, muted());
        p.change = ui::label(parent, Font::BODY, muted());
        parts_.push_back(p);
      }
      if (owner_->pick) choose(owner_->pick);
    }
    owner_->pick = 0;   // used: a card opened another way starts at the list
    show_mode();
  }

  void on_state(JsonObjectConst data) override {
    tessera::List<Departure> fresh;
    parse_all(data, trip_, owner_, fresh);
    if (fresh.empty()) return;
    items_.clear();
    for (size_t i = 0; i < fresh.size(); ++i)
      if (!direct_ || fresh[i].changes == 0) items_.push_back(fresh[i]);
  }

  void on_tick(uint32_t now) override {
    now_ = now;
    if (pick_) paint_legs(now);
    else paint_list(now);
  }

  void on_theme() override {
    for (size_t i = 0; i < rows_.size(); ++i) {
      ui::set_color(rows_[i].to, ink());
      ui::set_color(rows_[i].track, muted());
      ui::set_color(rows_[i].when, ink());
    }
    for (size_t i = 0; i < trips_.size(); ++i) {
      TripRow &r = trips_[i];
      ui::set_color(r.main, ink());
      ui::set_color(r.when, ink());
      ui::set_color(r.sub, muted());
    }
    for (size_t i = 0; i < parts_.size(); ++i) {
      LegParts &p = parts_[i];
      for (lv_obj_t *l : {p.dir, p.dtrack, p.atrack, p.change}) ui::set_color(l, muted());
      for (lv_obj_t *l : {p.dep, p.arr}) ui::set_color(l, ink());
    }
    for (lv_obj_t *l : {head_main_, head_when_})
      if (l) ui::set_color(l, ink());
    for (lv_obj_t *l : {head_sub_, status_, note_})
      if (l) ui::set_color(l, muted());
  }

  // A tap on a connection of the list shows its legs.
  void on_tap() override {
    if (!trip_ || pick_) return;
    const tessera::Point p = tessera::tap_point();
    if (p.y < 0 || row_h_ <= 0) return;
    const size_t i = static_cast<size_t>(p.y / row_h_);
    if (i < listed_.size()) {
      choose(listed_[i]);
      show_mode();
      paint_legs(now_);
    }
  }

  bool on_back() override {
    if (!pick_) return false;
    pick_ = 0;
    show_mode();
    paint_list(now_);
    return true;
  }

  // The app's answer to the question for the legs. It holds the legs of the next three connections from the time asked,
  // one after the other; the card keeps the connection that leaves at the time tapped.
  void answer(JsonObjectConst m) {
    if (!asked_ || (m["re"] | 0u) != asked_) return;
    asked_ = 0;
    legs_.clear();
    tessera::List<Leg> all;
    tessera::List<uint16_t> starts;   // where each connection's legs begin in `all`
    if (m["ok"] | false) {
      std::string origin;
      for (JsonObjectConst it : m["result"]["items"].as<JsonArrayConst>()) {
        Leg l;
        const char *cat = it["cat"] | "";
        make_badge(l.badge, sizeof l.badge, cat, it["line"] | "");
        l.kind = kind_of(cat);
        snprintf(l.dir, sizeof l.dir, "%s", it["dir"] | "");
        snprintf(l.from, sizeof l.from, "%s", it["from"] | "");
        snprintf(l.to, sizeof l.to, "%s", it["to"] | "");
        const char *ftrack = it["ftrack"] | "";
        const char *moved = it["moved"] | "";
        l.moved = *moved && std::strcmp(moved, ftrack) != 0;
        snprintf(l.ftrack, sizeof l.ftrack, "%s", l.moved ? moved : ftrack);
        snprintf(l.ttrack, sizeof l.ttrack, "%s", it["ttrack"] | "");
        l.dep = it["dep"] | 0u;
        l.arr = it["arr"] | 0u;
        l.late = std::max(0, static_cast<int>(it["delay"] | 0.0f));
        l.walk = static_cast<int>(it["walk"] | 0.0f);
        if (!l.dep && !l.walk) continue;
        // The service lists the legs of each connection one after the other; a connection begins at the first stop.
        if (all.empty()) origin = l.from;
        if (all.empty() || (!origin.empty() && origin == l.from)) starts.push_back(static_cast<uint16_t>(all.size()));
        all.push_back(l);
      }
    }
    // The connection that leaves at the time tapped, or within two minutes of it (a walk first can move it). Never a
    // neighbour: without it the card says it has no details.
    size_t chosen = starts.size();
    uint32_t best = 121;
    for (size_t g = 0; g < starts.size(); ++g) {
      const uint32_t dep = all[starts[g]].dep;
      const uint32_t off = dep > pick_ ? dep - pick_ : pick_ - dep;
      if (off < best) {
        best = off;
        chosen = g;
      }
    }
    if (chosen < starts.size()) {
      const size_t end = chosen + 1 < starts.size() ? starts[chosen + 1] : all.size();
      for (size_t i = starts[chosen]; i < end; ++i) legs_.push_back(all[i]);
    }
    state_ = legs_.empty() ? FAILED : SHOWN;
    loaded_ = legs_.empty() ? 0 : pick_;
    tessera::refresh();
  }

 private:
  struct TripRow {
    lv_obj_t *badge{}, *number{}, *main{}, *when{}, *sub{};
    int y = 0;
  };
  struct LegParts {
    lv_obj_t *badge{}, *number{}, *dir{}, *dep{}, *dtrack{}, *arr{}, *atrack{}, *change{};
  };
  enum State : uint8_t { ASKING, SHOWN, FAILED };

  // A connection is chosen: ask the app for its legs, unless they are here already. The service finds it again by its
  // two stops and its time of departure.
  void choose(uint32_t plan) {
    pick_ = plan;
    if (loaded_ == plan && state_ == SHOWN) return;
    legs_.clear();
    state_ = FAILED;
    if (!from_id_[0] || !to_id_[0]) return;
    const tessera::LocalTime t = tessera::local_time(plan);
    char date[16], time[8];
    snprintf(date, sizeof date, "%04d-%02d-%02d", t.year, t.month, t.day);
    snprintf(time, sizeof time, "%02d:%02d", t.hour, t.minute);
    JsonDocument request;
    request["ask"] = "fetch:legs";
    request["data"]["from"] = from_id_;
    request["data"]["to"] = to_id_;
    request["data"]["date"] = date;
    request["data"]["time"] = time;
    asked_ = tessera::send(owner_, request.as<JsonObjectConst>());
    state_ = asked_ ? ASKING : FAILED;
  }

  const Departure *picked() const {
    for (size_t i = 0; i < items_.size(); ++i)
      if (items_[i].plan == pick_) return &items_[i];
    return nullptr;
  }

  // Empty what the other view shows: the list while the legs show, the legs while the list does. The next paint
  // fills the view on the glass.
  void show_mode() {
    if (pick_) {
      for (size_t i = 0; i < trips_.size(); ++i) {
        TripRow &r = trips_[i];
        place_badge(r.badge, r.number, "", 0, r.y);
        for (lv_obj_t *l : {r.main, r.when, r.sub}) ui::set_text(l, "");
      }
      ui::set_text(note_, "");
    } else {
      for (size_t i = 0; i < parts_.size(); ++i) hide_leg(parts_[i]);
      for (lv_obj_t *l : {head_main_, head_when_, head_sub_, status_})
        if (l) ui::set_text(l, "");
    }
  }

  std::string load_text(int load) const {
    if (load < 1 || load > 3) return "";
    static const char *const KEYS[] = {"load_1", "load_2", "load_3"};
    return owner_->text(KEYS[load - 1]);
  }

  std::string changes_text(int changes) const {
    return changes > 0 ? tessera::format(owner_->text("changes"), static_cast<long>(changes))
                       : std::string(owner_->text("direct_word"));
  }

  // "15:33 – 16:30  +2'".
  std::string span_text(const Departure &d) const {
    std::string text = tessera::clock_text(d.plan);
    if (d.arrive) text += " – " + tessera::clock_text(d.arrive);
    const std::string late = late_text(d.late);
    return late.empty() ? text : text + "  " + late;
  }

  void paint_list(uint32_t now) {
    std::vector<const Departure *> next;
    for (size_t i = 0; i < items_.size(); ++i) {
      const Departure &d = items_[i];
      if (!now || d.at + 30 >= now + static_cast<uint32_t>(walk_) * 60) next.push_back(&d);
    }
    listed_.clear();
    ui::set_text(note_, next.empty() ? std::string(owner_->text(!now ? "waiting" : "none")) : std::string());
    if (!trip_) {
      for (size_t i = 0; i < rows_.size(); ++i) {
        if (i >= next.size()) {
          place_row(rows_[i], "", "", "", false, "");
          continue;
        }
        const Departure &d = *next[i];
        place_row(rows_[i], d.badge, with_delay(d), track_text(d.track), d.moved, when_text(d.at, now, i == 0),
                  d.kind);
      }
      return;
    }
    // Every row's times start at one column: after the widest badge on the card.
    int column = 0;
    for (size_t i = 0; i < trips_.size() && i < next.size(); ++i)
      column = std::max(column, place_badge(trips_[i].badge, trips_[i].number, next[i]->badge, 0, trips_[i].y,
                                            next[i]->kind));
    for (size_t i = 0; i < trips_.size(); ++i) {
      TripRow &r = trips_[i];
      if (i >= next.size()) {
        place_badge(r.badge, r.number, "", 0, r.y);
        for (lv_obj_t *l : {r.main, r.when, r.sub}) ui::set_text(l, "");
        continue;
      }
      const Departure &d = *next[i];
      listed_.push_back(d.plan);
      const int x = column ? column + ui::px(8) : 0;
      const std::string when = now ? when_text(d.at, now, i == 0) : "";
      const int when_w = when.empty() ? 0 : ui::text_width(when, Font::TITLE) + ui::px(2);
      ui::set_text(r.when, when);
      lv_obj_set_size(r.when, std::max(1, when_w), ui::line_height(Font::TITLE));
      lv_obj_set_pos(r.when, width_ - when_w, r.y + (head_ - ui::line_height(Font::TITLE)) / 2);
      const int main_w = std::max(1, width_ - when_w - ui::px(6) - x);
      const std::string bare = d.arrive ? tessera::clock_text(d.plan) + " – " + tessera::clock_text(d.arrive)
                                        : tessera::clock_text(d.plan);
      ui::set_text(r.main, first_fitting({span_text(d), bare, tessera::clock_text(d.plan)}, main_w, Font::BODY_LARGE));
      lv_obj_set_size(r.main, main_w, ui::line_height(Font::BODY_LARGE));
      lv_obj_set_pos(r.main, x, r.y + (head_ - ui::line_height(Font::BODY_LARGE)) / 2);
      const std::string time = duration_text(d.plan, d.arrive), changes = changes_text(d.changes);
      const std::string track = track_text(d.track), load = load_text(d.load);
      const int sub_w = std::max(1, width_ - x);
      ui::set_text(r.sub, first_fitting({joined(joined(joined(time, changes), track), load),
                                         joined(joined(time, changes), track), joined(time, changes), time},
                                        sub_w));
      lv_obj_set_size(r.sub, sub_w, ui::line_height(Font::BODY));
      lv_obj_set_pos(r.sub, x, r.y + head_);
    }
  }

  void place_line(lv_obj_t *label, lv_obj_t *track, const std::string &text, const std::string &track_text, bool moved,
                  int x, int y) {
    const int body = ui::line_height(Font::BODY);
    const int track_w = track_text.empty() ? 0 : ui::text_width(track_text, Font::BODY) + ui::px(2);
    ui::set_text(track, track_text);
    ui::set_color(track, moved ? ink() : muted());
    lv_obj_set_size(track, std::max(1, track_w), body);
    lv_obj_set_pos(track, width_ - track_w, y);
    const int w = std::max(1, width_ - x - track_w - (track_w ? ui::px(6) : 0));
    ui::set_text(label, text);
    lv_obj_set_size(label, w, body);
    lv_obj_set_pos(label, x, y);
  }

  void hide_leg(LegParts &p) {
    place_badge(p.badge, p.number, "", 0, 0);
    for (lv_obj_t *l : {p.dir, p.dep, p.dtrack, p.arr, p.atrack, p.change}) ui::set_text(l, "");
  }

  void paint_legs(uint32_t now) {
    const Departure *d = picked();
    if (!d) {   // gone from the list (it left): back to the list
      pick_ = 0;
      show_mode();
      paint_list(now);
      return;
    }
    const int body = ui::line_height(Font::BODY), title = ui::line_height(Font::TITLE);
    int y = 0;
    // The connection: when it leaves and arrives, the countdown, and how long, its changes and how full.
    const std::string when = now ? when_text(d->at, now, d->at < now + 600) : "";
    const int when_w = when.empty() ? 0 : ui::text_width(when, Font::TITLE) + ui::px(2);
    ui::set_text(head_when_, when);
    lv_obj_set_size(head_when_, std::max(1, when_w), title);
    lv_obj_set_pos(head_when_, width_ - when_w, y);
    const int main_w = std::max(1, width_ - when_w - ui::px(6));
    ui::set_text(head_main_, first_fitting({span_text(*d), tessera::clock_text(d->plan)}, main_w, Font::TITLE));
    lv_obj_set_size(head_main_, main_w, title);
    lv_obj_set_pos(head_main_, 0, y);
    y += title + ui::px(2);
    const std::string time = duration_text(d->plan, d->arrive), changes = changes_text(d->changes);
    ui::set_text(head_sub_,
                 first_fitting({joined(joined(time, changes), load_text(d->load)), joined(time, changes)}, width_));
    lv_obj_set_size(head_sub_, width_, body);
    lv_obj_set_pos(head_sub_, 0, y);
    y += body + gap_ + ui::px(4);

    if (state_ != SHOWN) {
      ui::set_text(status_, owner_->text(state_ == ASKING ? "asking_legs" : "no_legs"));
      lv_obj_set_size(status_, width_, ui::line_height(Font::BODY_LARGE));
      lv_obj_set_pos(status_, 0, y);
      for (size_t i = 0; i < parts_.size(); ++i) hide_leg(parts_[i]);
      return;
    }
    ui::set_text(status_, "");
    const int indent = ui::px(ui::large() ? 14 : 10), line = body + ui::px(2);
    size_t shown = 0;
    for (size_t i = 0; i < legs_.size() && i < parts_.size(); ++i) {
      const Leg &l = legs_[i];
      LegParts &p = parts_[i];
      // Between two rides: the minutes to change.
      std::string change;
      if (i > 0 && !l.walk && !legs_[i - 1].walk && legs_[i - 1].arr && l.dep > legs_[i - 1].arr)
        change = tessera::format(owner_->text("change_min"), static_cast<long>((l.dep - legs_[i - 1].arr) / 60));
      const int need = (change.empty() ? 0 : line) + (l.walk ? line : head_ + 2 * line) + gap_;
      if (y + need > height_) break;
      ui::set_text(p.change, change);
      if (!change.empty()) {
        lv_obj_set_size(p.change, width_ - indent, body);
        lv_obj_set_pos(p.change, indent, y);
        y += line;
      }
      if (l.walk) {
        place_badge(p.badge, p.number, "", 0, y);
        const long minutes = std::max(1L, static_cast<long>((l.walk + 59) / 60));
        ui::set_text(p.dir, tessera::format(owner_->text("walk"), minutes));
        lv_obj_set_size(p.dir, width_, body);
        lv_obj_set_pos(p.dir, 0, y);
        for (lv_obj_t *o : {p.dep, p.dtrack, p.arr, p.atrack}) ui::set_text(o, "");
        y += line + gap_;
        ++shown;
        continue;
      }
      // The train: its badge and where it goes, then where and when you board it and get off, each with its platform.
      const int badge_w = place_badge(p.badge, p.number, l.badge, 0, y, l.kind);
      const int x = badge_w ? badge_w + ui::px(8) : 0;
      ui::set_text(p.dir, l.dir[0] ? tessera::fill(owner_->text("towards"), "to", l.dir) : std::string());
      lv_obj_set_size(p.dir, std::max(1, width_ - x), body);
      lv_obj_set_pos(p.dir, x, y + (head_ - body) / 2);
      y += head_ + ui::px(2);
      std::string dep = (l.dep ? tessera::clock_text(l.dep) + "  " : std::string()) + l.from;
      const std::string late = late_text(l.late);
      if (!late.empty()) dep += "  " + late;
      place_line(p.dep, p.dtrack, dep, track_text(l.ftrack), l.moved, indent, y);
      y += line;
      place_line(p.arr, p.atrack, (l.arr ? tessera::clock_text(l.arr) + "  " : std::string()) + l.to,
                 track_text(l.ttrack), false, indent, y);
      y += line + gap_;
      ++shown;
    }
    for (size_t i = shown; i < parts_.size(); ++i) hide_leg(parts_[i]);
  }

  bool trip_ = false, direct_ = false;
  int height_ = 0, row_h_ = 0, walk_ = 0;
  uint32_t now_ = 0, pick_ = 0, loaded_ = 0, asked_ = 0;
  State state_ = FAILED;
  char from_id_[16] = {}, to_id_[16] = {};
  tessera::List<Departure> items_;
  tessera::List<uint32_t> listed_;
  tessera::List<Row> rows_;
  tessera::List<TripRow> trips_;
  tessera::List<LegParts> parts_;
  tessera::List<Leg> legs_;
  lv_obj_t *note_{}, *head_main_{}, *head_when_{}, *head_sub_{}, *status_{};
};

void SwissDepartures::setup() {
  add_tile("next", [this]() { return new NextTile(this, false); });
  add_tile("trip", [this]() { return new NextTile(this, true); });
  add_card("list", [this]() { return new ListCard(this); });
}

void SwissDepartures::on_message(JsonObjectConst message) {
  if (card) card->answer(message);
}

}  // namespace esphome::swiss_departures
