from time import time
from random import shuffle, randrange, seed, random
import torch
import pyspiel
from open_spiel.python.bots.cycle import CycleBot
from open_spiel.python.bots.prisoner import TFTBot, GrimBot, SwitchBot
from prediction_net import PredictionNet
from ml_util import get_inner_game, get_extra_info_size, create_info_matrix_game
from prediction_train import prediction_train_batches, play_match

"""Test an asymmetric (3x2 actions) game"""

def rotate(lst, n):
    if not lst:
        return []
    n %= len(lst)
    return lst[-n:] + lst[:-n]

def create_3x2(perm):
    """Create a 3x2 possibly cooperative game"""
    short_name = '3x2_' + str(perm)
    long_name = '3x2_' + str(perm)
    row_names = ('T', 'C', 'B')
    col_names = ('L', 'R')
    br_utils = [[-1, 1], [0, -1], [1, 0]]
    bc_utils = [[1,-1], [1, -1], [-1, 1]]
    row_utils = rotate(br_utils, perm % 3)
    col_utils = rotate(bc_utils, perm % 2)
    return pyspiel.create_matrix_game(long_name, short_name,
        row_names, col_names, row_utils, col_utils)


if __name__ == '__main__':
    started = time()
    reps = 50
    recall = 6
    inner_game = create_3x2(0)
    rep_game = pyspiel.create_repeated_game(inner_game, {
        "num_repetitions": reps,"recall": recall, "relative":True})
    input_size = get_extra_info_size(rep_game)
    net = PredictionNet(players=2, actions=3, input_size=input_size)
    optimizer = torch.optim.Adam(
       net.parameters(),
       lr=0.001,
    )
    bots_games = []
    for ii in range(6):
        inner_game = create_3x2(ii)
        rep_game = pyspiel.create_repeated_game(inner_game, {
          "num_repetitions": reps,"recall": recall, "relative":True})
        info_game = create_info_matrix_game(rep_game)
        for player_id in 0, 1:
            bot = CycleBot([0])
            bots_games.append([info_game, bot, player_id])
            bot = CycleBot([1])
            bots_games.append([info_game, bot, player_id])
            bot = CycleBot([0, 1])
            bots_games.append([info_game, bot, player_id])
            bot = CycleBot([1, 0])
            bots_games.append([info_game, bot, player_id])
            if player_id == 0:
                continue
            bot = CycleBot([0, 1, 2])
            bots_games.append([info_game, bot, player_id])
            bot = CycleBot([1, 2, 0])
            bots_games.append([info_game, bot, player_id])
            bot = TFTBot(player_id=0)
            bots_games.append([info_game, bot, player_id])
    prediction_train_batches(500, bots_games, net, optimizer)
    for elm in bots_games:
        game = elm[0]
        bot = elm[1]
        bot_id = 1 - elm[2]
        play_match(game, bot, bot_id, net)
    print("elasped", int(time() - started))
