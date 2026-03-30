#!/usr/bin/env python3
#!/usr/bin/env python3
"""
analisi_suffissi.py
Legge automaticamente tutti i file suffix_data_*.csv da una cartella
e genera un grafico PNG per ciascuno.

Uso:
    python analisi_suffissi.py                    # legge da C:/dati_suffix/
    python analisi_suffissi.py C:/altra/cartella  # cartella personalizzata
    python analisi_suffissi.py file.csv           # file singolo

Dipendenze:
    pip install pandas matplotlib
"""

import sys, os, glob
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from collections import Counter
import numpy as np

CARTELLA_DEFAULT = r"C:\dati_suffix"

COLOR_A = "#1D9E75"
COLOR_B = "#378ADD"
COLOR_C = "#D85A30"
COLOR_G = "#7F77DD"
plt.rcParams.update({
    "figure.facecolor": "white", "axes.facecolor": "white",
    "axes.grid": True, "grid.alpha": 0.25,
    "axes.spines.top": False, "axes.spines.right": False,
    "font.size": 11,
})

def trova_csv(argomento=None):
    if argomento is None:
        cartella = CARTELLA_DEFAULT
    elif os.path.isfile(argomento):
        base = os.path.splitext(argomento)[0]
        return [(argomento, base + ".png")]
    else:
        cartella = argomento
    pattern = os.path.join(cartella, "suffix_data_*.csv")
    files = sorted(glob.glob(pattern))
    if not files:
        print(f"Nessun file suffix_data_*.csv trovato in: {cartella}")
        sys.exit(1)
    return [(f, os.path.splitext(f)[0] + ".png") for f in files]

def abbr(n, max_len=22):
    s = str(n)
    return s if len(s) <= max_len else s[:10] + "..." + s[-8:]

def analizza(csv_path, png_path):
    print(f"\n{'='*60}\nFile: {csv_path}")
    df = pd.read_csv(csv_path, comment='#', dtype={"prefix": str}, low_memory=False)
    prefissi = sorted(df["prefix"].astype(str).unique())
    if len(prefissi) != 2:
        print(f"  Attenzione: trovati {len(prefissi)} prefissi, saltato.")
        return
    pA, pB = prefissi[0], prefissi[1]
    sA = df[df["prefix"] == pA]["suffix"].tolist()
    sB = df[df["prefix"] == pB]["suffix"].tolist()
    W  = int(df["suffix"].max()) + 1
    set_A  = set(sA)
    set_B  = set(sB)
    comuni = sorted(set_A & set_B)
    n_common  = min(len(sA), len(sB))
    gaps      = [sB[i] - sA[i] for i in range(n_common)]
    gap_medio = sum(gaps) / len(gaps)
    print(f"  A={pA}  ({len(sA)} primi)")
    print(f"  B={pB}  ({len(sB)} primi)")
    print(f"  W={W}  comuni={len(comuni)}  gap medio={gap_medio:.1f}")

    t1, t2  = W // 3, 2 * W // 3
    limiti  = [(0, t1), (t1, t2), (t2, W)]
    conta_A = [sum(1 for s in sA if lo <= s < hi) for lo, hi in limiti]
    conta_B = [sum(1 for s in sB if lo <= s < hi) for lo, hi in limiti]
    pct_A   = [c / len(sA) * 100 for c in conta_A]
    pct_B   = [c / len(sB) * 100 for c in conta_B]
    zone_labels = [f"0-{t1:,}\n(primo terzo)",
                   f"{t1:,}-{t2:,}\n(secondo terzo)",
                   f"{t2:,}-{W:,}\n(terzo finale)"]

    fig = plt.figure(figsize=(16, 13))
    fig.suptitle(f"prefix {abbr(pA)} vs {abbr(pB)}  (W={W})",
                 fontsize=13, fontweight="bold", y=0.99)
    gs = gridspec.GridSpec(4, 2, figure=fig, hspace=0.50, wspace=0.32)

    # 1. Timeline
    ax1 = fig.add_subplot(gs[0, :])
    ax1.set_title("Timeline suffissi  (verde=A, blu=B, arancio=comuni)")
    ax1.set_ylim(-0.6, 1.6); ax1.set_xlim(0, W)
    ax1.set_yticks([0, 1])
    ax1.set_yticklabels([abbr(pA, 18), abbr(pB, 18)], fontsize=8)
    ax1.set_xlabel("suffisso (0 ... W-1)")
    for s in sA:
        col = COLOR_C if s in set_B else COLOR_A
        ax1.axvline(s, ymin=0.55, ymax=0.95, color=col,
                    linewidth=1.2 if col==COLOR_C else 0.7, alpha=0.9)
    for s in sB:
        col = COLOR_C if s in set_A else COLOR_B
        ax1.axvline(s, ymin=0.05, ymax=0.45, color=col,
                    linewidth=1.2 if col==COLOR_C else 0.7, alpha=0.9)
    ax1.legend(handles=[
        plt.Line2D([0],[0],color=COLOR_A,lw=1.5,label=f"solo A ({len(sA)-len(comuni)})"),
        plt.Line2D([0],[0],color=COLOR_B,lw=1.5,label=f"solo B ({len(sB)-len(comuni)})"),
        plt.Line2D([0],[0],color=COLOR_C,lw=2,  label=f"comuni ({len(comuni)})"),
    ], loc="upper right", fontsize=9)

    # 2. Densita per terzi
    ax2 = fig.add_subplot(gs[1, :])
    ax2.set_title("Densita primi per zona  (% sul totale di ciascun prefix)")
    xb, wb = np.arange(3), 0.32
    bars_A = ax2.bar(xb-wb/2, pct_A, width=wb, color=COLOR_A, alpha=0.85, label="A")
    bars_B = ax2.bar(xb+wb/2, pct_B, width=wb, color=COLOR_B, alpha=0.85, label="B")
    for bar, pct, cnt in zip(bars_A, pct_A, conta_A):
        ax2.text(bar.get_x()+bar.get_width()/2, bar.get_height()+0.2,
                 f"{pct:.1f}%\n({cnt})", ha="center", va="bottom",
                 fontsize=9, color=COLOR_A, fontweight="bold")
    for bar, pct, cnt in zip(bars_B, pct_B, conta_B):
        ax2.text(bar.get_x()+bar.get_width()/2, bar.get_height()+0.2,
                 f"{pct:.1f}%\n({cnt})", ha="center", va="bottom",
                 fontsize=9, color=COLOR_B, fontweight="bold")
    ax2.axhline(100/3, color="gray", lw=0.8, ls="--", label="uniforme 33.3%")
    for xi in [0.5, 1.5]: ax2.axvline(xi, color="lightgray", lw=1.0, ls=":")
    ax2.set_xticks(xb); ax2.set_xticklabels(zone_labels, fontsize=10)
    ax2.set_ylabel("% primi trovati")
    ax2.set_ylim(0, max(max(pct_A), max(pct_B)) * 1.25)
    ax2.legend(fontsize=9)

    # 3. Gap per posizione i
    ax3 = fig.add_subplot(gs[2, 0])
    ax3.set_title(f"Gap = s_B[i] - s_A[i]  (media {gap_medio:.1f})")
    ax3.set_xlabel("i (posizione ordinale)"); ax3.set_ylabel("gap")
    ax3.bar(range(len(gaps)), gaps,
            color=[COLOR_B if g>=0 else COLOR_G for g in gaps],
            width=1.0, alpha=0.8)
    ax3.axhline(gap_medio, color="black", lw=0.8, ls="--")
    ax3.axhline(0, color="gray", lw=0.5)

    # 4. Distribuzione gap
    ax4 = fig.add_subplot(gs[2, 1])
    ax4.set_title("Distribuzione dei gap")
    ax4.set_xlabel("valore del gap"); ax4.set_ylabel("frequenza")
    gc = Counter(gaps)
    vals = sorted(gc.keys())
    ax4.bar(vals, [gc[v] for v in vals],
            color=[COLOR_B if v>=0 else COLOR_G for v in vals],
            width=max(1,(max(vals)-min(vals))//50), alpha=0.8)

    # 5. Ultima cifra
    ax5 = fig.add_subplot(gs[3, 0])
    ax5.set_title("Ultima cifra suffissi  (attesa: 25% ciascuna)")
    ax5.set_xlabel("ultima cifra"); ax5.set_ylabel("% dei primi")
    digits = [1, 3, 7, 9]
    for serie, col, lbl in [(sA,COLOR_A,"A"),(sB,COLOR_B,"B")]:
        tot  = len(serie)
        pcts = [sum(1 for s in serie if s%10==d)/tot*100 for d in digits]
        off  = -0.2 if col==COLOR_A else 0.2
        ax5.bar(np.arange(len(digits))+off, pcts,
                width=0.35, color=col, alpha=0.8, label=lbl)
    ax5.axhline(25, color="gray", lw=0.8, ls="--", label="teorico 25%")
    ax5.set_xticks(range(len(digits))); ax5.set_xticklabels([str(d) for d in digits])
    ax5.legend(fontsize=9)

    # 6. Gap mod q
    ax6 = fig.add_subplot(gs[3, 1])
    ax6.set_title(f"Gap mod piccoli q  (firma W={W})")
    ax6.set_xlabel("q"); ax6.set_ylabel("residuo dominante (% casi)")
    small_primes = [3, 7, 11, 13, 17, 19, 23]
    dom_pct, dom_val = [], []
    for q in small_primes:
        cnt = Counter(g % q for g in gaps)
        best_r, best_n = cnt.most_common(1)[0]
        dom_val.append(best_r); dom_pct.append(best_n/len(gaps)*100)
    ax6.bar([str(q) for q in small_primes], dom_pct, color=COLOR_G, alpha=0.8)
    ax6.axhline(100/7, color="gray", ls="--", lw=0.8, label="uniforme")
    for i, (r, p) in enumerate(zip(dom_val, dom_pct)):
        ax6.text(i, p+0.5, f"={r}", ha="center", fontsize=8)
    ax6.legend(fontsize=9)

    plt.savefig(png_path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  Grafico salvato: {png_path}")

# ── Main ──────────────────────────────────────────────────────
try:
    argomento  = sys.argv[1] if len(sys.argv) > 1 else None
    lista_file = trova_csv(argomento)
    print(f"Trovati {len(lista_file)} file da processare.")
    for i, (csv_path, png_path) in enumerate(lista_file, 1):
        print(f"\n[{i}/{len(lista_file)}]", end="")
        analizza(csv_path, png_path)
    print(f"\n{'='*60}")
    print(f"Completato. {len(lista_file)} grafici PNG salvati.")
    # Successo: esce da solo senza attendere input
except Exception as e:
    print(f"\n{'='*60}")
    print(f"ERRORE: {e}")
    import traceback
    traceback.print_exc()
    input("\nPremi INVIO per chiudere...")