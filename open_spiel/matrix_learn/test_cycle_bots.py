import pyspiel
from open_spiel.python.bots.cycle import CycleBot

def play_cyclebots():
    bot0 = CycleBot([1,1,0,0])
    bot1 = CycleBot([1])
    game = pyspiel.create_repeated_game("matrix_coordination", {
        "num_repetitions": 20,"recall": 4,})
    state = game.new_initial_state()
    count = 0
    while not state.is_terminal():
        action0 = bot0.step(state)
        action1 = bot1.step(state)
        print('bot0 action', action0, 'bot1 action', action1)
        state.apply_actions([action0, action1])


if __name__ == '__main__':
    play_cyclebots()
