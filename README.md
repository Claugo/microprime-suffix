# microprime-suffix
Segmented sieve GC-60 for prime window analysis beyond 1e19,  first public benchmarks up to 27 digits (1e26)


**Autore:** Govi Claudio  
**Data:** Aprile 2026  
**Linguaggi:** C++ (motore di calcolo) + Python (analisi e visualizzazione)  
**Licenza:** MIT

---

## Descrizione

`microprime_suffix` è uno strumento per analizzare la distribuzione dei numeri primi all'interno di due finestre arbitrarie posizionate in qualsiasi punto della retta dei numeri,  incluse altezze irraggiungibili dai crivelli convenzionali.

Il concetto centrale è il **suffisso**: data una finestra di larghezza W che inizia all'indirizzo A, il suffisso di un numero primo p trovato dentro quella finestra è semplicemente `p - A`. Misura la posizione del primo _relativamente all'inizio della finestra_, indipendentemente da dove si trova la finestra sulla retta dei numeri.

Due finestre a altezze completamente diverse condividono un **suffisso in comune** quando entrambe contengono un numero primo alla stessa posizione relativa:

```
Finestra A inizia a    10.000  →  trova il primo 10.069  →  suffisso = 69
Finestra B inizia a   100.000  →  trova il primo 100.069  →  suffisso = 69

10.069 ≠ 100.069  ma entrambi si trovano a distanza 69 dall'inizio
della loro finestra. Suffisso 69 è in comune.
```

---

## Contesto della ricerca

Il crivello segmentato di Eratostene è ben studiato fino a circa 1e19 (il limite int64 di strumenti come **primesieve** di Kim Walisch). Oltre quella soglia **non esistono benchmark pubblici**.

Gli ultimi risultati pubblicati per il sieve di finestre ad alta quota risalgono a **Tomás Oliveira e Silva** (Università di Aveiro, Portogallo, circa 2010), misurati su un processore Athlon 900 MHz a singolo thread.

Una review sistematica della letteratura pubblicata su _MDPI Algorithms_ (aprile 2024) conclude:

> _"La generazione sistematica di numeri primi è stata quasi ignorata dagli anni 90. Al momento le tecniche di sieve sono usate principalmente per scopi didattici, e non sono stati identificati avanzamenti significativi negli ultimi 20 anni."_

Questo repository si colloca esattamente in quel vuoto, con misurazioni empiriche originali fino a **1e26 (27 cifre)** su hardware consumer moderno.

---

## Hardware utilizzato

Tutti i benchmark sono stati misurati su:

|Componente|Specifiche|
|---|---|
|CPU|AMD Ryzen 7 4800U with Radeon Graphics|
|Core / Thread|8 core / 16 thread|
|Clock base|1.80 GHz (boost fino a 4.2 GHz)|
|RAM|32 GB|
|Sistema|Windows 11, GCC/MinGW, flag: -O2 -std=c++20 -fopenmp|

Si tratta di un **processore mobile da laptop**, non di una workstation o di un server. I tempi riportati sono quindi conservativi rispetto a hardware desktop o server con clock più elevato.

---

## Benchmark misurati

Tutti i dati sono misurati empiricamente su hardware reale, non stimati.

La colonna **microprime_2** si riferisce al programma gemello che calcola una sola finestra (W = 10.000.000). La colonna **microprime_suffix** calcola due finestre (W = 100.000) e impiega circa il 16% in più per la gestione della seconda finestra.

**Nota sulla notazione:** 1e26 significa 1 seguito da 26 zeri, ovvero un numero di 27 cifre. L'esponente indica il numero di zeri, non il numero di cifre totali.

|Scala|Cifre|Cicli GC-60|microprime_2|microprime_suffix|
|---|---|---|---|---|
|1e19|20|1.273|~2 sec|2.4 sec|
|1e20|21|4.023|~6 sec|7.3 sec|
|1e21|22|12.717|~20 sec|24.5 sec|
|1e22|23|40.212|~70 sec|86 sec|
|1e23|24|127.158|~280 sec|328 sec|
|1e24|25|402.106|~1.050 sec|1.216 sec|
|1e25|26|1.271.567|**4.003 sec** (~67 min)|**4.635 sec** (~77 min)|
|1e26|27|4.021.045|**17.600 sec** (~4.9 ore)|~20.300 sec (~5.6 ore)|

Il tempo scala con **sqrt(10) ≈ 3.16** ad ogni decade — esattamente come previsto dalla teoria per un crivello segmentato. I dati misurati confermano questa proporzionalità con ottima precisione.

---

## Scoperte principali

Confrontando due finestre A e B di larghezza W, l'analisi dei suffissi rivela tre comportamenti distinti nella **curva del gap** (`s_B[i] − s_A[i]`,  la differenza tra la posizione dell'i-esimo primo di B e quella dell'i-esimo primo di A):

### 1. Rampa del gap

Con finestre adiacenti (distanza = W) a scala ~1e17, il gap parte vicino allo zero e scende progressivamente fino a circa −5.000, poi risale parzialmente. La finestra B accumula un vantaggio sistematico sulla finestra A lungo tutta la gara.

### 2. Oscillazione simmetrica

A scala ~2.5e18 con le stesse condizioni, il gap oscilla attorno allo zero senza trend sistematico. Le due finestre si alternano senza che nessuna prevalga. Esiste una scala di transizione tra i due comportamenti ancora da determinare con precisione.

### 3. Firma modulare di W — invariante di scala

Per ogni piccolo primo q, il residuo dominante di `gap(i) mod q` vale:

```
r_atteso(q) = (-W) mod q = (q - W mod q) mod q
```

Questa proprietà è **invariante rispetto alla scala** — vale identicamente a 1e14 e a 1e21. Dipende solo dalla larghezza W, non da dove si trovano le finestre sulla retta dei numeri.

### Tabella riepilogativa degli esperimenti

|Scala A|Scala B|Distanza|Gap medio|Suffissi comuni|Comportamento|
|---|---|---|---|---|---|
|1e17|1e17 + W|W|-1.674|116|Rampa negativa|
|1e17|1e17 + 1|1|-1.0|0|Piano costante|
|2^63|2^63 + 1|1|-1.0|0|Piano costante|
|2^63|2^64|2^63|-2.421|65|Rampa negativa|
|1e20|1e21|9e20|+1.226|166|Rampa positiva|
|1e17|1e17 + 2^63|2^63|+2.074|216|Rampa positiva|
|2.5e18|2.5e18 + W|W|+403|92|Oscillazione|

---

## Come si usa

**Passo 1**  Creare la cartella di output:

```
C:\dati_suffix\
```

**Passo 2** — Modificare `dati_di_ricerca.txt` con gli esperimenti desiderati:

```
# formato:  A,  B,  W
# commenti con #

# potenze di 10
10^20, 10^21, 100000

# potenze di 2
2^63, 2^64, 100000

# numeri estesi
7652376576523765, 9652376576523765, 100000

```

Sono supportate le espressioni `10^N`, `2^N`, e numeri interi di qualsiasi dimensione fino al limite teorico di 1e38 (__int128).

**Passo 3** — Lanciare `microprime_suffix.exe`  
I risultati vengono scritti in `C:\dati_suffix\suffix_data_N.csv` (un file per esperimento, numerato in ordine).

**Passo 4** — Lanciare `analisi_suffix.py`  
I grafici PNG vengono generati automaticamente per ogni CSV trovato in `C:\dati_suffix\`. Nessun argomento necessario.


---

## I grafici prodotti

Ogni grafico è composto da sei pannelli:

1. **Timeline parallela** — distribuzione spaziale dei primi nelle due finestre
2. **Densità per terzi** — quanti primi cadono nel primo, secondo e terzo terzo della finestra
3. **Gap per posizione** — la "gara" tra le due finestre, momento per momento
4. **Distribuzione dei gap** — fotografia statistica dell'intera gara
5. **Ultima cifra** — verifica del Teorema di Dirichlet (attesa: 25% per cifre 1, 3, 7, 9)
6. **Gap mod q** — la firma modulare di W, invariante di scala

Per la spiegazione dettagliata con esempi numerici consultare `docs/guida_tecnica_suffissi.pdf`.

---

## Algoritmo

Il motore usa un **crivello segmentato GC-60** — ruota di fattorizzazione mod 60, bit-packing a 64 bit, parallelizzazione OpenMP su tutti i thread disponibili.

La differenza fondamentale rispetto a un segmented sieve classico è che **non viene costruito il crivello completo fino a sqrt(N)**. Vengono raccolti solo i divisori che colpiscono le finestre richieste, saltando tutto il resto. Questo è il motivo per cui il programma raggiunge 1e26 in ~77 minuti su un laptop consumer con 16 thread, senza richiedere nessun archivio precompilato.

---

## Struttura del repository

```
microprime_suffix/
├── README.md
├── LICENSE
├── cpp/
│   └── main.cbp      <- sorgente microprime_suffix C++ (Code::Blocks + GCC/MinGW)
│   └── microprime_suffix.cbp      <- file di configurazione code::block
├── python/
│   ├── analisi_suffissi.py        <- generatore grafici PNG
├── exe/
│   ├── microprime_suffix.exe      <- motore compilato
│   ├── dati_di_ricerca.txt        <- file di input
│   └── SHA256.txt                 <- hash di verifica del programma exe
└── docs/
    └── guida_tecnica_suffissi.pdf <- guida tecnica con esempi numerici
```

---

## Dipendenze

**Per usare gli exe precompilati:** nessuna dipendenza.

**Per ricompilare il sorgente C++:**

- GCC 10 o superiore con supporto OpenMP
- Standard C++20

```
g++ -O2 -std=c++20 -fopenmp -o microprime_suffix microprime_suffix.cpp
```

**Per eseguire gli script Python:**

```
pip install pandas matplotlib reportlab
```

---

## Verifica integrità

Gli hash SHA256 degli eseguibili sono disponibili in `exe/SHA256.txt`. Per verificare su Windows (PowerShell):

```powershell
Get-FileHash microprime_suffix.exe -Algorithm SHA256
```

---

## Progetto correlato

[microprime v3](https://github.com/claudiogovi/microprime) — interfaccia grafica per la navigazione dei numeri primi tramite archivio precompilato (71 GB, fino a 25 cifre). I due progetti sono complementari: microprime v3 per la navigazione veloce su archivio preesistente, microprime_suffix per l'analisi on-demand senza limiti di scala e senza necessità di archivio.

---

_Govi Claudio — Aprile 2026_
