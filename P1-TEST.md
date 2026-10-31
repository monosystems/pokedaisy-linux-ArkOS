# P1: Manuelle Testliste für den R36S

Diese Liste prüft P1.1 bis P1.16 aus [`TODO.md`](TODO.md) auf dem R36S.
Nach den automatisierten Prüfungen findest du die verbleibenden Geräteprüfungen
und bekannte Fehler in [`P1-R36S-RESTTEST.md`](P1-R36S-RESTTEST.md).
Hake einen Punkt erst ab, wenn das erwartete Ergebnis auf dem Gerät sichtbar ist.
Wenn dir eine Voraussetzung fehlt, notiere **nicht getestet** statt den Punkt
abzuhaken. Ein Fehler ist nicht automatisch ein fehlendes Feature: Notiere auch,
was genau passiert ist.

## Vorbereitung

- [ ] Installiere den aktuellen Port-Build auf dem R36S und starte ihn über das
      Ports-Menü. Notiere Build oder Commit: ____________________
- [ ] Notiere Datum, ArkOS-Version und verwendete Spiele: ____________________
- [ ] Lege **außerhalb des ROM-Ordners** eine unabhängige Kopie deiner Saves und
      des Ordners `pokedaisy-states` an. Prüfe, ob sich die Kopie lesen lässt.
- [ ] Verwende für Neustart, Save-States und Wiederherstellung eine Testkopie
      deines Spiels und Saves. Verlasse dich nicht allein auf die automatischen
      Backups: Es werden höchstens zehn pro Spiel behalten.
- [ ] Bereite möglichst englische FireRed- und Emerald-Teststände vor. Für alle
      Prüfungen brauchst du unter anderem ein spielbares Team, gehaltene Items,
      verschiedene Beutel-Items inklusive einer TM, Pokédex-Einträge, einen
      erreichbaren Kampf und Guide-Fortschritt. Für `MAX` brauchst du ein
      Pokémon auf Level 100, für bestimmte Typ- und Statusfälle passende Pokémon.
- [ ] Lege für P1.16 zwei unterscheidbare Test-Saves desselben Testspiels an,
      zum Beispiel an unterschiedlichen Orten. Bewahre beide zusätzlich außerhalb
      des ROM-Ordners auf. Notiere ihren Unterschied: ____________________
- [ ] Kopiere für P1.16 den älteren Battery-Save in den ROM-Ordner unter dem
      Namen `<Spielname>.backup-20200101-000000.sav`. Der neuere Test-Save bleibt
      als `<Spielname>.sav` oder die bereits vorhandene `<Spielname>.srm` aktiv.
      Verwende denselben Spielnamen wie bei der ROM und keinen Save-State.

**Bedienung:** `L2` öffnet und schließt den Companion. Darin bewegst du den
Cursor mit dem Steuerkreuz, bestätigst mit `A`, gehst mit `B` zurück und
wechselst mit `L1`/`R1` den Tab. `START` schließt den Companion. Im Spiel hältst
du `R2` für Fast-Forward. Halte `START+SELECT`, um den Port zu verlassen.
Im Companion läuft das Spiel weiter. Suche dir für Menütests eine sichere Stelle.

## P1.1 Tab-Leiste

- [ ] Öffne den Companion außerhalb eines Kampfes. Die verfügbaren Tabs stehen
      in der Reihenfolge `PARTY`, `DEX`, `MAP`, `BAG`, `GUIDE`; `SETTINGS` ist
      das Zahnrad. Ein Tab ohne verfügbare Spieldaten kann fehlen.
- [ ] Prüfe bei mehr verfügbaren Tabs als Plätzen `MORE`: Versteckte Tabs lassen
      sich dort öffnen. Wenn nichts versteckt ist, erscheint `MORE` nicht.
- [ ] Beginne einen Kampf. `BATTLE` steht vorne; für die übrigen Tabs ist ein
      Platz weniger frei und ein verdrängter Tab ist über `MORE` erreichbar.
- [ ] Beende den Kampf. Die Leiste kehrt zur Belegung ohne `BATTLE` zurück.

## P1.2 Tab-Leiste und Tweaks einstellen

- [ ] Öffne `SETTINGS > COMPANION > TAB BAR`. Verfügbare Tabs zeigen `SHOWN`
      oder `HIDDEN`; die Überschrift zeigt die Zahl der ausgewählten Tabs von 5.
- [ ] Blende einen Tab aus. Sein Chip verschwindet und der Tab bleibt über
      `MORE` erreichbar, auch wenn auf der Leiste Platz frei ist.
- [ ] Blende nacheinander alle verfügbaren Tabs aus. Kein normaler Tab-Chip
      bleibt sichtbar; `MORE` und das Zahnrad bleiben bedienbar. Stelle danach
      die gewünschten Tabs wieder auf `SHOWN`.
- [ ] Fülle die fünf Plätze und versuche einen weiteren Tab einzuschalten.
      Ein Hinweis erklärt, dass zuerst ein anderer Tab ausgeblendet werden muss.
- [ ] Stelle die ursprüngliche Tab-Auswahl wieder her. Verlasse den Port und
      starte ihn erneut. Die gewählte Belegung bleibt erhalten.
- [ ] Öffne `SETTINGS > COMPANION > TWEAKS`. Die vier Schalter für Party-Icons,
      Karten-Cursor, Tab-Animationen und den Wechsel zum Kampf sind vorhanden.
- [ ] Schalte `MAP CURSOR BLINK` aus und wieder ein. Der Karten-Cursor pulsiert
      nur bei eingeschaltetem Schalter.
- [ ] Schalte `TAB ANIMATIONS` aus und wieder ein. Tabs wechseln ohne Animation
      direkt und mit Animation gleiten sie ein.
- [ ] Schalte `JUMP TO BATTLE` aus und beginne einen Kampf. Der Companion springt
      nicht automatisch auf `BATTLE`. Wiederhole den Test mit eingeschaltetem
      Schalter: `BATTLE` öffnet sich automatisch und nach dem Kampf kehrt der
      vorherige Tab zurück.
- [ ] Prüfe den Schalter `PARTY ICONS MOVE`. Die Icon-Bewegung betrifft das
      gehaltene Party-Feld. Falls sich dieses Halten mit der R36S-Steuerung nicht
      auslösen lässt, notiere den Punkt als **nicht getestet**.

## P1.3 Titelschirm und geladener Spielstand

- [ ] Starte das Testspiel mit vorhandenem Team tatsächlich neu bis zum
      Titelschirm, ohne einen alten Resume-Zustand zu laden. Öffne vor `CONTINUE` den Companion: `PARTY`,
      `BAG`, `MAP` und, falls verfügbar, `CARD` warten mit `PRESS START` statt
      alte oder scheinbar gültige Daten anzuzeigen. Der Hinweis blinkt.
- [ ] Lade den Spielstand im Spiel und bewege dich über mindestens fünf
      verschiedene Positionen. Die Tabs zeigen anschließend die tatsächlichen
      Daten statt der Warteansicht. Prüfe das auch mit leerem Team.
- [ ] Verlasse das Spiel normal und starte es erneut mit Resume. Der Companion
      zeigt nach dem Fortsetzen die Daten, ohne auf `PRESS START` zu warten.

## P1.4 Getragenes Item

- [ ] Gib im Spiel mindestens einem Team-Pokémon ein Item und öffne `PARTY`.
      Unter den KP-Zahlen stehen der Itemname und, soweit Platz ist, das Symbol.
- [ ] Vergleiche ein Pokémon ohne Item und, falls vorhanden, eines mit Status wie
      Gift oder Schlaf. Der Text bleibt lesbar; bei Status kann das Itemsymbol
      entfallen.

## P1.5 EXP und Typ-Matchups

- [ ] Öffne in `PARTY` die Zusammenfassung eines Pokémon. Der EXP-Balken und
      `NEXT` passen zum Fortschritt bis zum nächsten Level.
- [ ] Öffne die Matchup-Seite. `WEAK TO`, `RESISTS` und, falls vorhanden,
      `IMMUNE TO` zeigen passende Typen und Multiplikatoren.
- [ ] Prüfe ein bekanntes Beispiel, falls vorhanden: Glurak/Charizard zeigt eine
      vierfache Schwäche gegen Gestein/Rock. Bei einem Level-100-Pokémon steht
      `MAX` statt eines nächsten Levels.

## P1.6 STATS

- [ ] Öffne in der Pokémon-Zusammenfassung `STATS`. Für alle sechs Werte sind
      `STAT`, `IV` und `EV` sichtbar, dazu die Gesamtsummen.
- [ ] Vergleiche die angezeigten Statuswerte mit der Zusammenfassung im Spiel.
      Das Wesen und gegebenenfalls rote und blaue Pfeile für beeinflusste Werte
      sind sichtbar. Ein neutrales Wesen muss keine Pfeile haben.
- [ ] `HIDDEN POWER` zeigt Typ und Stärke (`PWR`). Prüfe sie, wenn möglich,
      gegen eine unabhängige Berechnung für dieses Pokémon.

## P1.7 Beutel: ALL, Sortierung und TMs

- [ ] Öffne `BAG > ALL`. Items aus mehreren Taschen erscheinen gemeinsam.
- [ ] Wechsle die Sortierung durch `DEFAULT`, `NAME`, `TYPE` und `COUNT`. Die
      Reihenfolge entspricht jeweils der Auswahl und keine Items verschwinden.
- [ ] Öffne die TM-Tasche. Bei einer unterstützten, erkannten ROM steht hinter
      der TM-Nummer der passende Attackenname. Vergleiche ihn mit dem Spiel.

## P1.8 Beutel: Favoriten

- [ ] Wähle ein Item mit `A`, drücke `RECHTS` und aktiviere den Stern in der
      Beschreibung mit `A`. Mit `LINKS` kommst du zurück zum Item. Das Item zeigt
      in der Liste einen goldenen Stern und die Tasche `FAVORITES` erscheint.
- [ ] Öffne `FAVORITES`. Das markierte Item ist enthalten. Entferne den Stern;
      wenn keine Favoriten mehr übrig sind, verschwindet die Tasche wieder.
- [ ] Markiere erneut ein Item, verlasse den Port und starte das Spiel erneut.
      Der Favorit ist noch vorhanden. Prüfe mit einem anderen unterstützten Spiel,
      dass dessen Favoriten unabhängig sind.

## P1.9 Beutel: Darstellung

- [ ] Prüfe in FireRed die Taschen links, die markierte offene Tasche, die
      Itemliste rechts und die Beschreibung darunter. `PC` und Sortierung stehen
      unter den Taschen.
- [ ] Prüfe dieselbe Ansicht in Emerald: Die blau-goldene Darstellung und der
      gestreifte Hintergrund unterscheiden sich von FireRed.
- [ ] Wähle `PC`. Es erscheint ein Hinweis, dass diese Funktion noch nicht
      verfügbar ist. Fehlende Item-Icons und ROM-Beutelgrafiken sind bekannte
      Einschränkungen und kein Fehler dieser P1-Aufgabe.
- [ ] Navigiere in einer langen Itemliste mit `UNTEN` bis zum letzten Item und
      mit `OBEN` wieder zurück. Die Liste scrollt und der Cursor springt nicht
      auf `PC`. Mit `A` kannst du auch das letzte Item anwählen; mit `LINKS`
      gelangst du zurück zu den Taschen und zu `PC`.

## P1.10 Pokédex-Filter und Navigation

- [ ] Öffne `DEX`. `ALL`, `CAUGHT`, `SEEN ONLY` und `NOT SEEN` zeigen jeweils
      eine Anzahl. Die Liste passt sich dem gewählten Filter an.
- [ ] Scrolle mit `UNTEN` weit in der Liste nach unten und mit `OBEN` wieder bis
      zum ersten Eintrag. Der Cursor bleibt sichtbar, folgt den Einträgen und
      die Liste aktualisiert sich dabei.
- [ ] Öffne einen Eintrag und wechsle mit den Pfeilen zum vorherigen und nächsten
      Eintrag. Der Zähler aktualisiert sich passend zur gefilterten Liste.
- [ ] Falls der Tab oder ein Eintrag fehlt, prüfe zuerst, ob das gewählte Spiel
      und seine ROM-Tabellen vom Port unterstützt werden.

## P1.11 Kampf: Gegner und Hinweise

- [ ] Öffne während eines Kampfes `BATTLE > INFO`. Bei bekannten Typen erscheinen
      die gegnerischen Schwächen und Resistenzen; die Karten zeigen passende
      Bedrohungshinweise. Eine längere Ansicht lässt sich scrollen.
- [ ] Prüfe die Attackenbewertungen und den besten Vorschlag, sofern passende
      Daten vorliegen.
- [ ] Schalte `SETTINGS > BATTLE > BATTLE HINTS` aus. Bewertungen, bester
      Vorschlag und Matchups werden ausgeblendet. Schalte die Option wieder ein
      und prüfe, dass sie zurückkehren.

## P1.12 Guide: Entwicklungen und Bosse

- [ ] Öffne `GUIDE > EVOLUTIONS`. Das aktuelle Team steht zuerst; weitere
      Pokémon mit nicht rein levelbasierten Entwicklungen folgen, wenn die
      benötigten ROM-Tabellen verfügbar sind.
- [ ] Öffne `BOSS`. Der nächste noch nicht besiegte Boss wird angezeigt; spätere
      Bosse erscheinen als weitere Abschnitte, sofern der Spielfortschritt und
      die Guide-Daten solche Einträge enthalten.

## P1.13 Gruppierte Einstellungen und Neustart

- [ ] Öffne `SETTINGS`. `SCREEN`, `SPEED`, `COMPANION`, `BATTLE` und `SAVES`
      sind erreichbar. Die Gruppen öffnen ihre jeweiligen Unterseiten.
- [ ] Öffne `SAVES > RESTART GAME`. Die Bestätigung warnt vor dem Verlust von
      Fortschritt seit dem letzten Ingame-Save. `CANCEL` startet nicht neu.
- [ ] **Nur mit Test-Save:** Speichere im Spiel, verändere eine Position und
      bestätige `RESTART GAME`. Das Spiel startet neu und lädt nicht den alten
      Resume-Moment über den Spielstand.
- [ ] Öffne `SAVES > LEAVE GAME`. Die Bestätigung erklärt, dass der Save erhalten
      bleibt und der Moment für Resume gemerkt wird. `CANCEL` bleibt im Spiel.
- [ ] **Nur mit Test-Save:** Bestätige `LEAVE GAME`, starte den Port erneut und
      prüfe, ob du am verlassenen Moment fortsetzen kannst.

## P1.14 Geschwindigkeit

- [ ] Öffne `SETTINGS > SPEED > FAST-FORWARD`. Die Stufen reichen von `1.5x`
      bis `10x` und enthalten `INFINITE`; jede Stufe lässt sich auswählen.
- [ ] Wähle eine gut erkennbare Stufe wie `2x`. Halte im Spiel `R2`: Das Spiel
      läuft schneller und der Ton ist dabei stumm. Nach dem Loslassen läuft es
      wieder mit der normalen eingestellten Spielgeschwindigkeit.
- [ ] Aktiviere im Companion den `FF`-Schalter und schließe den Companion. Das
      Spiel bleibt ohne gehaltenes `R2` beschleunigt. Schalte `FF` wieder aus.
- [ ] Stelle unter `GAME SPEED` zum Vergleich eine langsamere und eine schnellere
      Stufe ein. Das Spiel läuft entsprechend langsamer oder schneller. Stelle
      danach `1x` wieder her.
- [ ] Schalte `SLOW MOTION (L1)` ein. Halte bei geschlossenem Companion `L1`:
      Das Spiel läuft langsamer. Nach dem Loslassen läuft es wieder normal.
      Schalte die Option aus und prüfe, dass `L1` nicht mehr verlangsamt.

## P1.15 Statusleiste

- [ ] Schalte `SETTINGS > COMPANION > STATUS BAR` ein und schließe den Companion.
      Über dem Spiel erscheint eine Zeile im Stil des OPTION-Fensters: ein kurzer
      Spielname (FIRERED, LEAFGREEN, EMERALD), Karten-Pin mit Ort,
      Pokedollar-Zeichen mit Geld, Uhrzeit und Akku-Symbol mit Prozentwert. Was
      das Gerät nicht liefert, fehlt einfach. Ein langer ROM-Dateiname darf nicht
      in der Leiste stehen.
- [ ] Ändere im Testspiel Ort und Geld und warte kurz. Die Leiste aktualisiert
      die Werte. Vergleiche Uhrzeit und Akkustand mit dem System. Bei 15% oder
      weniger wird der Akku rot, beim Laden grün.
- [ ] Öffne den Companion: Die Statusleiste verdeckt ihn nicht. Schalte die
      Option aus und prüfe, dass die Leiste über dem Spiel verschwindet.
- [ ] Schalte die Option ein, verlasse den Port und starte ihn erneut. Die
      Einstellung bleibt erhalten. Prüfe bei Bedarf alle drei `SCREEN`-Modi auf
      Lesbarkeit und störende Überdeckung.

## P1.16 Bibliothek und Backup-Wiederherstellung

**Nur mit Testkopien ausführen.** Eine Wiederherstellung ersetzt den aktuellen
Battery-Save und entfernt den alten Resume-Zustand. Ein Save-State ist kein
Battery-Save und darf nicht als Backup-Datei verwendet werden.

Der automatische Test in `linux/test/smoke.sh` prüft Navigation, Abbruch,
Wiederherstellung und den Schutz des laufenden Saves mit künstlichen Dateien.
Die folgenden Punkte prüfen zusätzlich die echte Bedienung und die SD-Karte.

- [ ] Starte den Port ohne direkt vorgegebenes Spiel. Wähle in der Bibliothek
      ein Spiel und drücke `SELECT`. `INFO` zeigt ROM-Größe, Save-Größe sowie
      vorhandenen State und Resume mit Datum oder `NONE`.
- [ ] Prüfe ein Spiel ohne Save beziehungsweise State: Fehlende Dateien werden
      als solche angezeigt. In der Liste tragen Spiele mit Save `SAVE` als Text.
- [ ] Öffne bei einem Testspiel mit Backups `INFO > A`. Eine Liste der Backups
      erscheint. Wähle einen Eintrag; vor dem Ersetzen erscheint eine
      Bestätigung mit dem ausgewählten Backup. `B` bricht ab, ohne zu ersetzen.
- [ ] **Destruktiver Test:** Wähle den vorbereiteten älteren Test-Save als Backup
      und bestätige die Wiederherstellung. Starte das Testspiel und prüfe anhand
      des notierten Unterschieds, dass der gewählte Stand geladen wurde.
- [ ] Prüfe außerhalb des Spiels, dass der zuvor aktuelle Test-Save als weiteres
      Backup existiert und der alte Resume-Zustand nicht mehr geladen wird.
      Prüfe sowohl `.sav` als auch, falls verwendet, `.srm` mit Testkopien.
- [ ] Starte ein Spiel und öffne `SETTINGS > SAVES > LIBRARY`. Das Spiel pausiert,
      der laufende Eintrag zeigt `PLAYING`. Öffne dessen `INFO`: Die
      Wiederherstellung ist gesperrt, solange der Save geöffnet ist.
- [ ] Öffne gegebenenfalls `INFO` eines anderen Spiels. Verlasse die Bibliothek
      mit `B`: Das pausierte Spiel läuft weiter. Ein Spielwechsel aus dieser
      Ansicht ist nicht implementiert.
- [ ] Achievements auf `INFO` und grafische Save-Icons sind noch nicht umgesetzt;
      Textmarkierungen sind der aktuelle Stand.

## Ergebnis und Fehlernotizen

- [ ] Stelle geänderte Einstellungen nach dem Test nach Wunsch zurück.
- [ ] Bewahre die unabhängigen Save-Kopien auf, bis du die Ergebnisse geprüft hast.
- [ ] Notiere offene oder fehlgeschlagene Punkte mit Spiel, Spielstand, konkreten
      Schritten, erwartetem und tatsächlichem Ergebnis. Sichere bei Abstürzen
      zusätzlich `log.txt` aus dem Port-Ordner; teile keine ROMs oder Saves.

| Aufgabe | Ergebnis oder Fehler | Spiel / Situation |
| --- | --- | --- |
| P1.__ | | |
| P1.__ | | |
| P1.__ | | |
