from itertools import combinations
from collections import defaultdict
import pyspiel
from open_spiel.python.bots.cycle import CycleBot
from open_spiel.python.bots.prisoner import TFTBot, GrimBot

def tournament(game, bots):
    """Round Robin torunament. Each bot must have a unique name."""
    scores = defaultdict(int)
    for apair in combinations(bots, 2):
        apair[0].player_id = 0
        apair[1].player_id = 1
        apair[0].restart()
        apair[1].restart()
        state = game.new_initial_state()
        while not state.is_terminal():
           bot0_action = apair[0].step(state)
           bot1_action = apair[1].step(state)
           state.apply_actions([bot0_action, bot1_action])
           for ii in range(2):
               scores[apair[ii].name] += state.rewards()[ii]
    scores = dict(scores)
    return scores

if __name__ == '__main__':
    from argparse import ArgumentParser
    parser = ArgumentParser()
    parser.add_argument('--game', default='pd')
    parser.add_argument('--reps', help='repetitions', type=int, default=50)
    parser.add_argument('--recall', help='recall', type=int, default=4)
    args = parser.parse_args()
    game_name = 'matrix_' + args.game
    game = pyspiel.create_repeated_game(game_name, {
      "num_repetitions": args.reps,"recall": args.recall,})
    bots = [CycleBot([0]), CycleBot([1]), CycleBot([0, 1]), TFTBot(), GrimBot()]
    print(tournament(game, bots))




