from random import shuffle, randrange
import torch
import pyspiel
from open_spiel.python.bots.cycle import CycleBot
from open_spiel.python.bots.prisoner import TFTBot, GrimBot
from matrix_net import MatrixNet

# DEVICE = torch.device("cuda" if torch.cuda.is_available() else "cpu")

def get_bots(spec):
    """Spec is a list of tuples cycle, count."""
    bots = []
    for cycle, count in spec:
        for _ in range(count):
            bot = CycleBot(cycle)
            bots.append(bot)
    return bots


def train_iteration(game, net, bots, optimizer, explore=0):

    samples = []
    losses = []
    score = 0

    for bot in bots:
        for player_id in range(1):
            bot.player_id = 1 - player_id
            bot.restart()
            state = game.new_initial_state()
            rounds = 0
            log_probs = []
            values = []
            rewards = []
            entropies = []
            while not state.is_terminal():
                bot_action = bot.step(state)
                obs = torch.tensor(
                  state.observation_tensor(player_id),
                  dtype=torch.float32,
                )

                logits, value = net(obs)

                dist = torch.distributions.Categorical(logits=logits)
                action = dist.sample()

                log_probs.append(dist.log_prob(action))
                values.append(value)
                entropies.append(dist.entropy())

                # play round
                if player_id == 0:
                    state.apply_actions([action.item(), bot_action])
                else:
                    state.apply_actions([bot_action, action.item()])
                rewards.append(state.rewards()[player_id])
                rounds += 1


            returns = []
            G = 0.0
            gamma = 1.0

            for reward in reversed(rewards):
                 G = reward + gamma * G
                 returns.append(G)
            returns.reverse()
            for log_prob, value, return_to_go, entropy in zip(
                log_probs, values, returns, entropies
            ):
                advantage = (return_to_go - value).detach()

                policy_loss = -log_prob * advantage
                value_loss = (value - return_to_go) ** 2

                loss = (policy_loss + 0.01 * value_loss - 0.05 * entropy)

                losses.append(loss)

            score += state.returns()[player_id]
    optimizer.zero_grad()
    loss = torch.stack(losses).mean()
    loss.backward()
    optimizer.step()

    print("average score", score / (2 * len(bots) * rounds))

def train_old(game, net, bots, optimizer):

    samples = []
    losses = []
    player_id = 0
    score = 0

    for bot in bots:
        state = game.new_initial_state()
        rounds = 0
        while not state.is_terminal():
            bot_action = bot.step(state)
            obs = torch.tensor(
              state.observation_tensor(player_id),
              dtype=torch.float32,
            )

            logits, value = net(obs)

            dist = torch.distributions.Categorical(logits=logits)
            action = dist.sample()
            log_prob = dist.log_prob(action)

            # play round
            state.apply_actions([action.item(), bot_action])

            reward = state.rewards()[player_id]
            advantage = reward - value
            policy_loss = -log_prob * advantage
            value_loss = (value - reward) ** 2
            loss = policy_loss + value_loss
            losses.append(loss)
            rounds += 1
        score += state.returns()[player_id]
    print("average score", score / (len(bots) * rounds))



if __name__ == '__main__':
    from argparse import ArgumentParser
    from ast import literal_eval
    parser = ArgumentParser()
    parser.add_argument('--reps', help='repetitions', type=int, default=50)
    parser.add_argument('--iter', help='iterations', type=int, default=80)
    parser.add_argument('--recall', help='recall', type=int, default=4)
    parser.add_argument('--game', default='coordination')
    parser.add_argument('--bots', default=None)
    # supported: coordination, pd, ...
    args = parser.parse_args()
    game_name = 'matrix_' + args.game
    game = pyspiel.create_repeated_game(game_name, {
      "num_repetitions": args.reps,"recall": args.recall,})

    #bot_spec = [([1,1,0,0], 2), ([0], 4), ([1,0], 1)] # unbalanced
    # bot_spec = [([1,1,0,0], 2), ([0], 8), ([1], 1), ([1,0], 1)] # less unbalanced
    # bot_spec = [([1,1,0,0], 1)]
    #bot_spec = [([1], 1), ([0], 1)]
    #bot_spec = [([1,1,1,1,0,0,0,0], 1)] # one long cycle
    #bot_spec = [([1,1,1,1,0,0,0,0], 1), ([0], 1), ([1], 1)] # unlearnable
    if args.bots:
        bot_spec = literal_eval(args.bots)
        bots = get_bots(bot_spec)
    else:
       bots = [TFTBot(1), CycleBot([0])]

    input_size = game.observation_tensor_size()
    net = MatrixNet(input_size)
    optimizer = torch.optim.Adam(
       net.parameters(),
       lr=0.001,
    )
    for iteration in range(args.iter):
        #train_iteration(game, net, bots, optimizer, explore=8)
        train_old(game, net, bots, optimizer)
