import torch
import torch.nn as nn
from torch.utils.data import DataLoader, TensorDataset, random_split
import numpy as np
import pandas as pd
import math
import time
import io
import UDPBind
X=None
y=None

# Datos sinteticos
torch.manual_seed(42)
X = torch.randn(1000, 14)
active_indices = torch.randint(0, 1000, (1000,))
y = torch.nn.functional.one_hot(active_indices, num_classes=1000).float()

# Conectar
servidor = UDPBind.MasterServer()
servidor.start(45000)

# Esperar slaves
while servidor.getClientsConectedSize() < 10:
    time.sleep(2)

# Cargar CSV
df = pd.read_csv("./Dataset of Diabetes .csv", header=None, skiprows=1)

# Dividir y guardar CSVs entre el nro de slaves + maestro (los batch sobrantes se quedan en maestro)
loteSize = len(df) // (10 + 1)

# Enviar cada lote de batchs a su slave respectivo
for i in range(10):
    pass 

# Lote del maestro
begin = loteSize * 10
dfMaster = df.iloc[begin : len(df)]
maxBatchsSlaves = math.ceil(loteSize / 50)

# Preparar datos maestro
X_np = dfMaster.iloc[:, :14].values.astype(np.float32)
y_onehot_np = dfMaster.iloc[:, -3:].values.astype(np.float32)

X = torch.tensor(X_np)
y = torch.tensor(y_onehot_np)

# Crear dataset y dividirlo en training/testing
dataset = TensorDataset(X, y)

train_size = int(len(dataset))
test_size = len(dataset) - train_size
train_dataset, test_dataset = random_split(dataset, [train_size, test_size])

train_loader = DataLoader(train_dataset, batch_size=50, shuffle=True)
test_loader  = DataLoader(test_dataset,  batch_size=50, shuffle=False)

# Model setup
# model

# Training loop
train_tracker, test_tracker, accuracy_tracker = [], [], []

# Evaluation on test set
# model.eval()
y_true = []
y_pred = []
log_vars_all = []

# Entrenamiento
for epoch in range(1):
    for batchIdx, (batch_x, batch_y) in enumerate(train_loader):

        # entrenar batch

        if batchIdx < maxBatchsSlaves:
            # esperar confirmacion, enviar pesos, recibir y promediar pesos

