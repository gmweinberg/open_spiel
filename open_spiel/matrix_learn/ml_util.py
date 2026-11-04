from pyspiel import WrappedGame, RepeatedGame, create_extra_info_game
import numpy as np

def get_cycle_bots(spec):
    """Spec is a list of tuples cycle, count."""
    bots = []
    for cycle, count in spec:
        for _ in range(count):
            bot = CycleBot(cycle)
            bots.append(bot)
    return bots

def get_inner_game(game):
    while True:
        if isinstance(game, WrappedGame):
            game = game.get_wrapped_game()
        elif isinstance(game, RepeatedGame):
            game = game.stage_game()
        else:
            return game

def normalized_utilities(game, player_id):
    inner = get_inner_game(game)

    if player_id == 0:
        utilities = inner.row_utilities()
    elif player_id == 1:
        utilities = inner.col_utilities()
    else:
        raise ValueError(f"Invalid player_id: {player_id}")

    low = utilities.min()
    high = utilities.max()

    if high == low:
        return np.zeros(utilities.size, dtype=np.float32)

    normalized = 2.0 * (utilities.ravel() - low) / (high - low) - 1.0
    return normalized.astype(np.float32)

def create_info_matrix_game(game):
    """Wrap a matrix game with space for normalized player payoffs."""
    inner = get_inner_game(game)
    return create_extra_info_game(
        game,
        {"extra_observation_tensor_size":inner.row_utilities().size}
    )

def get_extra_info_size(game):
    """Get the size of the observation tensor of the extra info game."""
    inner = get_inner_game(game)
    return game.observation_tensor_size() + inner.row_utilities().size



def set_utilities_tensor(state, player_id):
    """Set the extra observation tensor to normalized player payoffs."""
    game = state.get_game()
    normalized = normalized_utilities(game, player_id)
    state.set_extra_observation_tensor(values=normalized)

