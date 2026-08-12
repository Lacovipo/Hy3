# Informe para el autor de Hy3 1.7

## Que es esto y como usarlo

Este documento lo ha generado **Camifurlo**, un analizador de solidez funcional de motores de ajedrez: somete al motor a una bateria de pruebas de protocolo, tiempo, ponder, ciclo de vida y partidas reales, y anota todo lo que se aparta de la especificacion o resulta fragil.

A diferencia del informe principal (que explica como debe *sortear* estos problemas una GUI), este va dirigido a **arreglar el motor en su codigo fuente**.

> **Si eres un modelo de IA al que le han pasado este documento junto al codigo fuente del motor**: cada apartado trae el sintoma observado, la secuencia exacta que lo reproduce, la evidencia del log, la causa que suele haber detras y el cambio sugerido. Empieza por los de prioridad mas alta. Antes de tocar nada, localiza en el fuente el area indicada y confirma que la causa descrita se corresponde con lo que hace ese codigo: la causa es la explicacion **tipica**, no una certeza sobre este motor en concreto. Al terminar, la forma de verificar es volver a pasar Camifurlo y comprobar que el hallazgo ha desaparecido.

## Motor analizado

| Campo | Valor |
|---|---|
| Nombre declarado | Hy3 1.7 |
| Autor declarado | Hy3 |
| Protocolo analizado | uci |
| Ejecutable | Hy3 1.7.exe |
| MD5 | ed614532f151dd46e6b54e852c5c5e70 |
| Fecha del analisis | 2026-08-12 03:33:34 |
| Duracion de la bateria | 349.6 minutos |
| Partidas jugadas | 77 |
| Pruebas superadas | 98 |

## Resumen de lo que hay que arreglar

| # | Prioridad | Esfuerzo | Area del codigo | Problema |
|---|---|---|---|---|
| 1 | 🔴 CRITICA | medio | bucle de busqueda | El motor no devuelve 'bestmove' |
| 2 | 🟠 ALTA | medio | gestion del tiempo de busqueda | El motor pierde por tiempo |
| 3 | 🟡 MEDIA | trivial | gestion del tiempo de busqueda | 'go' sin parametros se comporta como busqueda infinita |
| 4 | 🟡 MEDIA | medio | gestion del tiempo de busqueda | La busqueda no termina por si sola en el plazo previsto |
| 5 | 🔵 BAJA | medio | gestion del tiempo de busqueda | El motor ignora 'go nodes N' |
| 6 | 🔵 BAJA | medio | parseo de posiciones (FEN y lista de jugadas) | El motor acepta un FEN sintacticamente invalido |

**Por donde empezar**: 1 de estos 6 arreglos son de esfuerzo trivial, de unas pocas lineas — 'go' sin parametros se comporta como busqueda infinita.

## Detalle

### 1. El motor no devuelve 'bestmove'

| Campo | Valor |
|---|---|
| Prioridad | 🔴 CRITICA |
| Esfuerzo estimado | medio |
| Donde mirar | bucle de busqueda |
| Veces observado | 2 |
| Codigo del comportamiento | `bestmove_ausente` |
| Pruebas que lo detectaron | `F04_position_buscando`, `F05_setoption_buscando` |

**Sintoma y por que importa**: La busqueda termina (o no) sin la linea obligatoria 'bestmove'. La partida se queda parada para siempre.

**Comportamiento correcto**: Siempre 'bestmove <jugada>' al terminar la busqueda.

**Lo que hizo este motor**:

```
con 'position' en mitad de una busqueda no llega ningun bestmove
con 'setoption' en mitad de una busqueda no llega ningun bestmove
```

**Secuencia que lo reproduce** (enviada al motor por su entrada estandar):

```
position startpos
go movetime 1500
position startpos moves e2e4
```

**Causa habitual**: La busqueda sale por un camino que no emite 'bestmove': excepcion capturada, lista de jugadas vacia, o un 'return' temprano al agotarse el tiempo.

**Cambio sugerido**: Garantizar por construccion que todo camino de salida de la busqueda emite exactamente una linea 'bestmove'. Lo mas robusto es emitirla en un unico punto (patron 'defer'/RAII o un bloque final), con una jugada legal de reserva elegida antes de empezar a buscar.

<sub>Mientras no se arregle, la GUI tiene que: Timeout duro por jugada = tiempo asignado + margen. Al vencer: 'stop', esperar un poco mas, y si sigue sin contestar, matar y relanzar, dando la partida por perdida.</sub>

<details><summary>Extracto del log de comunicacion</summary>

```
[21:59:49.437] [+    266.2ms] >> ucinewgame
[21:59:49.437] [+    266.2ms] >> isready
[21:59:49.437] [+    266.2ms] << readyok   <-- CRLF
[21:59:49.437] [+    266.3ms] >> position startpos
[21:59:49.437] [+    266.3ms] >> go movetime 1500
[21:59:49.437] [+    266.3ms] >> position startpos moves e2e4
```

</details>

### 2. El motor pierde por tiempo

| Campo | Valor |
|---|---|
| Prioridad | 🟠 ALTA |
| Esfuerzo estimado | medio |
| Donde mirar | gestion del tiempo de busqueda |
| Veces observado | 7 |
| Codigo del comportamiento | `bandera` |
| Pruebas que lo detectaron | `G_autoplay` |

**Sintoma y por que importa**: Agota el reloj antes de devolver la jugada.

**Comportamiento correcto**: Terminar siempre antes de que caiga la bandera.

**Lo que hizo este motor**:

```
6 perdidas por tiempo en 6 partidas con 1s a caer bandera ponder=no
6 perdidas por tiempo en 6 partidas con 1s a caer bandera ponder=si
8 perdidas por tiempo en 8 partidas con 2s + 0.1s ponder=si
6 perdidas por tiempo en 6 partidas con 10s + 0.1s ponder=si
3 perdidas por tiempo en 3 partidas con 30s a caer bandera ponder=si
2 perdidas por tiempo en 2 partidas con 10 jugadas / 20s ponder=si
2 perdidas por tiempo en 2 partidas con 60s + 1s ponder=si
```

**Causa habitual**: El reparto del tiempo no tiene en cuenta el reloj real que envia la GUI, o no reserva nada para las jugadas siguientes.

**Cambio sugerido**: Recalcular el presupuesto en cada jugada a partir de 'wtime'/'btime' del comando 'go' (no de un valor propio), y no gastar nunca mas de un tercio del tiempo restante en una sola jugada salvo emergencia.

<sub>Mientras no se arregle, la GUI tiene que: Usar controles de tiempo con incremento, subir el tiempo base o activar 'ignorar bandera'. Comprobar antes si el problema es el ponder (ver la seccion de ponder del informe).</sub>

### 3. 'go' sin parametros se comporta como busqueda infinita

| Campo | Valor |
|---|---|
| Prioridad | 🟡 MEDIA |
| Esfuerzo estimado | trivial |
| Donde mirar | gestion del tiempo de busqueda |
| Veces observado | 1 |
| Codigo del comportamiento | `go_sin_limite_infinito` |
| Pruebas que lo detectaron | `C_go_pelado` |

**Sintoma y por que importa**: Es lo que dice la especificacion, pero muchas GUIs lo mandan por error.

**Comportamiento correcto**: Busqueda infinita hasta 'stop'.

**Lo que hizo este motor**:

```
'go' sin parametros busca indefinidamente (comportamiento correcto segun la especificacion)
```

**Causa habitual**: Comportamiento correcto segun la especificacion.

**Cambio sugerido**: Nada que arreglar.

<sub>Mientras no se arregle, la GUI tiene que: No enviar nunca 'go' pelado: incluir siempre movetime, depth o reloj.</sub>

### 4. La busqueda no termina por si sola en el plazo previsto

| Campo | Valor |
|---|---|
| Prioridad | 🟡 MEDIA |
| Esfuerzo estimado | medio |
| Donde mirar | gestion del tiempo de busqueda |
| Veces observado | 281 |
| Codigo del comportamiento | `busqueda_no_termina_sola` |
| Pruebas que lo detectaron | `B04_nodes`, `C_nodes_50k`, `G_autoplay` |

**Sintoma y por que importa**: El motor no respeta el limite que se le dio (movetime, reloj o profundidad) y sigue buscando hasta que la GUI le manda 'stop'. Obedece el stop sin problema, asi que es un fallo de gestion del tiempo, no de comunicacion.

**Comportamiento correcto**: Terminar solo dentro del limite indicado.

**Lo que hizo este motor**:

```
la busqueda no termino por si sola en 20010 ms (go nodes 200000); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 30014 ms (nodes_50k ply 1, reloj 0 ms); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 30000 ms (nodes_50k ply 2, reloj 0 ms); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 30004 ms (nodes_50k ply 3, reloj 0 ms); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 30013 ms (nodes_50k ply 4, reloj 0 ms); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 30007 ms (nodes_50k ply 5, reloj 0 ms); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 30002 ms (nodes_50k ply 6, reloj 0 ms); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 30016 ms (nodes_50k ply 7, reloj 0 ms); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 30006 ms (nodes_50k ply 8, reloj 0 ms); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 30015 ms (nodes_50k ply 9, reloj 0 ms); obedecio al 'stop' en 1 ms
la busqueda no termino por si sola en 60013 ms (nodes_50k ponder=False partida 1 ply 1); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 60006 ms (nodes_50k ponder=False partida 1 ply 2); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 60004 ms (nodes_50k ponder=False partida 1 ply 3); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 60016 ms (nodes_50k ponder=False partida 1 ply 4); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 60010 ms (nodes_50k ponder=False partida 1 ply 5); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 60016 ms (nodes_50k ponder=False partida 1 ply 6); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 60010 ms (nodes_50k ponder=False partida 1 ply 7); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 60005 ms (nodes_50k ponder=False partida 1 ply 8); obedecio al 'stop' en 0 ms
la busqueda no termino por si sola en 60009 ms (nodes_50k ponder=False partida 1 ply 9); obedecio al 'stop' en 0 ms
```

**Causa habitual**: El limite se comprueba en un sitio al que no siempre se llega, o el modo 'infinite' se queda pegado.

**Cambio sugerido**: Unificar la comprobacion del limite (tiempo, nodos o profundidad) en una unica funcion que se llame desde el bucle de nodos, y respetarla en todos los modos.

<sub>Mientras no se arregle, la GUI tiene que: Mantener el timeout duro por jugada y el 'stop' de emergencia: es exactamente lo que evita que la partida se quede parada. Ajustar ademas el tiempo asignado para que el stop llegue antes de que caiga la bandera.</sub>

<details><summary>Extracto del log de comunicacion</summary>

```
[21:44:06.339] [+   9314.7ms] >> go nodes 200000
[21:44:26.348] [+  29324.3ms] ## TIMEOUT esperando bestmove: enviamos stop
[21:44:26.348] [+  29324.4ms] >> stop
[21:44:26.348] [+  29324.5ms] << bestmove e2e4 ponder b8c6   <-- CRLF
```

</details>

### 5. El motor ignora 'go nodes N'

| Campo | Valor |
|---|---|
| Prioridad | 🔵 BAJA |
| Esfuerzo estimado | medio |
| Donde mirar | gestion del tiempo de busqueda |
| Veces observado | 1 |
| Codigo del comportamiento | `no_soporta_nodes` |
| Pruebas que lo detectaron | `B04_nodes` |

**Comportamiento correcto**: Terminar al alcanzar el numero de nodos.

**Lo que hizo este motor**:

```
'go nodes 200000' no termino en 20 s
```

**Causa habitual**: El parametro 'nodes' no se lee.

**Cambio sugerido**: Contar nodos y abortar al superar el limite. Util para pruebas reproducibles.

<sub>Mientras no se arregle, la GUI tiene que: No ofrecer control por nodos para este motor.</sub>

<details><summary>Extracto del log de comunicacion</summary>

```
[21:44:06.339] [+   9314.7ms] >> go nodes 200000
[21:44:26.348] [+  29324.3ms] ## TIMEOUT esperando bestmove: enviamos stop
[21:44:26.348] [+  29324.4ms] >> stop
[21:44:26.348] [+  29324.5ms] << bestmove e2e4 ponder b8c6   <-- CRLF
```

</details>

### 6. El motor acepta un FEN sintacticamente invalido

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
con el FEN invalido 'campos_de_menos' devuelve g1f3 en vez de quejarse
con el FEN invalido 'fila_incompleta' devuelve g1f3 en vez de quejarse
con el FEN invalido 'pieza_invalida' devuelve g1f3 en vez de quejarse
con el FEN invalido 'sin_reyes' devuelve g1f3 en vez de quejarse
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

_Generado por Camifurlo v1.0.0 el 2026-08-12._

---

## Estado de resolución (revisión del autor — Hy3)

Revisado el 2026-08-11 por Hy. Se leyó el código fuente, no solo el informe, y se
recompiló `Hy3 1.7.exe` (versión **sin** incrementar). Estado de cada hallazgo:

| # | Hallazgo | Estado | Notas |
|---|---|---|---|
| 1 | No devuelve `bestmove` | **RESUELTO** | Causa real: la propia 1.7 introdujo `g_suppress_bestmove`, que suprimía el `bestmove` al abortar por `position`/`setoption`/`quit`. Mecanismo eliminado; `run_search()` emite siempre un `bestmove`. |
| 2 | Pierde por tiempo | **RESUELTO** | Causa real: `g_ponder_offset` obsoleto de una partida con ponder previa hacía que `effective_elapsed` fuera negativo y `time_up()` nunca se disparara. Ahora se reinicia en cada `search()`. |
| 3 | `go` sin parámetros = infinita | **NO APLICA** | Comportamiento correcto según la especificación UCI (el propio informe dice "Nada que arreglar"). |
| 4 | La búsqueda no termina sola | **RESUELTO** | Mismo origen que #2 (offset de ponder). Además, las instancias `nodes_50k` se resolvieron con el límite de nodos (#5). |
| 5 | Ignora `go nodes N` | **RESUELTO** | `lim.max_nodes` se parsea y `time_up()` lo respeta. `go nodes 200000` termina en ~0,3 s. |
| 6 | Acepta un FEN inválido | **FALSO POSITIVO** | `validate_fen()` ya rechaza los 6 casos (campos, filas, rey, turno, ep) y avisa con `info string Invalid FEN ignored`. Verificado empíricamente contra el binario. Sin cambio. |

Verificación: `tests/verify_timefix.py` → TODO OK (bestmove siempre emitido, `go nodes` termina, FEN inválidos rechazados).