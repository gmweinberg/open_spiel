import torch.nn as nn


class MatrixNet(nn.Module):
    def __init__(self, input_size=16, hidden_size=32):
        super().__init__()

        self.body = nn.Sequential(
            nn.Linear(input_size, hidden_size),
            nn.ReLU(),
            nn.Linear(hidden_size, hidden_size),
            nn.ReLU(),
        )

        self.policy_head = nn.Linear(hidden_size, 2)

        self.value_head = nn.Linear(hidden_size, 1)

    def forward(self, x):
        x = self.body(x)
        policy = self.policy_head(x)
        value = self.value_head(x).squeeze(-1)
        return policy, value
