import torch
import torch.nn as nn
from torch.utils.data import DataLoader, TensorDataset, random_split
import numpy as np
import pandas as pd
import math
import time
import UDPBind
import globals as args

# conectar
servidor = UDPBind.MasterServer()
servidor.start(args.UDP_PORT)

while servidor.getClientsConectedSize() < args.NUM_ESCLAVOS:
    time.sleep(args.TIME_WAIT)
    print(f"[Maestro] Esperando {servidor.getClientsConectedSize()}/{args.NUM_ESCLAVOS}")

print("[Maestro] Slaves conectados")

# cargar csv
df = pd.read_csv(args.CSV_PATH, header=None, skiprows=1)

X_np = df.iloc[:, :args.INPUT_DIM].values.astype(np.float32)
y_np = df.iloc[:, -args.NUM_CLASSES:].values.astype(np.float32)

X = torch.tensor(X_np)
y = torch.tensor(y_np)

dataset = TensorDataset(X, y)

train_size = int(0.8 * len(dataset))
test_size = len(dataset) - train_size

train_dataset, test_dataset = random_split(dataset, [train_size, test_size])

df_train = df.iloc[train_dataset.indices]

# distribucion dataset
loteSize = len(df_train) // (args.NUM_ESCLAVOS + 1)

for i in range(args.NUM_ESCLAVOS):
    begin = loteSize * i
    end = loteSize * (i + 1)

    df_slave = df_train.iloc[begin:end]
    csv_data = df_slave.to_csv(index=False, header=False)

    servidor.sendCSV(csv_data, 99, i)

df_master = df_train.iloc[loteSize * args.NUM_ESCLAVOS:]

X_master = torch.tensor(df_master.iloc[:, :args.INPUT_DIM].values.astype(np.float32))
y_master = torch.tensor(df_master.iloc[:, -args.NUM_CLASSES:].values.astype(np.float32))

train_dataset = TensorDataset(X_master, y_master)

train_loader = DataLoader(train_dataset, batch_size=args.BATCH_SIZE, shuffle=True)
test_loader = DataLoader(test_dataset, batch_size=args.BATCH_SIZE, shuffle=False)

# modelo
model = args.MulticlassClassifier(
    input_dim=args.INPUT_DIM,
    num_classes=args.NUM_CLASSES
)

criterion = nn.CrossEntropyLoss()
optimizer = torch.optim.Adam(model.parameters(), lr=args.LEARNING_RATE)

# training
for epoch in range(args.NUM_EPOCHS):

    model.train()
    epoch_loss = 0

    for batchIdx, (batch_x, batch_y) in enumerate(train_loader):

        optimizer.zero_grad()
        logits, log_vars = model(batch_x)
        loss = criterion(logits, batch_y)

        loss.backward()
        optimizer.step()

        epoch_loss += loss.item()

        with torch.no_grad():

            capas = model.getCapas()

            for capaIdx, capa in enumerate(capas):

                weights = capa.weight.data.cpu().numpy()
                bias = capa.bias.data.cpu().numpy()

                servidor.sendLayer(weights, capaIdx)
                servidor.sendLayer(bias, capaIdx + 50)

            for capaIdx, capa in enumerate(capas):

                avg_w = servidor.receiveAvgLayer(capaIdx, args.NUM_ESCLAVOS)
                avg_b = servidor.receiveAvgLayer(capaIdx + 50, args.NUM_ESCLAVOS)

                capa.weight.data.copy_(torch.tensor(avg_w))
                capa.bias.data.copy_(torch.tensor(avg_b))

        print(f"Epoch {epoch} | Batch {batchIdx} | Loss {loss.item():.4f}")

    print("Epoch loss:", epoch_loss / len(train_loader))

# test
model.eval()

correct = 0
total = 0

with torch.no_grad():
    for x, y in test_loader:
        logits, _ = model(x)

        preds = torch.argmax(logits, dim=1)
        labels = torch.argmax(y, dim=1)

        correct += (preds == labels).sum().item()
        total += y.size(0)

print("Accuracy:", correct / total)