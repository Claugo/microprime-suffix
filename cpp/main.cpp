// microprime_suffix.cpp
// Autore: Govi Claudio
// Mese: 04
// Anno: 2026
// ============================================================
//  MicroPrime Studio - Analisi Suffissi su Due Finestre
//
//  Base: microprime_2.cpp (GC-60, bit-packing, OpenMP)
//
//  Uso:
//    microprime_suffix <prefix_A> <prefix_B> <W>
//
//  Esempio:
//    microprime_suffix 1000 1003 1000
//
//  Output:
//    suffix_data.csv  (prefix,suffix  -- un primo per riga)
//
//  Funzionamento:
//    Fase 1+2 - Setaccio GC-60 fino a sqrt(max_target + W)
//               I divisori utili vengono raccolti per ENTRAMBE
//               le finestre in un solo passaggio parallelo.
//    Fase 3   - applica_finestra_passiva() eseguita due volte,
//               una per ciascun prefix.
//    Output   - CSV con colonne prefix,suffix (interi puri).
//               Python legge e visualizza.
// ============================================================

#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <algorithm>
#include <sys/stat.h>
#include <omp.h>
#include <bit>

// ============================================================
// __int128: supporto stampa e operazioni
// ============================================================
using i128 = __int128;

std::string i128_to_str(i128 n) {
    if (n == 0) return "0";
    bool neg = (n < 0);
    if (neg) n = -n;
    std::string s;
    while (n > 0) {
        s = char('0' + (int)(n % 10)) + s;
        n /= 10;
    }
    if (neg) s = "-" + s;
    return s;
}

std::string fmt128(i128 n) {
    std::string s = i128_to_str(n);
    int ins = (int)s.size() - 3;
    while (ins > 0) { s.insert(ins, "."); ins -= 3; }
    return s;
}

std::string fmt(long long n) {
    std::string s = std::to_string(n);
    int ins = (int)s.size() - 3;
    while (ins > 0) { s.insert(ins, "."); ins -= 3; }
    return s;
}

i128 isqrt128(i128 n) {
    if (n <= 0) return 0;
    i128 x = (i128)sqrtl((long double)n);
    while (x * x > n) x--;
    while ((x+1)*(x+1) <= n) x++;
    return x;
}

// ============================================================
// Parametri GC-60
// ============================================================
const long long DIMENSIONE_MASCHERA = 131072;
const int SOTTOLISTA_BASE[16] = { 1,3,7,9,13,19,21,27,31,33,37,39,43,49,51,57 };
int lookup[60];

struct CicloRisultato {
    long long lista[16];
    int       numero[16];
    int       len;
};

void ricerca_ciclo(long long p, long long riferimento, CicloRisultato& out) {
    out.len = 0;
    long long start;
    if (riferimento == 0) {
        start = p * p;
    } else {
        long long A = riferimento * 60 + 10;
        start = A - (A % p);
        if (start % 2 == 0) start += p;
        else                 start += 2 * p;
    }

    long long prodotto = start;
    long long p_lista  = (prodotto - 10) / 60 - riferimento;
    int       p_numero = (int)((prodotto - 10) % 60);

    if (lookup[p_numero] >= 0) {
        out.lista[out.len] = p_lista;
        out.numero[out.len] = p_numero;
        out.len++;
    } else {
        for (int i = 0; i < 5; i++) {
            prodotto += 2 * p;
            p_lista  = (prodotto - 10) / 60 - riferimento;
            p_numero = (int)((prodotto - 10) % 60);
            if (lookup[p_numero] >= 0) {
                out.lista[out.len] = p_lista;
                out.numero[out.len] = p_numero;
                out.len++;
                break;
            }
        }
    }
    if (out.len == 0) return;
    int termina = p_numero;

    for (int i = 0; i < 1000000; i++) {
        prodotto += p * 2;
        p_numero  = (int)((prodotto - 10) % 60);
        if (p_numero == termina) break;
        if (lookup[p_numero] >= 0) {
            p_lista = (prodotto - 10) / 60 - riferimento;
            out.lista[out.len] = p_lista;
            out.numero[out.len] = p_numero;
            out.len++;
        }
    }
}

// ============================================================
// Restituisce true se p ha almeno un multiplo in [a_start, a_start+W)
// ============================================================
bool e_divisore_utile(long long p, i128 a_start, i128 fine) {
    i128 st = a_start - (a_start % (i128)p);
    if (st % 2 == 0) st += (i128)p;
    else              st += (i128)(2 * p);
    return (st < fine);
}

// ============================================================
// Buffer circolare 25 primi (invariato da microprime_2)
// ============================================================
struct PrimeWindow {
    static const int CAPACITY = 25;
    i128 data[CAPACITY];
    int  count = 0;
    int  head  = 0;
    bool full()  const { return count == CAPACITY; }
    bool empty() const { return count == 0; }
    void push(i128 p) {
        int slot;
        if (count < CAPACITY) { slot = (head + count) % CAPACITY; count++; }
        else { slot = head; head = (head + 1) % CAPACITY; }
        data[slot] = p;
    }
    i128 element(int i) const { return data[(head + i) % CAPACITY]; }
};

// ============================================================
// FASE 3 - Finestra passiva
// Proietta i divisori su [a_target, a_target+W) e raccoglie
// i sopravvissuti come coppie (prefix, suffix).
// Scrive direttamente nel file CSV aperto dal chiamante.
// ============================================================
long long applica_finestra_e_scrivi(
    const std::vector<long long>& divisori,
    i128        a_target,
    long long   W,
    i128        prefix,
    std::ofstream& csv)
{
    std::vector<uint8_t> finestra((size_t)(W + 1), 1);

    for (long long p : divisori) {
        i128 resto = a_target % (i128)p;
        i128 off   = (resto == 0) ? 0 : ((i128)p - resto);
        long long idx = (long long)off;
        for (; idx <= W; idx += p)
            finestra[(size_t)idx] = 0;
    }

    long long trovati = 0;
    std::string prefix_str = i128_to_str(prefix);
    for (long long i = 0; i <= W; i++) {
        if (finestra[(size_t)i]) {
            i128 n = a_target + (i128)i;
            if (n % 2 != 0 && n % 3 != 0 && n % 5 != 0 && n % 7 != 0) {
                // Scrivi: prefix (stringa esatta),suffix
                csv << prefix_str << "," << i << "\n";
                trovati++;
            }
        }
    }
    return trovati;
}

// ============================================================
// Bootstrap primers
// ============================================================
std::vector<long long> lista_primi_bootstrap = {
    7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,
    101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199,211,
    223,227,229,233,239,241,251,257,263,269,271,277,281,283,293,307,311,313,317,331,337,347,
    349,353,359,367,373,379,383,389,397,401,409,419,421,431,433,439,443,449,457,461,463,467,
    479,487,491,499,503,509,521,523,541,547,557,563,569,571,577,587,593,599,601,607,613,617,
    619,631,641,643,647,653,659,661,673,677,683,691,701,709,719,727,733,739,743,751,757,761,
    769,773,787,797,809,811,821,823,827,829,839,853,857,859,863,877,881,883,887,907,911,919,
    929,937,941,947,953,967,971,977,983,991,997,1009,1013,1019,1021,1031,1033,1039,1049,1051,
    1061,1063,1069,1087,1091,1093,1097,1103,1109,1117,1123,1129,1151,1153,1163,1171,1181,1187,
    1193,1201,1213,1217,1223,1229,1231,1237,1249,1259,1277,1279,1283,1289,1291,1297,1301,1303,
    1307,1319,1321,1327,1361,1367,1373,1381,1399,1409,1423,1427,1429,1433,1439,1447,1451,1453,
    1459,1471,1481,1483,1487,1489,1493,1499,1511,1523,1531,1543,1549,1553,1559,1567,1571,1579,
    1583,1597,1601,1607,1609,1613,1619,1621,1627,1637,1657,1663,1667,1669,1693,1697,1699,1709,
    1721,1723,1733,1741,1747,1753,1759,1777,1783,1787,1789,1801,1811,1823,1831,1847,1861,1867,
    1871,1873,1877,1879,1889,1901,1907,1913,1931,1933,1949,1951,1973,1979,1987,1993,1997,1999,
    2003,2011,2017,2027,2029,2039,2053,2063,2069,2081,2083,2087,2089,2099,2111,2113,2129,2131,
    2137,2141,2143,2153,2161,2179,2203,2207,2213,2221,2237,2239,2243,2251,2267,2269,2273,2281,
    2287,2293,2297,2309,2311,2333,2339,2341,2347,2351,2357,2371,2377,2381,2383,2389,2393,2399,
    2411,2417,2423,2437,2441,2447,2459,2467,2473,2477,2503,2521,2531,2539,2543,2549,2551,2557,
    2579,2591,2593,2609,2617,2621,2633,2647,2657,2659,2663,2671,2677,2683,2687,2689,2693,2699,
    2707,2711,2713,2719,2729,2731,2741,2749,2753,2767,2777,2789,2791,2797,2801,2803
};

// ============================================================
// Parsing i128 da stringa decimale (supporta numeri > long long)
// ============================================================
i128 parse_i128(const std::string& s) {
    i128 result = 0;
    bool neg = false;
    size_t i = 0;
    if (s[i] == '-') { neg = true; i++; }
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        result = result * 10 + (s[i] - '0');
        i++;
    }
    return neg ? -result : result;
}

// ============================================================
// Valuta espressioni: "10^20", "2^64", oppure numero puro
// Supporta anche offset: "10^20+1", "2^63-1"
// ============================================================
i128 valuta_expr(std::string s) {
    // rimuovi spazi
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());

    // cerca "10^" o "2^"
    auto hat = s.find('^');
    if (hat != std::string::npos) {
        std::string base_str = s.substr(0, hat);
        // separa eventuale offset (+N o -N) dopo l'esponente
        std::string exp_str;
        std::string offset_str = "0";
        size_t plus  = s.find('+', hat);
        size_t minus = s.find('-', hat);
        size_t off   = std::string::npos;
        if (plus  != std::string::npos) off = plus;
        if (minus != std::string::npos && (off == std::string::npos || minus < off)) off = minus;

        if (off != std::string::npos) {
            exp_str    = s.substr(hat + 1, off - hat - 1);
            offset_str = s.substr(off);   // include il segno
        } else {
            exp_str = s.substr(hat + 1);
        }

        i128 base   = parse_i128(base_str);
        int  esp    = std::stoi(exp_str);
        i128 result = 1;
        for (int k = 0; k < esp; k++) result *= base;

        // applica offset
        if (offset_str != "0") result += parse_i128(offset_str);
        return result;
    }

    // numero puro
    return parse_i128(s);
}

// ============================================================
// Legge una riga del file di configurazione
// Formato: A, B, W   (una riga per esperimento)
// Commenti: righe che iniziano con #
// ============================================================
bool leggi_riga(const std::string& riga, i128& A, i128& B, long long& W) {
    // salta commenti e righe vuote
    std::string r = riga;
    auto comm = r.find('#');
    if (comm != std::string::npos) r = r.substr(0, comm);
    if (r.find_first_not_of(" \t\r\n") == std::string::npos) return false;

    // split su virgola
    std::vector<std::string> tok;
    std::stringstream ss(r);
    std::string t;
    while (std::getline(ss, t, ',')) {
        t.erase(0, t.find_first_not_of(" \t"));
        t.erase(t.find_last_not_of(" \t\r\n") + 1);
        if (!t.empty()) tok.push_back(t);
    }
    if (tok.size() < 3) {
        std::cerr << "  Riga malformata (servono 3 valori): " << riga << "\n";
        return false;
    }

    A = valuta_expr(tok[0]);
    B = valuta_expr(tok[1]);
    W = (long long)valuta_expr(tok[2]);
    return true;
}

// ============================================================
// MAIN
// ============================================================
int main(int argc, char* argv[]) {

    // ============================================================
    // PERCORSI FISSI
    // Il file di input viene letto dalla cartella dell'exe (argv[0])
    // I CSV di output vengono scritti in OUTPUT_DIR
    // Se OUTPUT_DIR non esiste il programma avvisa e si ferma
    // ============================================================
    const std::string OUTPUT_DIR = "C:\\dati_suffix\\";

    // Verifica che la cartella di output esista
    {
        struct stat st;
        if (stat(OUTPUT_DIR.c_str(), &st) != 0 || !(st.st_mode & S_IFDIR)) {
            std::cerr << "\n";
            std::cerr << "  ERRORE: cartella di output non trovata:\n";
            std::cerr << "  " << OUTPUT_DIR << "\n";
            std::cerr << "\n";
            std::cerr << "  Crea la cartella manualmente oppure cambia OUTPUT_DIR\n";
            std::cerr << "  nel sorgente (cerca la riga con OUTPUT_DIR).\n";
            std::cerr << "\nPremi INVIO per chiudere...\n";
            std::cin.get();
            return 1;
        }
    }

    // File di input: cerca nella stessa cartella dell'exe
    std::string exe_dir;
    {
        std::string exe_path = argv[0];
        size_t last_sep = exe_path.find_last_of("/\\");
        if (last_sep != std::string::npos)
            exe_dir = exe_path.substr(0, last_sep + 1);
    }

    std::string cfg_file;
    if (argc >= 2) {
        cfg_file = argv[1];
    } else {
        cfg_file = exe_dir + "dati_di_ricerca.txt";
    }

    std::ifstream fin(cfg_file);
    if (!fin) {
        std::cerr << "File non trovato: " << cfg_file << "\n";
        std::cerr << "Crea il file con righe nel formato:\n";
        std::cerr << "  A, B, W\n";
        std::cerr << "Esempi:\n";
        std::cerr << "  10^20, 10^21, 10^6\n";
        std::cerr << "  2^63, 2^64, 100000\n";
        std::cerr << "  7652376576523765, 876238768786223, 10^6\n";
        std::cerr << "  # commenti con #\n";
        return 1;
    }

    // Raccoglie tutti gli esperimenti validi dal file
    std::vector<std::tuple<i128,i128,long long>> esperimenti;
    std::string riga;
    int riga_num = 0;
    while (std::getline(fin, riga)) {
        riga_num++;
        i128 A, B; long long W;
        if (leggi_riga(riga, A, B, W))
            esperimenti.emplace_back(A, B, W);
    }
    fin.close();

    if (esperimenti.empty()) {
        std::cerr << "Nessun esperimento valido trovato in " << cfg_file << "\n";
        return 1;
    }

    std::cout << "================================================\n";
    std::cout << "   MicroPrime Suffix  04/2026  GC-60\n";
    std::cout << "   Input : " << cfg_file << "\n";
    std::cout << "   Output: " << OUTPUT_DIR << "\n";
    std::cout << "   Esperimenti trovati: " << esperimenti.size() << "\n";
    std::cout << "================================================\n\n";

    // Salva il bootstrap originale — va ripristinato ad ogni esperimento
    // perché la fase 1 lo modifica (clear + push_back)
    const std::vector<long long> bootstrap_originale = lista_primi_bootstrap;

    // Esegue ogni esperimento
    int n_exp = 0;
    for (auto& [tA, tB, W] : esperimenti) {
        n_exp++;

        // Ripristina il bootstrap per questo esperimento
        lista_primi_bootstrap = bootstrap_originale;

        i128 target_A = tA;
        i128 target_B = tB;

        if (target_A > target_B) std::swap(target_A, target_B);
        if (W <= 0) {
            std::cerr << "  Esperimento " << n_exp << ": W deve essere > 0, saltato.\n";
            continue;
        }

        // La radice massima si calcola sulla finestra piu' lontana
        i128 fine_B     = target_B + (i128)W;
        i128 radice_max = isqrt128(fine_B) + 1;

        long long quanti_cicli = (long long)(radice_max / ((i128)DIMENSIONE_MASCHERA * 60)) + 2;

        std::cout << "================================================\n";
        std::cout << "   Esperimento " << n_exp << " / " << esperimenti.size() << "\n";
        std::cout << "================================================\n";
        std::cout << "Finestra A : [" << fmt128(target_A)
                  << ", " << fmt128(target_A + (i128)W) << ")\n";
        std::cout << "Finestra B : [" << fmt128(target_B)
                  << ", " << fmt128(target_B + (i128)W) << ")\n";
        std::cout << "W          : " << W << "\n";
        std::cout << "Distanza   : " << fmt128(target_B - target_A) << "\n";
        std::cout << "Radice max : " << fmt128(radice_max) << "\n";
        std::cout << "Cicli GC-60: " << quanti_cicli << "\n";
        std::cout << "Thread OMP : " << omp_get_max_threads() << "\n";
        std::cout << "================================================\n\n";

    memset(lookup, -1, sizeof(lookup));
    for (int i = 0; i < 16; i++) lookup[SOTTOLISTA_BASE[i]] = i;

    long long total_bits = 16 * DIMENSIONE_MASCHERA;
    long long array_size = total_bits / 64;

    // I divisori utili per A e per B vengono raccolti insieme
    // (un divisore puo' essere utile per entrambe le finestre)
    std::vector<long long> divisori_fase1;
    std::vector<std::vector<long long>> divisori_per_ciclo(quanti_cicli - 1);

    auto t0 = std::chrono::high_resolution_clock::now();

    // ============================================================
    // FASE 1: Segmento 0 - seriale
    // ============================================================
    {
        std::vector<uint64_t> maschera_0(array_size, 0);
        CicloRisultato ciclo_buf;
        long long radice_0 = (long long)std::sqrt((double)(DIMENSIONE_MASCHERA * 60)) + 1;

        for (long long p : lista_primi_bootstrap) {
            if (p > radice_0) break;
            ricerca_ciclo(p, 0, ciclo_buf);
            if (ciclo_buf.len == 0) continue;
            for (long long ii = 0; ; ii++) {
                bool stop = false;
                for (int i = 0; i < ciclo_buf.len; i++) {
                    long long indice = ciclo_buf.lista[i] + p * ii;
                    if (indice > DIMENSIONE_MASCHERA - 1) { stop = true; break; }
                    int residuo = lookup[ciclo_buf.numero[i]];
                    long long bit_idx = (indice << 4) + residuo;
                    maschera_0[bit_idx >> 6] |= (1ULL << (bit_idx & 63));
                }
                if (stop) break;
            }
        }

        lista_primi_bootstrap.clear();
        lista_primi_bootstrap.push_back(7);

        for (long long bit_idx = 0; bit_idx < total_bits; bit_idx++) {
            if ((maschera_0[bit_idx >> 6] & (1ULL << (bit_idx & 63))) == 0) {
                long long p = (bit_idx / 16) * 60 + 10 + SOTTOLISTA_BASE[bit_idx % 16];
                if (p > (long long)radice_max) break;
                if (p > 7) lista_primi_bootstrap.push_back(p);

                // Utile se colpisce A o B (o entrambe)
                if (e_divisore_utile(p, target_A, target_A + (i128)W) ||
                    e_divisore_utile(p, target_B, fine_B))
                    divisori_fase1.push_back(p);
            }
        }
    }

    std::cout << "Avvio scrematura globale in parallelo...\n";

    // ============================================================
    // FASE 2: Cicli paralleli
    // ============================================================
    #pragma omp parallel for schedule(dynamic, 4)
    for (long long cicli = 1; cicli < quanti_cicli; cicli++) {
        std::vector<uint64_t> maschera_bit(array_size, 0);
        CicloRisultato ciclo_buf;

        long long riferimento = DIMENSIONE_MASCHERA * cicli;
        long long radice_c = (long long)std::sqrt(
            (double)(DIMENSIONE_MASCHERA * (cicli + 1) * 60)) + 1;

        for (long long p : lista_primi_bootstrap) {
            if (p > radice_c) break;
            ricerca_ciclo(p, riferimento, ciclo_buf);
            if (ciclo_buf.len == 0) continue;
            for (long long ii = 0; ; ii++) {
                bool stop = false;
                for (int i = 0; i < ciclo_buf.len; i++) {
                    long long indice = ciclo_buf.lista[i] + p * ii;
                    if (indice > DIMENSIONE_MASCHERA - 1) { stop = true; break; }
                    int residuo = lookup[ciclo_buf.numero[i]];
                    long long bit_idx = (indice << 4) + residuo;
                    maschera_bit[bit_idx >> 6] |= (1ULL << (bit_idx & 63));
                }
                if (stop) break;
            }
        }

        for (long long bit_idx = 0; bit_idx < total_bits; bit_idx++) {
            if ((maschera_bit[bit_idx >> 6] & (1ULL << (bit_idx & 63))) == 0) {
                long long p = (bit_idx / 16) * 60 + 10
                            + (long long)(DIMENSIONE_MASCHERA * cicli) * 60
                            + SOTTOLISTA_BASE[bit_idx % 16];

                if ((long long)p > (long long)radice_max) break;

                if (e_divisore_utile(p, target_A, target_A + (i128)W) ||
                    e_divisore_utile(p, target_B, fine_B))
                    divisori_per_ciclo[cicli - 1].push_back(p);
            }
        }
    }

    // Unione divisori
    std::vector<long long> archivio = divisori_fase1;
    for (auto& v : divisori_per_ciclo)
        archivio.insert(archivio.end(), v.begin(), v.end());
    std::sort(archivio.begin(), archivio.end());
    archivio.erase(std::unique(archivio.begin(), archivio.end()), archivio.end());

    auto t1 = std::chrono::high_resolution_clock::now();
    std::cout << "Divisori raccolti: " << fmt((long long)archivio.size())
              << "  (" << std::chrono::duration<double>(t1-t0).count() << " s)\n\n";

        // ============================================================
        // FASE 3: Finestra passiva su A e B — scrittura CSV
        // Un file per esperimento: suffix_data_1.csv, suffix_data_2.csv ...
        // ============================================================
        std::string csv_name = OUTPUT_DIR + "suffix_data_" + std::to_string(n_exp) + ".csv";
        std::ofstream csv(csv_name);
        if (!csv) {
            std::cerr << "Errore apertura " << csv_name << "\n";
            continue;
        }
        // Header con metadati commentati — Python li ignora
        csv << "# esperimento=" << n_exp
            << "  A=" << i128_to_str(target_A)
            << "  B=" << i128_to_str(target_B)
            << "  W=" << W << "\n";
        csv << "prefix,suffix\n";

        std::cout << "--- Finestra A  [" << fmt128(target_A) << ", +" << W << ") ---\n";
        long long nA = applica_finestra_e_scrivi(archivio, target_A, W,
                           target_A, csv);
        std::cout << "Primi trovati: " << nA << "\n\n";

        std::cout << "--- Finestra B  [" << fmt128(target_B) << ", +" << W << ") ---\n";
        long long nB = applica_finestra_e_scrivi(archivio, target_B, W,
                           target_B, csv);
        std::cout << "Primi trovati: " << nB << "\n\n";

        csv.close();

        auto t2 = std::chrono::high_resolution_clock::now();
        std::cout << "Output: " << csv_name << "\n";
        std::cout << "Righe totali (escluso header): " << (nA + nB) << "\n";
        std::cout << "Tempo esperimento: "
                  << std::chrono::duration<double>(t2-t0).count() << " s\n";
        std::cout << "================================================\n\n";

    } // fine loop esperimenti

    std::cout << "\nTutti gli esperimenti completati.\n";
    std::cout << "I file CSV sono nella stessa cartella dell'exe.\n";
    std::cout << "\nPremi INVIO per chiudere...\n";
    std::cin.get();
    return 0;
}
