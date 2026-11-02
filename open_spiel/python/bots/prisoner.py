#TODO add a copyright. Assign it to deepmind if we want to push this upstream, self otherwise.

"""Bots for use in prisoner's dilemma type games."""
# Each game must have 2 players and 2 actions
import pyspiel


class TFTBot(pyspiel.Bot):
    """tit for tat"""
    def __init__(self, player_id=0, name=None, reverse=False):
        super().__init__()
        if player_id is None:
            raise ValueError()
        self.player_id = player_id
        if name is None:
            self.name = "TitForTat"
        else:
            self.name = name
        self.reverse = reverse


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
            if self.reverse:
                action = 1
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
    def __init__(self, player_id=0, name=None, reverse=False):
        super().__init__()
        if player_id is None:
            raise ValueError()
        self.player_id = player_id
        if name is None:
            self.name = "GrimTrigger"
        else:
            self.name = name
        self.reverse = reverse


    def step_with_policy(self, state):
        """Returns the stochastic policy and selected action in the given state."""
        legal_actions = state.legal_actions(state.current_player())
        history = state.history()
        if reverse:
            defect = 0
        else:
            defect = 1
        if history:
            if defect in history:
                action = defect
            else:
                action = 1 - defect
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

class SwitchBot(pyspiel.Bot):
    """Changes actions based on whether or not previous actions were the same."""
    def __init__(self, player_id=0, name=None, initial=0, same=False):
        super().__init__()
        if name is None:
            if same:
               self.name = "SwitchSame"
            else:
               self.name = "SwitchDifferent"
        else:
            self.name = name
        self.initial = initial
        self.same = same # switch if actions were the same

    def step_with_policy(self, state):
        history = state.history()
        if history:
            if self.player_id == 0:
                my_last = history[-2]
            else:
                my_last = history[-1]
            if self.same:
                if history[-2] == history[-1]:
                    action = 1 - my_last
                else:
                    action = my_last
            else:
                if history[-2] == history[-1]:
                    action = my_last
                else:
                    action = 1 - my_last
        else:
            action = self.initial
        policy = [(action, 1)]
        return policy, action

    def step(self, state):
        return self.step_with_policy(state)[1]

    def restart_at(self, state):
        pass

