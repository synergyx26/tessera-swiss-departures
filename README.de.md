# Abfahrten Schweiz

Live-Fahrplan des Schweizer öffentlichen Verkehrs auf einem Tessera-Bildschirm: wann der nächste Zug, das nächste Tram,
der nächste Bus, das Schiff oder die Seilbahn an einer beliebigen Haltestelle der Schweiz abfährt, und alle
Verbindungen von einer Haltestelle zur anderen, mit Verspätung, Gleis und Gleiswechsel. Die Zeiten kommen von
[transport.opendata.ch](https://transport.opendata.ch), dem offenen Dienst über den Schweizer Fahrplan und seine
Echtzeitdaten (alle Unternehmen: SBB, BLS, ZVV, PostAuto und die übrigen). Es braucht weder Konto noch Schlüssel.

Nicht von SBB CFF FFS oder einem Verkehrsunternehmen hergestellt und in keiner Verbindung mit ihnen.

## Was dazukommt

**Nächste Abfahrt.** Die Abfahrten einer Haltestelle. Auf einer einzelnen Zelle die nächste, so gross wie die Zelle
erlaubt, mit der Linie (S6, IC5, 14) und dem Ziel darüber, darunter das Gleis und die zwei folgenden. Auf einer Kachel
von zwei oder mehr Spalten eine Liste: eine Zeile pro Abfahrt mit Gleis und Countdown.

**Verbindungen.** Die Züge von einer Haltestelle zur anderen, so wie sie der Schweizer Reiseplaner findet: auch ein
Zug, der über dein Ziel hinaus weiterfährt (von Solothurn nach Zürich HB ist das der IC 5, der weiter nach Rorschach
fährt, und der IR 55). Jede Zeile zeigt die Linie, die Ankunft, die Anzahl Umstiege, das Gleis und den Countdown,
unter dem Titel "Solothurn -> Zürich HB".

**Eine Karte für jede.** Tippe eine Kachel an, um sie zu öffnen. Eine Kachel Nächste Abfahrt zeigt jede Abfahrt ihrer
Haltestelle. Eine Kachel Verbindungen zeigt jede Verbindung mit Abfahrt und Ankunft, Dauer, Umstiegen, Gleis und
Auslastung des Zuges (wenn der Dienst sie kennt). Tippe auf eine Verbindung, auf der Karte oder in der Liste der
Kachel, und du siehst ihre Abschnitte: jeden Zug mit Ziel, wo du ein- und aussteigst mit den Gleisen, jeden Fussweg
und die Minuten zum Umsteigen. Zurück führt zur Liste.

## Einrichten

1. Füge das Plugin in Tessera einem Bildschirm hinzu (Plugins, oder der Tab Plugins des Bildschirms). Der Bildschirm
   wird einmal neu gebaut.
2. Platziere in Layout **Nächste Abfahrt** oder **Verbindungen** aus der Gruppe Plugins der Bibliothek.
3. Tippe im Inspektor unter **Haltestelle** (oder **Von** und **Nach**) ein paar Buchstaben des Bahnhofs oder der
   Haltestelle (`Zürich HB`, `Bern, Bahnhof`, `Lausanne, Flon`) und wähle sie aus der Liste.
4. Wenn du magst, beschränke eine Kachel Nächste Abfahrt auf ein **Verkehrsmittel** (Zug, Tram, Bus, Schiff,
   Seilbahn), eine **Linie** oder eine **Richtung**, oder eine Kachel Verbindungen auf **Nur direkt**.
5. Stelle **Weg zur Haltestelle** auf die Minuten ein, die du brauchst. Abfahrten, die du nicht mehr erreichst, werden
   übersprungen, und die Kachel sagt "Gehen in 3 min", dann "Jetzt gehen".
6. Wähle unter **Farben** das Aussehen (unten).

## Farben

- **Aussehen**: *Tessera* (die blauen Liniensymbole des Bildschirms), *Schweizer Rot* (rote Liniensymbole wie auf den
  Tafeln in den Bahnhöfen) oder *Abfahrtstafel* (die Kachel als dunkle Tafel mit weisser Schrift, im hellen wie im
  dunklen Modus; ihre Karte nimmt die roten Liniensymbole).
- **Farbe nach Verkehrsmittel**: Züge rot, Trams und Metros blau, Busse gelb, Schiffe violett, Seilbahnen grün.
- **Farbe, wenn es Zeit ist zu gehen**: Mit einem Weg zur Haltestelle wird die Kachel in der Minute orange, in der du
  losgehen musst.
- Die Farben sind die des Bildschirms und folgen seinem hellen und dunklen Modus. Du kannst der Kachel im Editor
  trotzdem eine eigene Farbe geben; die Abfahrtstafel behält ihr dunkles Gesicht.

## Gut zu wissen

- Tessera fragt transport.opendata.ch einmal pro Minute pro Haltestelle oder Verbindung ab, für alle Bildschirme und
  Kacheln zusammen. Dazwischen zählt der Bildschirm mit seiner eigenen Uhr in Minuten ("8 min"), die nächste Abfahrt in
  ihren letzten zwei Minuten auf die Sekunde (1:42, 0:42). Eine Zeit eine Stunde oder mehr entfernt erscheint als
  Uhrzeit.
- Eine Abfahrt mit einer Minute oder mehr Verspätung bekommt "+2'" hinter dem Ziel, und ihr Countdown enthält die
  Verspätung. Die Ankunftszeit einer Verbindung enthält ihre Verspätung am Ziel. Schalte **Verspätung zeigen** aus, um
  das "+2'" wegzulassen.
- Das Gleis eines Zuges (Gl. 7) oder die Kante eines Busses oder Trams (Kante D) erscheint, wenn der Dienst es kennt. Ein geändertes Gleis zeigt das neue, in
  einer Liste in voller Schriftfarbe. Schalte **Gleis zeigen** aus, um es wegzulassen.
- **Richtung** behält die Abfahrten, deren *letzte* Haltestelle dieser Ort ist. Eine Schweizer Haltestelle bedient meist
  beide Richtungen mit einer Nummer; so beschränkt eine Kachel sich auf eine Seite. Für Züge, die einen Ort nur
  durchfahren, nimm **Verbindungen**.
- **Linie** gilt nach Liniennummer. In einem grossen Bahnhof können ein Zug und ein Bus dieselbe Nummer haben (S5 und
  IC5); stelle zusätzlich das **Verkehrsmittel** ein, um sie zu trennen.
- Die Kachel Verbindungen fragt den Reiseplaner nach den nächsten Verbindungen ab jetzt. Ein Zug, der nach Fahrplan
  schon abgefahren ist, aber verspätet unterwegs, ist nicht dabei.
- Die Abschnitte einer Verbindung werden erst beim Antippen abgefragt, daher steht auf der Karte kurz "Route laden";
  solange die Karte offen ist, alle anderthalb Minuten neu. Hat eine Verbindung mehr Abschnitte als Platz, sagt die Karte,
  wie viele noch folgen.
- **Farbe, wenn es Zeit ist zu gehen** braucht einen Fussweg zur Haltestelle; ohne ihn tut es nichts.
- Eine fällige Abfahrt sagt "jetzt".
- Antwortet der Dienst nicht, behält die Kachel die letzten Zeiten und sagt, dass sie alt sein können.
- Auf Deutsch, Französisch, Italienisch und Englisch: sie folgt der Sprache des Bildschirms.
- Läuft auf jedem Board. Gebaut und täglich benutzt auf einem Guition 4-Zoll (ESP32-S3), dazu gezeichnet auf dem CYD,
  einem stehenden 4,3-Zoll- und einem 10-Zoll-Bildschirm.

## Datenschutz

Tessera (die App in Home Assistant, nicht der Bildschirm) fragt `transport.opendata.ch` nach den Abfahrten der von dir
gewählten Haltestellen, nach den Verbindungen zwischen Von und Nach einer Kachel Verbindungen, nach den Abschnitten
einer angetippten Verbindung und nach Haltestellennamen, während du im Inspektor suchst. Der Dienst sieht die Adresse
deines Home Assistant, diese Haltestellen-Nummern, Zeiten und was du in die Suche tippst, sonst nichts. Es werden kein
Schlüssel, kein Konto und keine persönlichen Daten gesendet.

Daten: [transport.opendata.ch](https://transport.opendata.ch) von Opendata.ch, aus der Open-Data-Plattform
Öffentlicher Verkehr Schweiz ([opentransportdata.swiss](https://opentransportdata.swiss)).
