import torch.nn as nn


class PredictionNet(nn.Module):
    def __init__(self, players=None, actions=None, input_size=None, hidden_size=None):
        super().__init__()
        if not input_size:
            input_size = 4 * (players - 1) * actions;
        if not hidden_size:
            hidden_size = 2 * input_size


        self.body = nn.Sequential(
            nn.Linear(input_size, hidden_size),
            nn.ReLU(),
            nn.Linear(hidden_size, hidden_size),
            nn.ReLU(),
        )

        self.prediction_head = nn.Linear(hidden_size, actions * (players - 1))

    def forward(self, x):
        x = self.body(x)
        prediction = self.prediction_head(x)
        return prediction
