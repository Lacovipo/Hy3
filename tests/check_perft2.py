import chess, subprocess

def our_count(fen):
    out = subprocess.run(["movelist.exe", fen], capture_output=True, text=True).stdout.splitlines()
    n = int(out[0])
    return n, set(out[1:1+n])

def ref_perft2(fen):
    b = chess.Board(fen); total=0
    for m in b.legal_moves:
        b.push(m); total += b.legal_moves.count(); b.pop()
    return total

def our_perft2(fen):
    b = chess.Board(fen); total=0
    for m in b.legal_moves:
        b.push(m); total += our_count(b.fen())[0]; b.pop()
    return total

fen = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
print("our perft2:", our_perft2(fen))
print("ref perft2:", ref_perft2(fen))
