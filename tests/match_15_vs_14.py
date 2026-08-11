import subprocess, sys, time, chess

# Enfrentamiento v1.5 (ENG_A) vs v1.4 (ENG_B).
ENG_A = "Hy3 1.5.exe"
ENG_B = "Hy3 1.4.exe"

def run_match(white_bin, black_bin, moves_per_side=80, time_ms=100):
    wa = subprocess.Popen([white_bin], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
    bb = subprocess.Popen([black_bin], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
    def send(p, line):
        try:
            p.stdin.write(line + "\n"); p.stdin.flush()
        except Exception: pass
    def read_line(p, deadline):
        if time.time() > deadline: return ""
        l = p.stdout.readline()
        return "" if l == "" else l.rstrip("\n")
    def wait_ready(p, deadline):
        send(p, "isready")
        while True:
            l = read_line(p, deadline)
            if l == "" or l.startswith("readyok"): return
    def read_bestmove(p, deadline):
        while True:
            l = read_line(p, deadline)
            if l == "": return "0000"
            if l.startswith("bestmove"):
                parts = l.split(); return parts[1] if len(parts) > 1 else "0000"
    send(wa, "uci"); send(bb, "uci")
    wait_ready(wa, time.time()+5); wait_ready(bb, time.time()+5)
    send(wa, "ucinewgame"); send(bb, "ucinewgame")
    board = chess.Board(); ply = 0
    cur = {True: wa, False: bb}
    while not board.is_game_over(claim_draw=True) and ply < moves_per_side*2:
        side = board.turn == chess.WHITE
        p = cur[side]
        mv_str = " ".join(m.uci() for m in board.move_stack)
        cmd = "position startpos" + ((" moves " + mv_str) if mv_str else "")
        send(wa, cmd); send(bb, cmd)
        wait_ready(wa, time.time()+3); wait_ready(bb, time.time()+3)
        send(p, f"go movetime {time_ms}")
        bm = read_bestmove(p, time.time() + time_ms/1000.0 + 10)
        if bm in ("0000","(none)"): break
        try: mv = chess.Move.from_uci(bm)
        except Exception: break
        if mv not in board.legal_moves:
            print("  [illegal]", bm, "ply", ply); break
        board.push(mv); ply += 1
    if board.is_checkmate():
        res = "1-0" if board.turn == chess.BLACK else "0-1"
    else:
        res = "1/2-1/2"
    send(wa, "quit"); send(bb, "quit")
    for p in (wa, bb):
        try: p.wait(timeout=3)
        except Exception: p.kill()
    return res

def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 10
    time_ms = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    wins = draws = losses = 0
    for i in range(n):
        a_white = (i % 2 == 0)
        if a_white:
            res = run_match(ENG_A, ENG_B, time_ms=time_ms)
            outcome = "W" if res=="1-0" else ("L" if res=="0-1" else "D")
        else:
            res = run_match(ENG_B, ENG_A, time_ms=time_ms)
            outcome = "W" if res=="0-1" else ("L" if res=="1-0" else "D")
        if outcome=="W": wins+=1
        elif outcome=="L": losses+=1
        else: draws+=1
        print(f"game {i+1}: A_white={a_white} res={res} -> v1.5 {outcome}  (W={wins} L={losses} D={draws})", flush=True)
    total = wins+losses+draws
    print(f"FINAL v1.5 vs v1.4: wins={wins} losses={losses} draws={draws} total={total}")
    import math
    w = wins + draws/2.0; score = w/total if total else 0.5
    if 0 < score < 1: elo = 400*math.log10(score/(1-score))
    elif score>=1: elo = 400
    else: elo = -400
    print(f"score%={score*100:.1f}  Elo(v1.5 vs v1.4) ~= {elo:+.0f}")

if __name__ == "__main__":
    main()
