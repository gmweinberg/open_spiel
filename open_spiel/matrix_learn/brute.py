import pyspiel
from open_spiel.python.bots.cycle import CycleBot
from open_spiel.python.bots.prisoner import TFTBot, GrimBot
from util import get_cycle_bots

action_counts = {}

def populate_action_counts(game, bots):
    for bot in bots:
        for player_id in range(2):
            bot.player_id = 1 - player_id
            bot.restart()
            state = game.new_initial_state()
            populate_action_counts_state(state, bot, player_id)

def populate_action_counts_state(state, bot, player_id):
    global action_counts
    if state.is_terminal():
        return
    key = (player_id, tuple(state.observation_tensor(player_id)))
    state1 = state.clone()
    if key not in action_counts:
        action_counts[key] = [0, 0]
    bot_action = bot.step(state) # TODO deal with nondeterministic bots
    action_counts[key][bot_action] += 1
    if player_id == 0:
        state.apply_actions([0, bot_action])
    else:
        state.apply_actions([bot_action, 0])
    populate_action_counts_state(state, bot, player_id)
    bot.restart_at(state1)
    if player_id == 0:
        state1.apply_actions([1, bot_action])
    else:
        state1.apply_actions([bot_action, 1])
    populate_action_counts_state(state1, bot, player_id)



if __name__ == '__main__':

    from argparse import ArgumentParser
    parser = ArgumentParser()
    parser.add_argument('--reps', help='repetitions', type=int, default=8)
    parser.add_argument('--recall', help='recall', type=int, default=4)
    parser.add_argument('--game', default='pd2')
    # supported: coordination, pd, ...
    args = parser.parse_args()
    game_name = 'matrix_' + args.game
    game = pyspiel.create_repeated_game(game_name, {
      "num_repetitions": args.reps,"recall": args.recall,})
    bots = [TFTBot(1), CycleBot([0]), CycleBot([1]), CycleBot([0, 1])]
    populate_action_counts(game, bots)
    print(action_counts)


