#TODO add a copyright. Assign it to deepmind if we want to push this upstream, self otherwise.

"""A stupid bot which cycles through a list of actions in a predetermined order."""
# I created this bot for playing matrix games. It can be used for other games but
# it will likely fail except for those games where all actions are always legal.
import pyspiel

class CycleBot(pyspiel.Bot):
    def __init__(self, cycle=None, name=None):
        super().__init__()
        if cycle is None:
            self.cycle = [0]
        else:
            self.cycle = list(cycle)
        self.offset = 0
        if name is None:
            self.name = "CycleBot" + ''.join(str(elm) for elm in self.cycle)
        else:
            self.name = name


    def step_with_policy(self, state):
        """Returns the stochastic policy and selected action in the given state."""
        legal_actions = state.legal_actions(state.current_player())
        action = self.cycle[self.offset]
        if action not in legal_actions:
            raise ValueError("Unpossible")
        self.offset = (self.offset + 1) % len(self.cycle)
        policy = [(action, 1)]
        return policy, action

    def step(self, state):
        return self.step_with_policy(state)[1]

    def restart_at(self, state):
        self.offset = state.move_number() % len(self.cycle)

    def restart(self):
        self.offset = 0




