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


def pretrain(game, net, bots, optimizer):
    losses = []
    for bot in bots:
        sucker = False
        if bot.name.find("Cycle") == 0:
            sucker = True

        for player_id in range(2):
            bot.player_id = 1 - player_id
            bot.restart()
            state = game.new_initial_state()
            if sucker:
                state.set_extra_observation_tensor([1.0])

            while not state.is_terminal():
                bot_action = bot.step(state)
                obs = torch.tensor(
                    state.observation_tensor(player_id),
                    dtype=torch.float32,
                )
                target = torch.tensor(1 if sucker else 0)

                logits, value = net(obs)
                net_action = randrange(2)
                if player_id == 0:
                    state.apply_actions([net_action, bot_action])
                else:
                    state.apply_actions([bot_action, net_action])

                loss = torch.nn.functional.cross_entropy(
                    logits.unsqueeze(0),
                    target.unsqueeze(0),
                )
                losses.append(loss)
    optimizer.zero_grad()
    loss = torch.stack(losses).mean()
    loss.backward()
    optimizer.step()

def train_iteration(game, net, bots, optimizer, explore=0):

    samples = []
    losses = []
    score = 0
    all_rounds = 0
    batch_size = 10
    for abatch in range(batch_size):
        for bot in bots:
            sucker = False
            if bot.name.find("Cycle") == 0:
                sucker = True

            for player_id in range(2):
                bot.player_id = 1 - player_id
                bot.restart()
                state = game.new_initial_state()
                inner_rounds = 0
                log_probs = []
                values = []
                rewards = []
                entropies = []
                actions = []
                if sucker:
                    state.set_extra_observation_tensor(values=[1.0])
                print("agent_id", player_id, "bot", bot.name)
                while not state.is_terminal():
                    bot_action = bot.step(state)
                    obs = torch.tensor(
                      state.observation_tensor(player_id),
                      dtype=torch.float32,
                    )


                    logits, value = net(obs)
                    temperature = 3.0 if inner_rounds < explore else 1.0
                    dist = torch.distributions.Categorical(logits=logits/temperature)
                    action = dist.sample()
                    actions.append(action.item())

                    log_probs.append(dist.log_prob(action))
                    values.append(value)
                    entropies.append(dist.entropy())
                    
                    # play round
                    net_action = action.item()

                    if player_id == 0:
                        state.apply_actions([net_action, bot_action])
                    else:
                        state.apply_actions([bot_action, net_action])
                    print(
                       "my action:", net_action,
                       "bot action:", bot_action,
                       "P(cooperate):", dist.probs[0].item(),
                       "reward:", state.rewards()[player_id],
                    )
                    rewards.append(state.rewards()[player_id])
                    all_rounds += 1
                    inner_rounds += 1


                returns = []
                G = 0.0
                gamma = 1.0

                for reward in reversed(rewards):
                     G = reward + gamma * G
                     returns.append(G)
                returns.reverse()
                advantages = torch.stack([
                    (return_to_go - value).detach()
                    for value, return_to_go in zip(values, returns)
                ])

                for log_prob, value, return_to_go, entropy, advantage in zip(
                    log_probs, values, returns, entropies, advantages
                ):
                    policy_loss = -log_prob * advantage
                    value_loss = (value - return_to_go) ** 2

                    loss = policy_loss + 0.01 * value_loss - 0.05 * entropy
                    losses.append(loss)

                score += state.returns()[player_id]
    optimizer.zero_grad()
    loss = torch.stack(losses).mean()
    loss.backward()
    optimizer.step()

    print("average score", score / all_rounds)
    print(actions)

def train_old(game, net, bots, optimizer):

    samples = []
    losses = []
    player_id = 0
    score = 0

    for bot in bots:
        state = game.new_initial_state()
        bot.restart()
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
            actions.append(action.item())

            reward = state.rewards()[player_id]
            advantage = (reward - value).detach()
            policy_loss = -log_prob * advantage
            value_loss = (value - reward) ** 2
            loss = policy_loss + value_loss
            losses.append(loss)
            rounds += 1
        score += state.returns()[player_id]
    optimizer.zero_grad()
    loss = torch.stack(losses).mean()
    loss.backward()
    optimizer.step()
    print("average score", score / (len(bots) * rounds))


if __name__ == '__main__':
    from argparse import ArgumentParser
    from ast import literal_eval
    parser = ArgumentParser()
    parser.add_argument('--reps', help='repetitions', type=int, default=50)
    parser.add_argument('--iter', help='iterations', type=int, default=80)
    parser.add_argument('--recall', help='recall', type=int, default=4)
    parser.add_argument('--seed', help='seed', type=int, default=12345)
    parser.add_argument('--game', default='pd2')
    parser.add_argument('--bots', default=None)
    parser.add_argument('--tft', action="store_true", help="include TFTBot")
    # supported: coordination, pd, ...
    args = parser.parse_args()
    game_name = 'matrix_' + args.game
    print(game_name)
    game = pyspiel.create_repeated_game(game_name, {
      "num_repetitions": args.reps,"recall": args.recall,})
    extra_game = pyspiel.create_extra_info_game(game,
          {"extra_observation_tensor_size":1})

    #bot_spec = [([1,1,0,0], 2), ([0], 4), ([1,0], 1)] # unbalanced
    # bot_spec = [([1,1,0,0], 2), ([0], 8), ([1], 1), ([1,0], 1)] # less unbalanced
    # bot_spec = [([1,1,0,0], 1)]
    #bot_spec = [([1], 1), ([0], 1)]
    #bot_spec = [([1,1,1,1,0,0,0,0], 1)] # one long cycle
    #bot_spec = [([1,1,1,1,0,0,0,0], 1), ([0], 1), ([1], 1)] # unlearnable
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

    input_size = extra_game.observation_tensor_size()
    torch.manual_seed(args.seed)

    net = MatrixNet(input_size)
    optimizer = torch.optim.Adam(
       net.parameters(),
       lr=0.001,
    )
    for _ in range(5000):
        pretrain(extra_game, net, bots, optimizer)

    for iteration in range(args.iter):
        if iteration < 8:
            train_iteration(extra_game, net, bots, optimizer) #, explore=8
        else:
            train_iteration(extra_game, net, bots, optimizer)
        #train_old(game, net, bots, optimizer)

#
