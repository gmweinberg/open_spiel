#!/usr/bin/python
from time import time
from functools import cache
from random import shuffle, randrange, seed, random
import torch
import pyspiel
from open_spiel.python.bots.cycle import CycleBot
from open_spiel.python.bots.prisoner import TFTBot, GrimBot
from matrix_net import MatrixNet

"""Try to learn the probability that the the player will play a given move in a given situation."""

expected_score_cache = {}

def prediction_train(game, net, bots, optimizer, evaluate=False, clue=False):
    """Learn the probabilities of bots playing moves in a game against a random player.
       These will not be the same as the probabilities against nonrandom players because
       the chance of getting into a history depends on the actions of both players.
    """
    losses = []
    reports = {}
    depth = 4

    for bot in bots:
        for player_id in range(2):
            bot.player_id = 1 - player_id
            bot.restart()
            state = game.new_initial_state()

            while not state.is_terminal():
                bot_action = bot.step(state)

                obs = torch.tensor(
                    list(state.observation_tensor(player_id)) + [float(player_id)],
                    dtype=torch.float32,
                )

                _, opponent_logit = net(obs)
                opponent_logit = opponent_logit.squeeze()

                # Play randomly to explore different histories
                if clue:
                    better = expected_score(state, player_id, net, depth)[0]
                    if random() < 0.8:
                        net_action = better
                    else:
                        net_action = 1 - better
                else:
                    net_action = randrange(2)

                if player_id == 0:
                    state.apply_actions([net_action, bot_action])
                else:
                    state.apply_actions([bot_action, net_action])

                # 1 means opponent cooperated; 0 means defected
                target = torch.tensor(
                    float(bot_action == 0),
                    dtype=torch.float32,
                )

                loss = torch.nn.functional.binary_cross_entropy_with_logits(
                    opponent_logit, target
                )
                if evaluate:
                    p = torch.sigmoid(opponent_logit).item()
                    y = target.item()

                    result = (loss.item(), (p - y)**2, p, y)

                    key = (bot.name, player_id)
                    reports.setdefault(key, []).append(result)

                    if state.move_number() >= args.recall:
                        key = (bot.name, player_id, "late")
                        reports.setdefault(key, []).append(result)
                else:
                    losses.append(loss)

    if evaluate:
        for bot in bots:
            for player_id in range(2):
                for phase in ("all", "late"):
                    key = (bot.name, player_id)
                    if phase == "late":
                        key += ("late",)

                    results = reports.get(key, [])
                    if not results:
                        continue

                    n = len(results)
                    log_loss = sum(r[0] for r in results) / n
                    brier = sum(r[1] for r in results) / n
                    mean_p = sum(r[2] for r in results) / n
                    actual_p = sum(r[3] for r in results) / n

                    print(
                        f"{bot.name:15} player {player_id} {phase:4}: "
                        f"log loss {log_loss:.4f}, "
                        f"Brier {brier:.4f}, "
                        f"P(C) {mean_p:.3f}, "
                        f"actual {actual_p:.3f}"
                    )
    else:
        optimizer.zero_grad()
        loss = torch.stack(losses).mean()
        loss.backward()
        optimizer.step()

def expected_score(state, player_id, net, depth):
    """Expected score using optimal play.
       Returns (best_action, score)."""

    key = (player_id, tuple(state.observation_tensor(player_id)), depth)

    if key in expected_score_cache:
        return expected_score_cache[key]

    scores = []
    obs = torch.tensor(
        list(state.observation_tensor(player_id)) + [float(player_id)],
        dtype=torch.float32,
    )

    with torch.no_grad():
        _, opponent_logit = net(obs)
        p = torch.sigmoid(opponent_logit).item()

    probs = (p, 1.0 - p)

    for my_action in (0, 1):
        score = 0.0

        for bot_action in (0, 1):
            if probs[bot_action] == 0: # never happens?
                continue
            if player_id == 0:
                joint_action = (my_action, bot_action)
            else:
                joint_action = (bot_action, my_action)
            state0 = state.clone()
            state0.apply_actions(joint_action)
            immediate = state0.rewards()[player_id]

            if depth == 0:
                future = 0.0
            else:
                future = expected_score(state0, player_id, net,  depth - 1)[1]

            score += probs[bot_action] * (immediate + future)

        scores.append(score)

    if scores[0] > scores[1]:
        expected_score_cache[key] = 0, scores[0]
    else:
        expected_score_cache[key] = 1, scores[1]
    return expected_score_cache[key]

def play_match(game, bot, net):
    """Have brute play game against a bot.
       print the average score."""
    # for now brute is always player 0
    player_id = 0
    depth = 8
    score = 0
    rounds = 0
    bot.player_id = 1 - player_id
    bot.restart()
    state = game.new_initial_state()

    while not state.is_terminal():
        bot_action = bot.step(state)
        net_action = expected_score(state, player_id, net, depth)[0]
        #print('brute', brute_action, 'bot', bot_action)
        state.apply_actions([net_action, bot_action])
        score += state.rewards()[player_id]
        rounds += 1
    print("avg score vs ", bot.name, score/rounds)



if __name__ == '__main__':
    started = time()
    from argparse import ArgumentParser
    from ast import literal_eval
    parser = ArgumentParser()
    parser.add_argument('--reps', help='repetitions', type=int, default=50) # per game
    parser.add_argument('--iter', help='iterations', type=int, default=80)
    parser.add_argument('--recall', help='recall', type=int, default=6)
    parser.add_argument('--seed', help='seed', type=int, default=12345)
    parser.add_argument('--game', default='pd2')
    parser.add_argument('--bots', default=None)
    parser.add_argument('--tft', action="store_true", help="include TFTBot")
    # supported: coordination, pd, ...
    args = parser.parse_args()
    seed(args.seed)
    game_name = 'matrix_' + args.game
    print(game_name)
    game = pyspiel.create_repeated_game(game_name, {
      "num_repetitions": args.reps,"recall": args.recall,})

    if args.bots:
        bot_spec = literal_eval(args.bots)
        bots = get_bots(bot_spec)
        if args.tft:
            bots.append(TFTBot())
    else:
        if args.tft:
            bots = [TFTBot()]
        else:
           bots = [TFTBot(1), CycleBot([0]), CycleBot([1]), CycleBot([0, 1])]
           #bots = [CycleBot([1])]
           # bots = [TFTBot(), CycleBot([1])]
           #bots = [TFTBot()]

    input_size = game.observation_tensor_size() + 1 # add player id
    torch.manual_seed(args.seed)

    net = MatrixNet(input_size)
    optimizer = torch.optim.Adam(
       net.parameters(),
       lr=0.001,
    )
    for _ in range(1000):
        prediction_train(game, net, bots, optimizer)
    prediction_train(game, net, bots, optimizer, evaluate=True)
    for _ in range(1000):
        prediction_train(game, net, bots, optimizer, clue=True)
    prediction_train(game, net, bots, optimizer, evaluate=True, clue=True)
    expected_score_cache = {}
    for bot in bots:
        play_match(game, bot, net)

    print("elasped", int(time() - started))

