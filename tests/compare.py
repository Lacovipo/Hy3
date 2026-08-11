import chess, subprocess, sys

def our_moves(fen):
    out = subprocess.run(["movelist.exe", fen], capture_output=True, text=True).stdout.splitlines()
    n = int(out[0])
    return set(out[1:1+n])

def rec(fen, depth, path=""):
    board = chess.Board(fen)
    ref = set(m.uci() for m in board.legal_moves)
    ours = our_moves(fen)
    if ref != ours:
        print("DIFF", fen, "path=", path)
        print("  missing(ours lacks):", sorted(ref - ours))
        print("  extra(ours has):   ", sorted(ours - ref))
        print()
    if depth > 0:
        for m in board.legal_moves:
            board.push(m)
            rec(board.fen(), depth-1, path + " " + m.uci())
            board.pop()

if __name__ == "__main__":
    fen = sys.argv[1]
    depth = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    rec(fen, depth)
