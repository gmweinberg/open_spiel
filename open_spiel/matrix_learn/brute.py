from functools import cache
import pyspiel
from open_spiel.python.bots.cycle import CycleBot
from open_spiel.python.bots.prisoner import TFTBot, GrimBot
from ml_util import get_cycle_bots

"""Brutally go through every possible combination of player actions for each bot.
   Determine optimal play for the population."""

rewards = {} 
action_counts = {} # key = (player_id, observation_tensor)
transitions = {}

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
    state0 = state.clone()
    state1 = state.clone()
    if key not in action_counts:
        action_counts[key] = [0, 0]
        transitions[key] = {}
    bot_action = bot.step(state) # TODO deal with nondeterministic bots
    action_counts[key][bot_action] += 1
    if player_id == 0:
        state0.apply_actions([0, bot_action])
        rewards[(0, bot_action)] = state0.rewards()
        nukey = (player_id, tuple(state0.observation_tensor(player_id)))
        transitions[key][(0, bot_action)] = nukey
    else:
        state0.apply_actions([bot_action, 0])
        rewards[(bot_action, 0)] = state0.rewards()
        nukey = (player_id, tuple(state0.observation_tensor(player_id)))
        transitions[key][(bot_action, 0)] = nukey
    populate_action_counts_state(state0, bot, player_id)
    bot.restart_at(state1)
    if player_id == 0:
        state1.apply_actions([1, bot_action])
        rewards[(1, bot_action)] = state1.rewards()
        nukey = (player_id, tuple(state1.observation_tensor(player_id)))
        transitions[key][(1, bot_action)] = nukey
    else:
        state1.apply_actions([bot_action, 1])
        rewards[(bot_action, 1)] = state1.rewards()
        nukey = (player_id, tuple(state1.observation_tensor(player_id)))
        transitions[key][(bot_action, 1)] = nukey
    populate_action_counts_state(state1, bot, player_id)

@cache
def expected_score(key, depth):
    """Expected score using optimal play.
       Returns (best_action, score)."""

    player_id = key[0]
    ac = action_counts[key]
    total = sum(ac)
    probs = [ac[0] / total, ac[1] / total]

    scores = []

    for my_action in (0, 1):
        score = 0.0

        for bot_action in (0, 1):
            if probs[bot_action] == 0:
                continue
            if player_id == 0:
                joint_action = (my_action, bot_action)
            else:
                joint_action = (bot_action, my_action)

            immediate = rewards[joint_action][player_id]

            if depth == 0:
                future = 0.0
            else:
                new_key = transitions[key][joint_action]
                future = expected_score(new_key, depth - 1)[1]

            score += probs[bot_action] * (immediate + future)

        scores.append(score)

    if scores[0] > scores[1]:
        return 0, scores[0]
    else:
        return 1, scores[1]

def play_match(game, bot):
    """Have brute play game against a bot.
       print the average score."""
    # for now brute is always player 0
    player_id = 0
    depth = 10
    score = 0
    rounds = 0
    bot.player_id = 1 - player_id
    bot.restart()
    state = game.new_initial_state()
    while not state.is_terminal():
        key = (player_id, tuple(state.observation_tensor(player_id)))
        bot_action = bot.step(state)
        try:
            brute_action = expected_score(key, depth)[0]
        except:
            brute_action = expected_score(key, depth)[0]
        #print('brute', brute_action, 'bot', bot_action)
        state.apply_actions([brute_action, bot_action])
        score += state.rewards()[player_id]
        rounds += 1
    print("avg score vs ", bot.name, score/rounds)



if __name__ == '__main__':

    from argparse import ArgumentParser
    parser = ArgumentParser()
    parser.add_argument('--reps', help='repetitions', type=int, default=50)
    parser.add_argument('--recall', help='recall', type=int, default=4)
    parser.add_argument('--game', default='pd2')
    # supported: coordination, pd, ...
    args = parser.parse_args()
    game_name = 'matrix_' + args.game
    game = pyspiel.create_repeated_game(game_name, {
      "num_repetitions": 2 * args.recall + 1,"recall": args.recall,})
    bots = [TFTBot(1), CycleBot([0]), CycleBot([1]), CycleBot([1, 0],), CycleBot([0, 1],),GrimBot(1)]
    # bots = [TFTBot(1), CycleBot([0]), CycleBot([1])]
    populate_action_counts(game, bots)
    #print(action_counts)
    #print(rewards)
    game = pyspiel.create_repeated_game(game_name, {
      "num_repetitions": 2 * args.reps + 1,"recall": args.recall,})
    #print(len(action_counts))
    for bot in bots:
        play_match(game, bot)


