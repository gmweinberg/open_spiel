from time import time
from random import shuffle, randrange, seed, random
import torch
import pyspiel
from open_spiel.python.bots.cycle import CycleBot
from open_spiel.python.bots.prisoner import TFTBot, GrimBot, SwitchBot
from prediction_net import PredictionNet
from ml_util import get_inner_game, get_extra_info_size, create_info_matrix_game
from prediction_train import prediction_train_batches, play_match

"""Verify that we can successfully use the same net to play multiple games
   with a mix of players. We can mix The games better all have the same recall
   and action space so the observation tensors stay the same size."""

def create_rep_game(game_name, reps, recall):
   rep_game = pyspiel.create_repeated_game(game_name, {
        "num_repetitions": reps,"recall": recall, "relative":True})
   return rep_game



if __name__ == '__main__':
    bots_games = []
    reps = 50
    recall = 6
    seed_ = 12345
    seed(seed_)
    torch.manual_seed(seed_)
    pd2 = create_rep_game('matrix_pd2', reps, recall)
    bos = create_rep_game('matrix_bos', reps, recall)
    input_size = get_extra_info_size(pd2)
    pd2 = create_info_matrix_game(pd2)
    bos = create_info_matrix_game(bos)

    net = PredictionNet(players=2, actions=2, input_size=input_size)
    optimizer = torch.optim.Adam(
       net.parameters(),
       lr=0.001,
    )
    for player_id in [0, 1]:
        for rep_game in [pd2, bos]:
            bots_games.append((rep_game, TFTBot(player_id), player_id))
            bots_games.append((rep_game, SwitchBot(player_id=player_id), player_id))
            bots_games.append((rep_game, SwitchBot(player_id=player_id, same=True), player_id))
            bots_games.append((rep_game, CycleBot([0]), player_id))
            bots_games.append((rep_game, CycleBot([1]), player_id))
            bots_games.append((rep_game, CycleBot([0, 1]), player_id))
            bots_games.append((rep_game, CycleBot([1,0]), player_id))
    prediction_train_batches(500, bots_games, net, optimizer)
    for elm in bots_games:
        game = elm[0]
        bot = elm[1]
        bot_id = 1 - elm[2]
        play_match(game, bot, bot_id, net)

