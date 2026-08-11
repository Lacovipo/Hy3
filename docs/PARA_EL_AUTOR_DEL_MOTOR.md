# Informe para el autor de Hy3 1.6

## Que es esto y como usarlo

Este documento lo ha generado **Camifurlo**, un analizador de solidez funcional de motores de ajedrez: somete al motor a una bateria de pruebas de protocolo, tiempo, ponder, ciclo de vida y partidas reales, y anota todo lo que se aparta de la especificacion o resulta fragil.

A diferencia del informe principal (que explica como debe *sortear* estos problemas una GUI), este va dirigido a **arreglar el motor en su codigo fuente**.

> **Si eres un modelo de IA al que le han pasado este documento junto al codigo fuente del motor**: cada apartado trae el sintoma observado, la secuencia exacta que lo reproduce, la evidencia del log, la causa que suele haber detras y el cambio sugerido. Empieza por los de prioridad mas alta. Antes de tocar nada, localiza en el fuente el area indicada y confirma que la causa descrita se corresponde con lo que hace ese codigo: la causa es la explicacion **tipica**, no una certeza sobre este motor en concreto. Al terminar, la forma de verificar es volver a pasar Camifurlo y comprobar que el hallazgo ha desaparecido.

## Motor analizado

| Campo | Valor |
|---|---|
| Nombre declarado | Hy3 1.6 |
| Autor declarado | Hy3 |
| Protocolo analizado | uci |
| Ejecutable | Hy3 1.6.exe |
| MD5 | cb7ab628c5ffcbcc05a974aa6b03939c |
| Fecha del analisis | 2026-08-09 19:22:13 |
| Duracion de la bateria | 64.4 minutos |
| Partidas jugadas | 77 |
| Pruebas superadas | 100 |

## Resumen de lo que hay que arreglar

| # | Prioridad | Esfuerzo | Area del codigo | Problema |
|---|---|---|---|---|
| 1 | 🟠 ALTA | trivial | modo ponder | El motor cuenta el tiempo de ponder como tiempo propio |
| 2 | 🟠 ALTA | medio | gestion del tiempo de busqueda | El motor sobrepasa gravemente el tiempo asignado |
| 3 | 🟠 ALTA | medio | bucle principal de entrada/salida (lectura de stdin) | El motor emite 'bestmove' sin que se le haya pedido |
| 4 | 🟠 ALTA | medio | gestion del tiempo de busqueda | El motor pierde por tiempo |
| 5 | 🟠 ALTA | medio | terminacion del proceso | Quedan procesos hijos vivos despues de cerrar el motor |
| 6 | 🔵 BAJA | trivial | bucle principal de entrada/salida (lectura de stdin) | No procesa el resto de la linea tras un token desconocido |
| 7 | 🔵 BAJA | trivial | gestion del tiempo de busqueda | 'go' sin parametros termina por su cuenta |
| 8 | 🔵 BAJA | medio | parseo de posiciones (FEN y lista de jugadas) | El motor acepta un FEN sintacticamente invalido |

**Por donde empezar**: 3 de estos 8 arreglos son de esfuerzo trivial, de unas pocas lineas — El motor cuenta el tiempo de ponder como tiempo propio; No procesa el resto de la linea tras un token desconocido; 'go' sin parametros termina por su cuenta.

## Detalle

### 1. El motor cuenta el tiempo de ponder como tiempo propio

| Campo | Valor |
|---|---|
| Prioridad | 🟠 ALTA |
| Esfuerzo estimado | trivial |
| Donde mirar | modo ponder |
| Veces observado | 1 |
| Codigo del comportamiento | `ponder_gasta_reloj` |
| Pruebas que lo detectaron | `D09_reloj_ponder` |

**Sintoma y por que importa**: Tras un ponderhit, consume el tiempo que ya habia gastado ponderando y llega tarde. ES LA CAUSA CLASICA de que un motor pierda por tiempo SOLO cuando se activa el ponder.

**Comportamiento correcto**: El reloj empieza a correr en el 'ponderhit'.

**Lo que hizo este motor**:

```
con el mismo reloj (6 s) una busqueda normal tarda 188 ms, pero tras ponderhit con 2,5 s de ponder devuelve en 0 ms
```

**Causa habitual**: El cronometro se arranca al empezar a ponderar, no al recibir 'ponderhit', asi que el tiempo pensado gratis se descuenta del propio reloj.

**Cambio sugerido**: Reiniciar la marca de tiempo en el 'ponderhit'. El tiempo de ponder es gratis por definicion: es tiempo del rival.

<sub>Mientras no se arregle, la GUI tiene que: Desactivar el ponder para este motor, o comunicarle un reloj reducido tras un ponderhit.</sub>

### 2. El motor sobrepasa gravemente el tiempo asignado

| Campo | Valor |
|---|---|
| Prioridad | 🟠 ALTA |
| Esfuerzo estimado | medio |
| Donde mirar | gestion del tiempo de busqueda |
| Veces observado | 1 |
| Codigo del comportamiento | `sobrepaso_tiempo_grave` |
| Pruebas que lo detectaron | `C_bala_1s` |

**Sintoma y por que importa**: Sobrepasos superiores al 50%: con este control de tiempo pierde partidas por bandera de forma sistematica.

**Comportamiento correcto**: Consumo <= tiempo asignado.

**Lo que hizo este motor**:

```
con 1s a caer bandera llego a gastar 1.88x el tiempo razonable para la jugada
```

**Causa habitual**: No hay tope duro: solo se comprueba el tiempo entre iteraciones, asi que una iteracion larga se lleva por delante todo el reloj.

**Cambio sugerido**: Dos limites: uno 'blando' para no empezar otra iteracion y otro 'duro' que aborta la busqueda en curso desde dentro (comprobando cada pocos miles de nodos) y devuelve la mejor jugada que se tenga. Nunca gastar mas del tiempo restante menos el margen.

<sub>Mientras no se arregle, la GUI tiene que: Evitar este control de tiempo con este motor. Si es imprescindible, activar 'ignorar bandera' para el (Coliseo ya lo contempla) y documentar que sus resultados no son homologables.</sub>

### 3. El motor emite 'bestmove' sin que se le haya pedido

| Campo | Valor |
|---|---|
| Prioridad | 🟠 ALTA |
| Esfuerzo estimado | medio |
| Donde mirar | bucle principal de entrada/salida (lectura de stdin) |
| Veces observado | 1 |
| Codigo del comportamiento | `bestmove_fantasma` |
| Pruebas que lo detectaron | `F03_doble_go` |

**Sintoma y por que importa**: Rompe la sincronia: la GUI tomara ese bestmove como respuesta a la siguiente jugada y a partir de ahi todo va desfasado.

**Comportamiento correcto**: Un 'bestmove' por cada 'go'.

**Lo que hizo este motor**:

```
con dos 'go' seguidos sin esperar el bestmove llegan 2 lineas bestmove: ['bestmove b1c3', 'bestmove g1f3 ponder g8f6']
```

**Secuencia que lo reproduce** (enviada al motor por su entrada estandar):

```
position startpos
go movetime 400
go movetime 400
```

**Causa habitual**: Se emite mas de un 'bestmove' por cada 'go' (por ejemplo, uno al abortar y otro al terminar), o se atiende un 'go' nuevo sin haber cerrado el anterior.

**Cambio sugerido**: Invariante: exactamente una linea 'bestmove' por cada 'go'. Serializar las peticiones e ignorar un 'go' que llegue con una busqueda en curso.

<sub>Mientras no se arregle, la GUI tiene que: Emparejar peticiones y respuestas con un contador; descartar los bestmove inesperados y registrarlos.</sub>

<details><summary>Extracto del log de comunicacion</summary>

```
[18:24:27.678] [+    297.0ms] >> ucinewgame
[18:24:27.678] [+    297.0ms] >> isready
[18:24:27.678] [+    297.0ms] << readyok   <-- CRLF
[18:24:27.678] [+    297.0ms] >> position startpos
[18:24:27.678] [+    297.0ms] >> go movetime 400
[18:24:27.678] [+    297.0ms] >> go movetime 400
[18:24:27.690] [+    313.0ms] << bestmove b1c3   <-- CRLF
[18:24:28.049] [+    672.0ms] << bestmove g1f3 ponder g8f6   <-- CRLF
```

</details>

### 4. El motor pierde por tiempo

| Campo | Valor |
|---|---|
| Prioridad | 🟠 ALTA |
| Esfuerzo estimado | medio |
| Donde mirar | gestion del tiempo de busqueda |
| Veces observado | 4 |
| Codigo del comportamiento | `bandera` |
| Pruebas que lo detectaron | `G_autoplay` |

**Sintoma y por que importa**: Agota el reloj antes de devolver la jugada.

**Comportamiento correcto**: Terminar siempre antes de que caiga la bandera.

**Lo que hizo este motor**:

```
6 perdidas por tiempo en 6 partidas con 1s a caer bandera ponder=no
6 perdidas por tiempo en 6 partidas con 1s a caer bandera ponder=si
6 perdidas por tiempo en 6 partidas con 2s + 0.1s ponder=no
5 perdidas por tiempo en 8 partidas con 2s + 0.1s ponder=si
```

**Causa habitual**: El reparto del tiempo no tiene en cuenta el reloj real que envia la GUI, o no reserva nada para las jugadas siguientes.

**Cambio sugerido**: Recalcular el presupuesto en cada jugada a partir de 'wtime'/'btime' del comando 'go' (no de un valor propio), y no gastar nunca mas de un tercio del tiempo restante en una sola jugada salvo emergencia.

<sub>Mientras no se arregle, la GUI tiene que: Usar controles de tiempo con incremento, subir el tiempo base o activar 'ignorar bandera'. Comprobar antes si el problema es el ponder (ver la seccion de ponder del informe).</sub>

### 5. Quedan procesos hijos vivos despues de cerrar el motor

| Campo | Valor |
|---|---|
| Prioridad | 🟠 ALTA |
| Esfuerzo estimado | medio |
| Donde mirar | terminacion del proceso |
| Veces observado | 1 |
| Codigo del comportamiento | `huerfanos` |
| Pruebas que lo detectaron | `G_autoplay` |

**Sintoma y por que importa**: El motor lanza subprocesos (envoltorios .bat, JVM, motores NNUE auxiliares) que sobreviven al padre.

**Comportamiento correcto**: Al cerrar el motor no queda ningun descendiente vivo.

**Lo que hizo este motor**:

```
procesos supervivientes: [(87968, 'ProcessGovernor.exe')]
```

**Causa habitual**: El motor lanza procesos hijos (envoltorios, ayudantes) que no mata al salir.

**Cambio sugerido**: Terminar explicitamente los hijos antes de salir, o lanzarlos de forma que mueran con el padre (job object en Windows).

<sub>Mientras no se arregle, la GUI tiene que: Fotografiar los descendientes ANTES de cerrar (Toolhelp32 / taskkill /T) y matarlos explicitamente despues. Con `cmd.exe /c motor.bat`, matar solo el cmd.exe deja el motor huerfano.</sub>

### 6. No procesa el resto de la linea tras un token desconocido

| Campo | Valor |
|---|---|
| Prioridad | 🔵 BAJA |
| Esfuerzo estimado | trivial |
| Donde mirar | bucle principal de entrada/salida (lectura de stdin) |
| Veces observado | 1 |
| Codigo del comportamiento | `no_procesa_tras_token_desconocido` |
| Pruebas que lo detectaron | `A06_token_desconocido` |

**Sintoma y por que importa**: La especificacion UCI pide ignorar los tokens iniciales que no se entiendan y procesar el resto ('joho debug on' debe activar el debug). Casi ninguna GUI depende de esto.

**Comportamiento correcto**: readyok (ignorando el token 'joho')

**Lo que hizo este motor**:

```
tras 'joho isready' no llego readyok
```

**Causa habitual**: Se mira solo el primer token y si no se reconoce se descarta la linea entera.

**Cambio sugerido**: La especificacion pide ir descartando tokens iniciales desconocidos hasta encontrar uno valido ('joho debug on' debe activar el debug). Basta con un bucle sobre los tokens antes de despachar.

<sub>Mientras no se arregle, la GUI tiene que: Enviar siempre comandos limpios, sin prefijos ni extensiones propias.</sub>

<details><summary>Extracto del log de comunicacion</summary>

```
[18:17:48.820] [+   2110.0ms] >> joho isready
```

</details>

### 7. 'go' sin parametros termina por su cuenta

| Campo | Valor |
|---|---|
| Prioridad | 🔵 BAJA |
| Esfuerzo estimado | trivial |
| Donde mirar | gestion del tiempo de busqueda |
| Veces observado | 1 |
| Codigo del comportamiento | `go_pelado_termina_solo` |
| Pruebas que lo detectaron | `C_go_pelado` |

**Sintoma y por que importa**: La especificacion dice que equivale a busqueda infinita. Este motor devuelve jugada solo, lo que en la practica es mas comodo.

**Comportamiento correcto**: Buscar hasta recibir 'stop'.

**Lo que hizo este motor**:

```
'go' sin parametros devolvio bestmove solo, en 687 ms
```

**Causa habitual**: 'go' sin parametros deberia buscar indefinidamente hasta 'stop'.

**Cambio sugerido**: Detalle menor de conformidad: tratar 'go' sin limites como 'go infinite'.

<sub>Mientras no se arregle, la GUI tiene que: Sin impacto: la GUI no deberia enviar 'go' pelado nunca.</sub>

### 8. El motor acepta un FEN sintacticamente invalido

| Campo | Valor |
|---|---|
| Prioridad | 🔵 BAJA |
| Esfuerzo estimado | medio |
| Donde mirar | parseo de posiciones (FEN y lista de jugadas) |
| Veces observado | 6 |
| Codigo del comportamiento | `acepta_fen_malformado` |
| Pruebas que lo detectaron | `F02_fen_malformado` |

**Sintoma y por que importa**: Sigue jugando desde un tablero indeterminado en vez de quejarse.

**Comportamiento correcto**: Rechazar el FEN invalido.

**Lo que hizo este motor**:

```
con el FEN invalido 'campos_de_menos' devuelve c2c4 en vez de quejarse
con el FEN invalido 'fila_incompleta' devuelve h2h3 en vez de quejarse
con el FEN invalido 'pieza_invalida' devuelve g1f3 en vez de quejarse
con el FEN invalido 'sin_reyes' devuelve 0000 en vez de quejarse
con el FEN invalido 'turno_invalido' devuelve g1f3 en vez de quejarse
con el FEN invalido 'casilla_ep_absurda' devuelve g1f3 en vez de quejarse
```

**Causa habitual**: El parseo del FEN no comprueba el numero de campos ni la coherencia de las filas.

**Cambio sugerido**: Validar: ocho filas, suma de casillas igual a ocho por fila, un rey de cada color, turno valido, casilla al paso coherente. Ante un FEN invalido, conservar la posicion anterior y avisar con 'info string'.

<sub>Mientras no se arregle, la GUI tiene que: Validar el FEN en la GUI antes de enviarlo.</sub>

## Como verificar los arreglos

```bash
python camifurlo.py --motor "<ruta del motor recompilado>" --protocolo u
```

Cada problema de este documento lleva el codigo del comportamiento y las pruebas que lo detectaron. Si el arreglo es correcto, esas pruebas pasan a la lista de superadas del informe nuevo y el hallazgo desaparece.

Para centrarse solo en una parte:

```bash
python camifurlo.py --motor "<ruta>" --protocolo u --fases D    # solo ponder
python camifurlo.py --motor "<ruta>" --protocolo u --fases CG   # tiempo y partidas
```

_Generado por Camifurlo v1.0.0 el 2026-08-09._

---

## Estado resuelto en Hy3 1.7

Los 8 hallazgos de este informe fueron corregidos en el código fuente y verificados
con `tests/verify_v17.py` (10/10). Resumen:

| # | Problema | Estado | Cambio |
|---|---|---|---|
| 1 | Ponder cuenta el tiempo como propio | ✅ Resuelto | El reloj arranca en `ponderhit` (`g_ponder_offset` en `search.cpp`). |
| 2 | Sobrepaso grave del tiempo | ✅ Resuelto | Tope duro `max_time_ms` dentro de `time_up()`. |
| 3 | `bestmove` fantasma (varios por `go`) | ✅ Resuelto | Se ignora `go` en curso; se suprime `bestmove` en abortos por `position`/`ucinewgame`/`setoption`/`quit`. |
| 4 | Pérdidas por tiempo | ✅ Resuelto | Presupuesto desde `wtime`/`btime` con techo duro = reloj − 10 ms. |
| 5 | Procesos hijos huérfanos | ✅ Resuelto (defensivo) | Job Object `KILL_ON_JOB_CLOSE` en Windows. |
| 6 | No procesa tras token desconocido | ✅ Resuelto | El bucle principal descarta tokens iniciales desconocidos. |
| 7 | `go` sin parámetros termina solo | ✅ Resuelto | `go` sin límites = búsqueda infinita hasta `stop`. |
| 8 | Acepta FEN inválido | ✅ Resuelto | `validate_fen()` rechaza FEN malformados conservando la posición previa. |

El motor compilado para release es `Hy3 1.7.exe` (MSVC, `build_release.bat`).
