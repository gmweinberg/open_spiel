
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "open_spiel/algorithms/alpha_zero_torch/model.h"
#include "open_spiel/algorithms/alpha_zero_torch/vpevaluator.h"
#include "open_spiel/algorithms/mcts.h"
#include "open_spiel/spiel.h"

ABSL_FLAG(std::string, game, "crazyhouse(chess960=true)", "Game");
ABSL_FLAG(std::string, az_path, "", "AlphaZero model directory");
ABSL_FLAG(std::string, az_graph_def, "vpnet.pb", "Model definition");
ABSL_FLAG(int, az_checkpoint, -1, "Checkpoint");
ABSL_FLAG(int, max_simulations, 100, "MCTS simulations");
ABSL_FLAG(double, uct_c, 2.0, "PUCT exploration constant");

namespace az = open_spiel::algorithms::torch_az;

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  auto game = open_spiel::LoadGame(absl::GetFlag(FLAGS_game));

  az::DeviceManager device_manager;
  device_manager.AddDevice(az::VPNetModel(
      *game, absl::GetFlag(FLAGS_az_path),
      absl::GetFlag(FLAGS_az_graph_def), "/cpu:0"));

  device_manager.Get(0, 0)->LoadCheckpoint(
      absl::GetFlag(FLAGS_az_checkpoint));

  auto evaluator = std::make_shared<az::VPNetEvaluator>(
      &device_manager,
      /*batch_size=*/1,
      /*threads=*/1,
      /*cache_size=*/16384,
      /*cache_shards=*/1);

  open_spiel::algorithms::MCTSBot bot(
      *game, evaluator,
      absl::GetFlag(FLAGS_uct_c),
      absl::GetFlag(FLAGS_max_simulations),
      /*max_memory_mb=*/1024,
      /*solve=*/false,
      /*seed=*/0,
      /*verbose=*/false,
      open_spiel::algorithms::ChildSelectionPolicy::PUCT,
      /*dirichlet_alpha=*/0,
      /*dirichlet_epsilon=*/0,
      /*dont_return_chance_node=*/true);
	std::string fen;

	while (std::getline(std::cin, fen)) {
			auto state = game->NewInitialState(fen);

			if (state->IsTerminal() || state->IsChanceNode()) {
					std::cout << "{\"error\":\"invalid position\"}"
										<< std::endl;
					continue;
			}

			auto raw = evaluator->Evaluate(*state);
			auto root = bot.MCTSearch(*state);
			const auto& best = root->BestChild();

			// Return the evaluations and selected action.

    double search_value =
        best.explore_count
            ? best.total_reward / best.explore_count
            : 0.0;

    // Crazyhouse player 1 is White.
    if (state->CurrentPlayer() == 0) {
      search_value = -search_value;
    }

    std::cout
        << "{\"action\":" << best.action
        << ",\"raw_value\":" << raw[1]
        << ",\"search_value\":" << search_value
        << ",\"visits\":" << best.explore_count
        << "}" << std::endl;
  }
}

