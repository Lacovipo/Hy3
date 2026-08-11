import subprocess, sys, random, time, chess

# Driver de enfrentamiento entre dos binarios UCI del motor Hy3.
# hy3.exe  = versión 1.1 (actual)
# hy3_v10.exe = versión 1.0 (baseline)

ENG_A = "hy3.exe"      # version 1.1 (a evaluar)
ENG_B = "hy3_v10.exe"  # version 1.0

def run_match(white_bin, black_bin, moves_per_side=60, time_ms=200):
    """Juega una partida completa. Devuelve '1-0','0-1' o '1/2-1/2'."""
    wa = subprocess.Popen([white_bin], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                          text=True, bufsize=1)
    bb = subprocess.Popen([black_bin], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                          text=True, bufsize=1)

    def send(p, line):
        try:
            p.stdin.write(line + "\n")
            p.stdin.flush()
        except Exception:
            pass

    def read_line(p, deadline):
        while True:
            if time.time() > deadline:
                return ""
            l = p.stdout.readline()
            if l == "":
                return ""
            return l.rstrip("\n")

    def wait_ready(p, deadline):
        send(p, "isready")
        while True:
            l = read_line(p, deadline)
            if l == "" or l.startswith("readyok"):
                return

    def read_bestmove(p, deadline):
        while True:
            l = read_line(p, deadline)
            if l == "":
                return "0000"
            if l.startswith("bestmove"):
                return l.split()[1] if len(l.split()) > 1 else "0000"
            if l.startswith("info"):
                continue

    send(wa, "uci"); send(bb, "uci")
    wait_ready(wa, time.time() + 5); wait_ready(bb, time.time() + 5)
    send(wa, "ucinewgame"); send(bb, "ucinewgame")
    send(wa, "position startpos"); send(bb, "position startpos")

    board = chess.Board()
    ply = 0
    cur_bin = {True: wa, False: bb}   # True=white
    while not board.is_game_over() and ply < moves_per_side * 2:
        side = board.turn == chess.WHITE
        p = cur_bin[side]
        # Resincroniza AMBOS motores con la posición completa (evita desfases).
        move_str = " ".join(m.uci() for m in board.move_stack)
        cmd = "position startpos" + ((" moves " + move_str) if move_str else "")
        send(wa, cmd); send(bb, cmd)
        wait_ready(wa, time.time() + 3); wait_ready(bb, time.time() + 3)
        send(p, f"go movetime {time_ms}")
        deadline = time.time() + (time_ms / 1000.0) + 10
        bm = read_bestmove(p, deadline)
        if bm == "0000" or bm == "(none)":
            print("  [no bestmove] ply", ply, "turn", ("W" if side else "B"))
            break
        try:
            mv = chess.Move.from_uci(bm)
        except Exception:
            break
        if mv not in board.legal_moves:
            print("  [illegal]", bm, "ply", ply)
            break
        board.push(mv)
        ply += 1

    if board.is_checkmate():
        res = "1-0" if board.turn == chess.BLACK else "0-1"
    elif board.is_stalemate() or board.is_insufficient_material() or board.can_claim_fifty_moves():
        res = "1/2-1/2"
    else:
        res = "1/2-1/2"
    send(wa, "quit"); send(bb, "quit")
    try: wa.wait(timeout=3)
    except Exception: wa.kill()
    try: bb.wait(timeout=3)
    except Exception: bb.kill()
    return res

def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 10
    time_ms = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    wins = draws = losses = 0
    for i in range(n):
        # Alternar colores: mitad con v1.1 blanco, mitad negro
        v11_white = (i % 2 == 0)
        if v11_white:
            res = run_match(ENG_A, ENG_B, time_ms=time_ms)
        else:
            res = run_match(ENG_B, ENG_A, time_ms=time_ms)
        # res es desde la perspectiva de (ENG_A=white, ENG_B=black)
        if res == "1-0":
            if v11_white: wins += 1
            else: losses += 1
        elif res == "0-1":
            if v11_white: losses += 1
            else: wins += 1
        else:
            draws += 1
        print(f"game {i+1}: v11_white={v11_white} result={res}  (W={wins} L={losses} D={draws})")
    total = wins + losses + draws
    print(f"FINAL: wins={wins} losses={losses} draws={draws} total={total}")
    if wins + losses > 0:
        # Elo aproximado (fórmula de percentil logístico simplificado)
        w = wins + draws/2.0
        score = w / total
        # Elo diff aprox (asumiendo base 0 para v1.0)
        import math
        if 0 < score < 1:
            elo_diff = 400 * math.log10(score / (1 - score))
        elif score >= 1:
            elo_diff = 400
        else:
            elo_diff = -400
        print(f"score%={score*100:.1f}  Elo(v1.1 vs v1.0) ~= {elo_diff:+.0f}")

if __name__ == "__main__":
    main()
