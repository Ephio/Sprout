# Sprout (AMB-1 × Pocket Scion)

**Sprout** è il nome di questa versione, di Paolo Pino. È derivata da AMB-1 1.3.2 di Charles Vestal (MPL-2.0; i crediti
di AMB-1, fm1-base e Felucca restano nel progetto). Il nome visibile (USB, dispositivo) è "Sprout" / "Sprout FM-1"; i file prodotti dalla compilazione si chiamano ancora **amb.fwsc** (il workflow e i test li chiamano così): puoi
rinominare il file scaricato in `sprout.fwsc` senza problemi.

Sprout risponde via MIDI agli impulsi dell'**Instruo Pocket Scion**.
Gli **otto pezzi** (manopola PRESETS) sono otto scene grafiche, e **tutte** rispondono al Pocket Scion.

## Collegamento

Pocket Scion → FM-1 con cavo MIDI TRS (o USB). Nell'app *Pocket SCION Controller* usa **MULTI CHANNEL** con le voci sui
canali **1–5**.

| Messaggio | Effetto sull'FM-1 |
| --- | --- |
| Note On, canali 1–5 | suona una campana (FM) con timbro e posizione stereo propri del canale: 1 a sinistra … 5 a destra. La nota è suonata com'è: le impostazioni di ottava del Pocket Scion restano tue. |
| Note Off / velocity 0 / canali 6–16 | ignorati (le campane si spengono da sole) |
| Control Change | secondo la **mappa dei CC** (vedi sotto). Di base CC 71–75 comandano la luminosità del timbro delle voci 1–5 (0 = tonda, 127 = metallica) |
| Program Change 0–7 | sceglie il pezzo |
| MIDI Clock / Start / Stop | come in AMB-1 (sincronizza la griglia) |

## Le pagine nuove

**Granulatore (tasto SEQ)** sostituisce la pagina delle percussioni (che non c'è più: il Pulse resta spento).
Registra gli ultimi ~1,4 secondi del suono del giardino (pad, basso, campane e note del Scion) e lo riproduce a
"grani": pezzetti di passato con una finestra morbida, da pochi al secondo fino a una nuvola. I grani vanno al riverbero.

| Manopola | Cosa fa |
| --- | --- |
| **Granules** | quanti grani al secondo (da 1 a 33). **0 = spento** (all'inizio è spento in tutti i pezzi) |
| **Size** | durata di ogni grano, da 25 a 250 ms |
| **Reverse** | percentuale di grani suonati al contrario |
| **Random** | quanto si allontanano: nei tempi, in che punto del passato leggono, nell'altezza (ottava sotto, quinta, ottava sopra) e nello spazio stereo |

**Mappa dei CC (tasto GLO, premuto due volte)**: la prima pressione è Setup (come prima), la seconda apre la mappa;
una terza torna a Setup. Si raggiunge anche con la manopola SELECT. Si imposta una voce del Scion alla volta:

| Manopola | Cosa fa |
| --- | --- |
| **Voice** | la voce del Scion da impostare (1–5) |
| **CC** | il numero di CC di quella voce (di base 71, 72, 73, 74, 75: quelli dell'app Pocket SCION) |
| **Target** | dove va quel CC (elenco sotto) |
| **Depth** | quanto (0–100%): il CC a 64 non cambia nulla, a 0 e 127 sposta fino all'estremo |

Destinazioni: **Off**; della voce stessa: **Bright** (tono), **Level** (volume), **Decay** (durata), **Pan**, **Pitch**
(fino a ±2 semitoni); del giardino: **Echo**, **Reverb**, **Grains** (Granules), **GrSize**, **GrRev**, **GrRand**,
**Pad**, **Bass**, **Bells** (livello delle voci Scion), **Tone** (timbro delle campane). Per le impostazioni del giardino
il CC muove il valore *attorno a dov'è la sua manopola*: la manopola non si muove sullo schermo, cambia solo ciò che si
sente. La mappa si salva col progetto (tasto SAVE).

## Gli otto pezzi

Ogni scena reagisce alle note; in basso cinque pallini si accendono per canale (utile per vedere il collegamento).

| # | Nome | Musica | Scena e reazione alle note |
| --- | --- | --- | --- |
| 1 | **Garden** | letto discreto in Re pentatonica minore | ciliegio giapponese fitto con laghetto e lanterna: i fiori di un ramo si accendono, cadono petali, un'onda sul laghetto |
| 2 | **Mycelium** | letto in La pentatonica maggiore | cinque reti di micelio, una per canale: crescono e lampeggiano |
| 3 | **Flora** | letto in Fa maggiore 7 (F A C E) | una Monstera che **cresce con le note** (nuove foglie, stelo più alto); la foglia del canale si muove; senza note torna piano a due foglie; la luce della finestra e il polline salgono |
| 4 | **Earth** | letto cromatico, gocce corte | globo che ruota (più veloce con il Tempo); ogni nota è una goccia di pioggia |
| 5 | **Posidonia** | musica di Departures (AMB-1) | prateria di posidonia che ondeggia, raggi di sole spezzettati; una nota agita le alghe, accende un raggio, libera una bolla o un pesce |
| 6 | **Night** | musica di Voices (AMB-1) | riva di un fiume al chiaro di luna, canne al vento; una nota è una folata, si alzano lucciole, increspature |
| 7 | **Lighthouse** | musica di Afternoon (AMB-1) | tempesta su una costa nordica, il faro con il suo fascio; una nota è un'onda che si infrange, ogni tanto un lampo |
| 8 | **Shoreline** | musica di Shoreline (AMB-1) | tramonto dal muretto sulla spiaggia: due innamorati di spalle (spostati a destra), il camioncino dei gelati a sinistra; una nota fa brillare il mare e accende una lampadina del camioncino (una per canale), passa un gabbiano, ogni tanto sale un cuore |

I pezzi 1–4 hanno un "letto" ambient discreto sotto le note del Scion (Density 0: il livello **Bells** è il volume delle
voci Scion). I pezzi 5–8 mantengono la musica generativa di AMB-1 (cicli di campane, Pad, Basso), con le note
del Scion sopra; le percussioni sono state tolte da tutti. Effetti e manopole sono quelli di AMB-1. L'intestazione dei pezzi 1–4 mostra la scala delle note del
Scion; sugli altri, tonalità e modo del letto. L'interfaccia ha lo sfondo scuro "Notte".

## Scelte che puoi cambiare (chiedimi e le modifico)

* Le destinazioni della mappa dei CC (e i valori iniziali): non conosco la semantica esatta dei CC del Pocket Scion.
* Il granulatore: il suo buffer è di 1,4 s a 11 kHz (volutamente "nastro"); l'eco a nastro è stata accorciata da 5,6 a 4,2 s per fargli posto (a tempi molto lenti, i ritardi più lunghi scelgono quello più corto).
* Il timbro di ogni pezzo e canale: tabella `SC_STYLE` in `app/dsp/amb.c`.
* Le scene sono in `app/scenes.c` (una funzione per scena).
* Le note del Scion non fanno ancora seguire gli accordi al "letto": possibile evoluzione.

## Stato dei test

Eseguiti qui: test del motore audio (`tests/host/amb_test.c`, con test per le voci Scion, il panning dei canali, le
differenze fra preset, i CC, una raffica di note, le scale Maj7 e cromatica, che ogni pezzo suoni le note Scion, il granulatore, anche agli estremi, e la mappa dei CC),
build del simulatore host di fm1-base e lo scenario `tests/scenarios/scion.amb` (MIDI, scale, cambio pezzo,
screenshot). **Non eseguiti qui**: la build per dispositivo (toolchain JieLi: lì la build controlla anche che la memoria basti; i conti dicono di sì, con un margine di ~12 KB), i test dei browser (Emscripten) e la
prova sull'FM-1 reale. Gli scenari originali di AMB-1 dipendono dai valori dei vecchi pezzi e sono in
`tests/scenarios_amb1/` (non eseguiti). Le scene sono state provate solo nel simulatore: il frame rate sul display
reale è da verificare.

## Compilare (senza toolchain locale): GitHub Actions

1. Crea un account GitHub e fai **Fork** di <https://github.com/charlesvestal/fm1-amb>.
2. Nel fork crea un ramo `scion` dal tag `v1.3.2` e carica i file di questa cartella (`app/`, `tests/`,
   `tools/gen_land.py`, `docs/SCION.md`, `web/app/index.html`); elimina i vecchi `tests/scenarios/*.amb` tranne
   `battery.amb` e `scion.amb`.
3. Tab *Actions* → **Build and attest firmware** → *Run workflow* sul ramo `scion`.
4. A build finita, scarica l'artifact **packages**: dentro c'è `amb.fwsc`.
5. Prova il `.fwsc` con il web installer di fm1-base, poi sull'FM-1 (l'installer può rimettere il firmware originale).
