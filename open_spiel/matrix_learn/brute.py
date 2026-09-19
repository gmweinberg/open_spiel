import pyspiel
from open_spiel.python.bots.cycle import CycleBot
from open_spiel.python.bots.prisoner import TFTBot, GrimBot

if __name__ == '__main__':
    from argparse import ArgumentParser
    parser = ArgumentParser()
    parser.add_argument('--reps', help='repetitions', type=int, default=50)
    parser.add_argument('--recall', help='recall', type=int, default=4)
    parser.add_argument('--game', default='coordination')
    # supported: coordination, pd, ...
    args = parser.parse_args()
    game_name = 'matrix_' + args.game
    game = pyspiel.create_repeated_game(game_name, {
      "num_repetitions": args.reps,"recall": args.recall,})
    brutal = pass


