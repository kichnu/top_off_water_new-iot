# Score / EMA Chart — rozważania projektowe

## 1. Aktualna implementacja stref (wykresu evaporation rate)

### Formuła przeliczenia score-thresholds → ml/h

```
dGreen  = 2^(zone_green  / (alarm_p1 × 100)) − 1
dYellow = 2^(zone_yellow / (alarm_p1 × 100)) − 1

bandW_green  = ema_rate × dGreen
bandW_yellow = ema_rate × (dYellow − dGreen)

yMax = ema_rate + 2 × bandW_yellow   ← góra wykresu
yMin = max(0, ema_rate − bandW_green) ← dół wykresu
```

### Granice bandów (ml/h)
```
bGreenBot  = ema_rate − bandW_green   (często < 0, clamp do 0)
bGreenTop  = ema_rate + bandW_green
bYellowTop = ema_rate + bandW_green + bandW_yellow
```

### Obserwowane proporcje wizualne (p1=0.10)

| zone_green | zone_yellow | zielony | żółty | czerwony |
|-----------|------------|---------|-------|----------|
| 80        | 100        | 1/6     | 3/6   | 2/6      |
| 90        | 100        | 3/6     | 3/6   | ~0       |
| 95        | 100        | ~100%   | ~0    | ~0       |

Przy zone_green=95, zone_yellow=100: bGreenTop (724×ema) > yMax (601×ema)
→ zielony wychodzi poza górę wykresu → cały wykres wygląda na zielony.

### Żółty jest TYLKO nad EMA (asymetryczny)
Czerwony na dole pojawia się tylko gdy bGreenBot > 0, co jest niemożliwe
gdy dGreen ≥ 1 (czyli zone_green ≥ alarm_p1×100).

### Steady-state score dla trwałego d
```
score_max = p1×100 × (1+d)/p2 × log2(1+d)
```
Przy d=1.236, p1=0.10, p2=0.30: score_max ≈ 87

### Intuicyjna reguła (p1=0.10)
- rate = 2× EMA (d=1.0) → +10 pkt/cykl
- rate = 1.5× EMA (d=0.5) → +5.8 pkt/cykl
- rate = 3× EMA (d=2.0) → +15.8 pkt/cykl

---

## 2. Propozycja nowej prezentacji (do przemyślenia)

### Założenia
- Zielony + żółty = stałe 80% wysokości bandu
- Czerwony = zawsze widoczny górne 20%
- Proporcja zielony/żółty = liniowa wg zone_green/zone_yellow (nie wykładnicza)
- Semantyka zone_green i zone_yellow zmienia się z "score threshold" na ml/h
- Oś Y: skala ml/h, bandy stałe, linia EMA i punkty dolewek ruchome

### Co to zmienia vs obecne
| Aspekt | Obecne | Proponowane |
|--------|--------|-------------|
| Próg alarmu | odchylenie od bieżącego EMA (względny) | przekroczenie stałej wartości ml/h (bezwzględny) |
| Adaptacja | EMA dryftuje, progi z nim | progi stałe, EMA może wyjść poza nie trwale |
| Wykrywa | trendy wzrostowe | poziom bezwzględny |
| Czerwony | może zniknąć | zawsze widoczny |
| Proporcje | nieprzewidywalne (wykładnicze) | liniowe, czytelne |

### Kluczowe pytanie bez odpowiedzi
Czy sezonowy dryft parowania (np. zima→lato) jest normalny i akceptowalny,
czy powinien wyzwalać alarm?
- TAK → obecne (adaptacyjne) podejście jest lepsze
- NIE → proponowane (absolutne) jest prostsze i czytelniejsze

### Dodatkowe pytanie
Gdzie leży linia EMA na nowym wykresie?
- Wariant A: EMA na granicy zielony/żółty (asymetryczny bufor)
- Wariant B: EMA wewnątrz zielonego (symetryczny, ale rozbija stałe proporcje)

---

## 3. Relacja score → ml/h (wyjaśnienie)

Score NIE jest wartością ml/h — to skumulowane "ciśnienie alarmu" bez jednostek.

```
d = (rate_tego_cyklu − EMA) / EMA

jeśli d > 0:
  instant = p1×100 × log2(1+d)
  decay   = p2 / (1+d)
  score   = score × (1−decay) + instant
jeśli d ≤ 0:
  factor = min(1, p2×(1+|d|))
  score  = max(0, score×(1−factor))
```

Steady-state (score przy trwałym d): `score_ss = p1×100 × (1+d)/p2 × log2(1+d)`

Nie ma bezpośredniego przelicznika score↔ml/h, bo score jest całkowy
(te same score mogą wynikać z wielu małych lub jednej dużej dewiacji).

---

## 4. Bugi naprawione przy okazji analizy

- `resetCycleHistory()` nie zapisywało zresetowanego EMA do FRAM
  → po restarcie wracał stary EMA z FRAM
  → naprawione: dodano `saveEmaToFRAM()` po resecie

- `resetCycleHistory()` nie resetowało `alarmArmed=true`
  → po kasowaniu historii system nie generował nowych alarmów RED
  → naprawione: dodano `alarmArmed=true` i `totalCycleCount=0`

---

## 5. fetch_sim.py — bug w backtrack (naprawiony)

`rate=0.0` (pierwszy cykl po resecie, brak poprzedniego timestamp) nie aktualizuje
EMA w firmware. Backtrack musi pomijać te rekordy:

```python
for rate in reversed(rates_oldest_first):
    if rate == 0.0:
        continue  # firmware też pomija, nie odwracaj
    ...
```

Bez tej poprawki EMA₀ był zawyżony (~54 zamiast ~50), co dawało:
- EMA na końcu: 68 zamiast rzeczywistych 63 ml/h
- Score na końcu: 223 zamiast rzeczywistych 232.8
