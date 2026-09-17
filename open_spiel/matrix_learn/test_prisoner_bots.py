import pyspiel
from open_spiel.python.bots.cycle import CycleBot
from open_spiel.python.bots.prisoner import TFTBot, GrimBot

def play_bots():
    bot0 = CycleBot([0, 1, 1])
    bot1 = GrimBot(1)
    game = pyspiel.create_repeated_game("matrix_pd", {
        "num_repetitions": 9,"recall": 4,})
    state = game.new_initial_state()
    count = 0
    while not state.is_terminal():
        action0 = bot0.step(state)
        action1 = bot1.step(state)
        print('bot0 action', action0, 'bot1 action', action1)
        state.apply_actions([action0, action1])
    print(state.history())


if __name__ == '__main__':
    play_bots()
