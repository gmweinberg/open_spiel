# Copyright 2026 DeepMind Technologies Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Tests for the extra_info game transformation."""

from absl.testing import absltest
from absl.testing import parameterized
import numpy as np

import pyspiel


class GamesExtraInfoTest(parameterized.TestCase):
    def test_binfings(self):
        game_name = 'matrix_pd2'
        game = pyspiel.load_game(game_name)
        state = game.new_initial_state()
        print('len', len(state.observation_tensor(0)))
        extra_game = pyspiel.load_game("extra_info", {"game":matrix_pd2(), 
                "extra_observation_tensor_size":1})
        state = extra_game.new_initial_state()
        print('len', len(state.observation_tensor(0)))

if __name__ == "__main__":
  absltest.main()
                  




