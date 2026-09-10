from random import shuffle
import torch
import pyspiel
from open_spiel.python.bots.cycle import CycleBot
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


def train_iteration(game, net, bots, optimizer):

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
    optimizer.zero_grad()
    loss = torch.stack(losses).mean()
    loss.backward()
    optimizer.step()
    #
    # append all (observation, action, reward, ...) samples

    #shuffle(samples)
    # train network on minibatches from samples
    print("average score", score / (len(bots) * rounds))


if __name__ == '__main__':
    num_repetitions = 50
    game = pyspiel.create_repeated_game("matrix_coordination", {
      "num_repetitions": num_repetitions,"recall": 4,})

    #bot_spec = [([1,1,0,0], 2), ([0], 4), ([1,0], 1)] # unbalanced
    bot_spec = [([1,1,0,0], 2), ([0], 8), ([1], 2), ([1,0], 1)] # less unbalanced
    # bot_spec = [([1,1,0,0], 1)]
    #bot_spec = [([1], 1), ([0], 1)]
    bots = get_bots(bot_spec)
    input_size = game.observation_tensor_size()
    net = MatrixNet(input_size)
    optimizer = torch.optim.Adam(
       net.parameters(),
       lr=0.01,
    )
    for iteration in range(40):
        train_iteration(game, net, bots, optimizer)
