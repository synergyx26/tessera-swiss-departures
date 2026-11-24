# Changelog

## 1.1.0 - 2026-11-24
- In German, French and Italian too, following the screen's language (and a README in German).
- Countdowns read "8 min", and to the second only in the last two minutes, so they never look like a time of day.
- A Next departure tile shows its stop's name above the departures.
- Buses and trams show their stand ("Stand D", "Kante D") instead of a platform.
- A connection's arrival time includes its delay at the destination.
- The card refreshes a connection's legs every minute and a half while it is open, says how many more legs there are
  when they do not all fit, and stays on a connection after it leaves the list.
- The legs are found by Swiss time, also on a screen set to another time zone.
- Fixed: the transport filter Cable car showed nothing; mountain railways (CC) count as cable cars.
- Fixed: the tile no longer turns orange where "Leave now" has no room; "Times may be old" also shows with a walk.
- Clearer hints in the inspector. A tap on a tile without times opens nothing.

## 1.0.0 - 2026-11-21
- First release: the next departures from any Swiss stop, and every connection from one stop to another, live from
  transport.opendata.ch, with delays, platforms and platform changes.
- **Next departure** tile: find your stop by name, and keep to one kind of transport, one line or one direction.
- **Connections** tile: every train from one stop to another (also one that runs on beyond your destination), with when
  it arrives and its changes, under a title "From -> To".
- A tap opens a card with every departure or connection; a tap on a connection shows its legs: each train, where you
  board and get off with times and platforms, walks, and the minutes to change.
- With a walk to the stop, departures you can no longer make are skipped and the tile says when to leave.
- Colours: Tessera's blue, Swiss red line badges, or a dark station board; badges by transport if you like; and the
  tile turns orange when it is time to leave.
