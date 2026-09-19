#TODO add a copyright. Assign it to deepmind if we want to push this upstream, self otherwise.

"""Bots for use in prisoner's dilemma type games."""
# Each game must have 2 players and 2 actions
import pyspiel


class TFTBot(pyspiel.Bot):
    """tit for tat"""
    def __init__(self, player_id=0, name=None):
        super().__init__()
        self.player_id = player_id
        if name is None:
            self.name = "TitForTat"
        else:
            self.name = name


    def step_with_policy(self, state):
        """Returns the stochastic policy and selected action in the given state."""
        legal_actions = state.legal_actions(state.current_player())
        history = state.history()
        if history:
            if self.player_id == 1:
                action = history[-2]
            else:
                action = history[-1]
        else:
            action = 0
        if action not in legal_actions:
            raise ValueError("Unpossible")
        policy = [(action, 1)]
        return policy, action

    def step(self, state):
        return self.step_with_policy(state)[1]

    def restart_at(self, state):
        pass

class GrimBot(pyspiel.Bot):
    """Grim Trigger (unforgiving)"""
    def __init__(self, player_id=0, name=None):
        super().__init__()
        self.player_id = player_id
        self.grim = False
        if name is None:
            self.name = "GrimTrigger"
        else:
            self.name = name


    def step_with_policy(self, state):
        """Returns the stochastic policy and selected action in the given state."""
        legal_actions = state.legal_actions(state.current_player())
        history = state.history()
        if history:
            if self.player_id == 1 and history[-2] == 1:
                self.grim = True
            if self.player_id == 0 and history[-1] == 1:
                self.grim = True
        if self.grim:
            action = 1
        else:
            action = 0
        if action not in legal_actions:
            raise ValueError("Unpossible")
        policy = [(action, 1)]
        return policy, action

    def step(self, state):
        return self.step_with_policy(state)[1]

    def restart(self):
        self.grim = False
