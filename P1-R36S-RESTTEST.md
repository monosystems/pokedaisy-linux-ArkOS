# P1: Was du zusätzlich auf dem R36S prüfen solltest

Diese Liste ergänzt [`P1-TEST.md`](P1-TEST.md). Die automatisierten Prüfungen
nutzen synthetische Daten, teilweise eine lokal vorhandene FireRed-Test-ROM,
simulierte Eingaben und den Linux-Host auf dem Entwicklungsrechner. Sie ersetzen
keinen Test mit dem R36S, seinem Controller und seiner SD-Karte.

**Stand der automatisierten Prüfung:** `linux/test/smoke.sh` und der portable
Test `make -C 3ds p1-test` bestehen. Die zuvor gefundenen Fehler bei P1.8 und
P1.14 sind behoben, aber noch auf dem R36S zu prüfen. Ein erfolgreicher
Teiltest bedeutet nicht, dass alle Fälle einer P1-Aufgabe geprüft sind.

## Vor dem Test

- [x] Installiere den zu testenden Build und notiere Commit oder Buildstand:
      _______Oct 29 23:17_____________
- [x] Sichere deine Saves und `pokedaisy-states` unabhängig außerhalb des
      ROM-Ordners. Nutze für Neustart und Wiederherstellung nur Testkopien.
- [x] Notiere Spiel, Version und Situation bei Abweichungen. Wenn ein Spielstand
      oder ein Pokémon für einen Punkt fehlt, notiere **nicht getestet**.
- [x] Prüfe, ob L2, A, B, das Steuerkreuz und L1/R1 auf deinem Gerät zuverlässig
      reagieren und der Text lesbar ist. Achte auch auf Ruckeln und Tonprobleme.

## Tab-Leiste, Party und Beutel

- [x] **P1.1:** Prüfe mit einem Spiel, das mehr Tabs als freie Plätze hat, die
      Inhalte von `MORE`, den verdrängten Tab im Kampf und die Rückkehr danach.
      Der automatische Test nutzte weniger verfügbare Tabs.
- [x] **P1.2:** Prüfe `TAB BAR` mit fünf gewählten Tabs, die Meldung beim sechsten
      Tab, das Speichern der Auswahl und die tatsächlichen Tab-Animationen.
      Blende auch nacheinander alle verfügbaren Tabs aus: Ihre Chips verschwinden,
      und `MORE` sowie das Zahnrad bleiben erreichbar.
      Prüfe die Bewegung eines gehaltenen Party-Icons, falls sie mit dem R36S
      ausgelöst werden kann.
- [x] **P1.3:** Prüfe den echten Titelschirm nach `RESTART GAME` mit einem
      Test-Save, der ein Team enthält. Vor `CONTINUE` soll `PRESS START` statt
      des Teams erscheinen. Lade danach den Save und gehe über fünf verschiedene
      Positionen: Dann soll das Team erscheinen. Wiederhole das bei Bedarf mit
      leerem Team. Die Übergänge wurden synthetisch geprüft, nicht auf dem R36S.
- [x] **P1.4:** Vergleiche angezeigte gehaltene Items und den Statusfall mit dem
      Spiel. Der Test prüft eine veränderte Darstellung, nicht den Itemtext oder
      jedes Symbol.
- [x] **P1.5–P1.6:** Vergleiche EXP, `NEXT`/`MAX`, Matchups, Werte, Wesen und
      Hidden Power mit deinem Spiel beziehungsweise einer unabhängigen Quelle.
      Automatisch wurden einzelne bekannte Beispiele geprüft, nicht jede Art,
      jedes Level oder jede Kombination.
- [x] **P1.7:** Prüfe die tatsächliche Reihenfolge der Items bei `DEFAULT`,
      `NAME`, `TYPE` und `COUNT` sowie TM-Namen in deinen Spielen. Der Test
      schaltet die Sortierungen durch und prüft eine FireRed-TM, aber nicht alle
      Sortierergebnisse und ROM-Versionen.
- [x] **P1.8:** Wähle ein Item mit `A`, gehe mit `RECHTS` zum Stern und schalte
      ihn mit `A` ein. Mit `LINKS` kehrst du zum ausgewählten Item zurück. Prüfe
      auch ein anderes Item nach dem Scrollen sowie die Favoriten nach Neustart
      und Spielwechsel. Alte Favoriten werden erhalten, aber nicht automatisch
      zugeordnet; markiere sie bei Bedarf erneut.
- [x] **P1.9:** Prüfe Aussehen, Lesbarkeit und Navigation des Beutels auf dem
      R36S in FireRed und Emerald. Gehe mit `UNTEN` bis zum letzten Item und mit
      `OBEN` zurück; `PC` darf dabei nicht unbeabsichtigt den Fokus erhalten.
      Prüfe auch `A` auf dem letzten Item und den Weg mit `LINKS` zu den Taschen.
      Die Preview ist kein Gerätetest.

## Pokédex, Kampf und Guide

- [x] **P1.10:** Vergleiche die Zahlen aller Filter mit deinem Pokédex und
      prüfe Vor/Zurück auch bei mehreren Treffern. Scrolle mit `UNTEN` weit nach
      unten und mit `OBEN` zurück zum ersten Eintrag: Der Cursor muss sichtbar
      bleiben und jeder Eintrag genau einmal erreicht werden. Der Test nutzte
      einen synthetischen gefangenen Eintrag und eine lokale FireRed-Test-ROM.
- [x] **P1.11:** Vergleiche Schwächen, Resistenzen und Vorschläge in echten
      Kämpfen. Prüfe Scrollen und `BATTLE HINTS` an und aus. Automatisch wurde
      der Schalter geprüft, nicht jede Bewertung oder jeder Kampfzustand.
- [x] **P1.12:** Vergleiche Party-Entwicklungen und die Reihenfolge der Bosse
      mit deinem Fortschritt. Automatisch wurde das Öffnen der Seiten geprüft,
      nicht die Vollständigkeit aller Einträge.

## Einstellungen, Geschwindigkeit und Statusleiste

- [x] **P1.13:** Prüfe Navigation und Warntexte am Gerät. Prüfe Neustart und
      Resume nur mit Test-Save. Automatisch wurden Bestätigung und Abbruch der
      Oberfläche geprüft, nicht ein echter Neustart deines Spiels.
- [x] **P1.14:** Prüfe Geschwindigkeit, Ton, R2, FF und L1 am Gerät. Prüfe
      insbesondere beide Auswahllisten, `INFINITE` und die gespeicherten Werte
      nach einem Neustart. Automatisiert sind Auswahl und Speicherung geprüft;
      die tatsächliche Spielgeschwindigkeit, der Ton und die Tasten am R36S
      sind damit nicht nachgewiesen.
- [x] **P1.15:** Prüfe, dass links der kurze Spielname steht (FIRERED,
      LEAFGREEN oder EMERALD) und nicht der Dateiname der ROM. Vergleiche dann
      Ort, Geld, Uhrzeit und Akku mit dem Gerät und Spiel. Prüfe das Karten-Pin- und das Pokedollar-Symbol, den Akku mit
      Prozentwert, die rote Farbe bei 15% oder weniger und das Laden. Prüfe die
      Lesbarkeit in den drei Bildschirmmodi und ob die Leiste über dem Spiel,
      aber nicht über dem Companion liegt. Der Desktop-Test kann den Akku nicht
      belegen, weil er keinen hat.

## Bibliothek und SD-Karte

- [x] **P1.16:** Öffne `INFO` und die pausierte Bibliothek mit dem R36S-Controller.
      Prüfe `PLAYING`, `SAVE`, Dateigrößen, Datumsangaben und Rückkehr ins Spiel.
      Der automatisierte Test verwendet simulierte Eingaben.
- [ ] **P1.16, optional und nur mit Testkopien:** Prüfe eine Wiederherstellung
      auf der SD-Karte inklusive anschließendem Spielstart und Schutz des
      vorherigen Saves. Der automatische Test prüft Dateiinhalte auf dem
      Entwicklungsrechner, nicht das exFAT-Dateisystem des Geräts oder das Laden
      eines echten Spielstands.
- [ ] Stelle deine gewünschten Einstellungen wieder her und bewahre die
      unabhängigen Save-Kopien auf, bis alle Ergebnisse geprüft sind.
