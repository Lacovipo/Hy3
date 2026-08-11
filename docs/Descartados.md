# Propuestas Descartadas de Hy3

Este documento recoge las propuestas que, a lo largo del desarrollo de Hy3, se evaluaron y **descartaron** (o se refutaron empíricamente). Se extraen del documento técnico (`docs/documento_tecnico.md`) —secciones 5.2 y 6.2— para dejar dicho documento centrado en el estado real y actual del desarrollo (v1.6).

---

## 1. Descartadas en la versión 1.5

Cada propuesta se contrastó con el código real antes de decidir su implementación (fuente: `docs/mejoras_y_optimizaciones.md`).

| Propuesta | Decisión | Razón del descarte |
| :--- | :--- | :--- |
| **Migración a Bitboards / Bitboards Mágicos** | Descartada | Implica reescribir por completo la representación del tablero, el generador de movimientos, `is_square_attacked` y SEE. Invalidaría todas las garantías de corrección verificadas por perft y exigiría una re-verificación integral. Es una reingeniería mayor, desproporcionada para un incremento de versión puntual (1.4 → 1.5). Queda como candidata para una versión 2.0. |
| **Evaluación NNUE** (red neuronal) | Descartada | Es un paradigma de evaluación distinto que requiere una red entrenada, un conjunto de datos de entrenamiento (millones de posiciones etiquetadas) y una infraestructura de acumuladores incrementales. Representa meses de trabajo y una dependencia de datos externa, incompatible con el alcance de un release incremental. |
| **Extensiones Singulares** (*Singular Extensions*) | Descartada (parcial de la propuesta de podas) | Alta complejidad y riesgo de inestabilidad (requiere búsquedas de verificación con ventana excluida) para una ganancia marginal en un motor de este nivel. Se implementaron en su lugar las podas de mayor relación beneficio/riesgo (RFP, LMP, futility). |
| **`is_legal` sin clonar el tablero** | Ya resuelta en v1.4 | La propuesta era correcta pero ya estaba aplicada: desde v1.4 `is_legal` hace make/unmake *in-place*. No requería acción. |

---

## 2. Descartadas (refutadas) en la versión 1.6

Todas las propuestas de las revisiones externas se contrastaron empíricamente contra la versión 1.5.

| Revisión | Hallazgo planteado | Veredicto | Evidencia |
| :--- | :--- | :--- | :--- |
| `Rev_Ling3F.md` (Bug 1) | `is_square_attacked` no detecta deslizantes | **Refutado** | Batería: Dd4 ataca por fila, columna y diagonal; falla en f5. Perft de 6 posiciones exacto. |
| `Rev_Ling3F.md` (Bug 2) | Las negras no generan capturas; perft inflado | **Refutado** | Perft Kiwipete d4 = 4 085 603 coincidente; `generate_captures` simétrico. |
| `Rev_DS4F.md` (§1.2) | Bono de peón pasado negro invertido | **Refutado** | Posiciones espejo: a6 blancas y a3 negras devuelven **193 cp** idénticos. 3 revisiones más coinciden. |
| `Rev_Nemo3U.md` | Avance doble de peón no verifica la casilla intermedia | **Refutado** | `en_passant_candidate` solo se marca si la intermedia está libre; Kiwipete d4 exacto. |
| `Rev_Nemo3U.md` | El rey se mueve junto a su rival | **Refutado** | `is_legal`/`legal_moves` usan `in_check`; Kiwipete d4 = 4 085 603 exacto. |
| `Rev_MiMo25.md` | La máscara de `move_flag` debe ser `&3` | **Refutado** | El código usa `&7`, correcto. El error del revisor es de bit: confundió el bit de promoción (2) con el de bandera (3), y además `&3` descartaría la promoción. |
| `Rev_Opus5.md` | Solicita `go depth` (ya existía) y `go infinite` (capado por el bug #1) | **Aplicado** (vía #1) | No era una carencia, sino consecuencia del bug crítico de `go`. |
| `Rev_DS4P.md` | Promoción usa índice `"nbrq"[promo-1]` | **Falso positivo** | `promo` ya es 1‑based (ver v1.5, corrección #7); el índice es correcto. |
| `Rev_DS4P.md` | Rey captura casilla defendida | **Ya corregido** | La función `see` base ya invoca `is_legal`. |

*(Las revisiones también contenían propuestas mayores —bitboards mágicos, NNUE, extensiones singulares— descartadas por las mismas razones de alcance que en v1.5; ver sección 1 de este documento.)*
