# Mejoras Pendientes de Hy3

Este documento recoge las posibles mejoras y trabajos futuros del motor de ajedrez **Hy3** que, en su momento, se dejaron planificados para versiones posteriores y no se incluyeron en la versión 1.6. Se extraen del documento técnico (`docs/documento_tecnico.md`) para reflejar el estado real y actual del desarrollo, separando lo ya realizado de lo pendiente.

---

## 1. Migración a Bitboards / Bitboards Mágicos

- **Origen**: propuesta del documento `docs/mejoras_y_optimizaciones.md` (v1.5), descartada para esa versión y pospuesta.
- **Descripción**: cambiar la representación del tablero (actualmente *mailbox* 8x8 en `squares[64]`) por *bitboards*, preferiblemente con generación mágica de movimientos deslizantes.
- **Alcance**: implica reescribir por completo la representación del tablero, el generador de movimientos, `is_square_attacked` y SEE.
- **Prioridad**: **alta / próxima mejora**. Se considera fundamental por la ganancia de velocidad que aporta (elo "gratis") y por la infraestructura que proporciona para sustentar una evaluación más avanzada en el futuro.
- **Cómo abordarlo**: la migración debe realizarse de manera **independiente**, aislada de cualquier otro cambio, para que no interfiera con el resto del desarrollo y su impacto pueda validarse por separado.
- **Riesgo / consideración**: invalidaría todas las garantías de corrección verificadas por *perft* y exigiría una re-verificación integral.

---

## 2. Evaluación NNUE (red neuronal)

- **Origen**: propuesta descartada en v1.5; sugerida también como objetivo a largo plazo en `docs/Info_humano.md` ("Algún día implementarás NNUE, pero mientras tanto, haz la mejor HCE que puedas").
- **Descripción**: sustituir o complementar la evaluación artesanal (HCE) por una red neuronal eficiente (*Efficiently Updatable Neural Network*).
- **Alcance**: paradigma de evaluación distinto que requiere una red entrenada, un conjunto de datos de entrenamiento (millones de posiciones etiquetadas) y una infraestructura de acumuladores incrementales.
- **Prioridad / momento**: **proyecto de futuro**. El objetivo es abordarlo cuando el motor alcance un nivel de **~3000 Elo CCRL con HCE**. Hasta entonces, la HCE es el camino.
- **Riesgo / consideración**: representa meses de trabajo y una dependencia de datos externa.

---

## 3. Extensiones Singulares (*Singular Extensions*) y mejoras de búsqueda

- **Origen**: propuesta de podas descartada (parcial) en v1.5 a favor de RFP, LMP y *futility**.
- **Descripción**: buscar una jugada singular (claramente mejor que el resto) a mayor profundidad mediante búsquedas de verificación con ventana excluida. Incluye, en general, cualquier otra mejora de búsqueda.
- **Alcance**: alta complejidad y riesgo de inestabilidad para una ganancia marginal en un motor de este nivel.
- **Prioridad**: **alta**. La búsqueda es donde más margen hay para ganar elo, por lo que las extensiones singulares y cualquier otra mejora de búsqueda son prioritarias.
- **Cómo abordarlo**: cada mejora debe **implementarse y probarse por separado** para validar su efecto de forma aislada antes de combinarla con otras.
- **Riesgo / consideración**: en v1.5 se priorizaron podas de mayor relación beneficio/riesgo; estas mejoras quedan pendientes de revaluar cuando su coste/beneficio lo justifique.

---

## Notas

Las tres mejoras anteriores aparecían en el documento técnico como candidatas postergadas (bitboards y NNUE citadas explícitamente como candidatas a "versión 2.0" o "algún día", y extensiones singulares como descarte parcial). Se mantienen aquí como hoja de ruta de posibles trabajos futuros, fuera ya del documento técnico que describe el estado actual (v1.6).
