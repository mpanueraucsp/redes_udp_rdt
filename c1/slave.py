# Commented out IPython magic to ensure Python compatibility.
import sys
import os
import io
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import DataLoader, TensorDataset, random_split

import torch.optim as optim
import torch.distributions as dist

import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
from sklearn.metrics import confusion_matrix, ConfusionMatrixDisplay, classification_report
import numpy as np

import pandas as pd
import math

import torch.distributions as dist

import time
import glob
import UDPBind
import globals as args

X=None
y=None

# Generate synthetic heteroscedastic multiclass data
torch.manual_seed(args.SEED)
X = torch.randn(args.NUM_SAMPLES, args.INPUT_DIM)  # 1000 samples, 100 inputs

active_indices = torch.randint(0, args.NUM_SAMPLES, (args.NUM_SAMPLES,))
y = torch.nn.functional.one_hot(active_indices, num_classes=args.NUM_SAMPLES).float()

#####################################################
#####################################################
# CONECTARSE AL SERVER
print("\nCONECTANDO\n")
cliente = UDPBind.SlaveClient()
# configurar anomalias de este cliente, (chekear ACK.h para ver paramtros)
cliente.setConfigTests(True, 2, 1, 1, # corrupt checksum
                       True, 2, 1, 1, # paquet loss
                       True, 2, 1, 1) # ack loss timeout
cliente.start(args.UDP_IP, args.UDP_PORT)

# ESPERAR LOTE DE BATCHS CSV CORRESPONDIENTE
print("\nESPERANDO LOTE DEL MAESTRO\n")
csv = cliente.receiveCSV(99)

# CARGAR LOTE
print("\nCARGANDO LOTE\n")
df = pd.read_csv(io.StringIO(csv), header=None)

#####################################################
#####################################################

# Assume first 14 columns are input features, last 3 are one-hot class labels
X_np =        df.iloc[:, :args.INPUT_DIM].values.astype(np.float32)
y_onehot_np = df.iloc[:, -args.NUM_CLASSES:].values.astype(np.float32)

X = torch.tensor(X_np)
y = torch.tensor(y_onehot_np)
print("y ",y[0:10])
print("y ",y.size())
print("X ",X.size())
# Create dataset and split into training/testing
dataset = TensorDataset(X, y)

# train_size = int(0.8 * len(dataset))
train_size = int(len(dataset))
test_size = len(dataset) - train_size
train_dataset, test_dataset = random_split(dataset, [train_size, test_size])

train_loader = DataLoader(train_dataset, batch_size=args.BATCH_SIZE, shuffle=True)
test_loader = DataLoader(test_dataset, batch_size=args.BATCH_SIZE, shuffle=False)

# Model setup
model = args.MulticlassClassifier(input_dim=args.INPUT_DIM, num_classes=args.NUM_CLASSES)

criterion = nn.CrossEntropyLoss()
optimizer = torch.optim.Adam(model.parameters(), lr=args.LEARNING_RATE)

# Training loop
train_tracker, test_tracker, accuracy_tracker = [], [], []

# Evaluation on test set
model.eval()
y_true = []
y_pred = []
log_vars_all = []

time.sleep(1) # 1 segundo

for epoch in range(args.NUM_EPOCHS):
    model.train()
    epoch_loss = 0
    for batchIdx, (batch_x, batch_y) in enumerate(train_loader):
        optimizer.zero_grad()
        
        #####################################################
        #####################################################
        with torch.no_grad():            
            capas = model.getCapas()
            
            # RECIBIR CAPAS
            if args.PRINT_PAQUETS : print(f"\n=======> EMPEZANDO A RECIBIR CAPAS Y BIAS DE MAESTRO\n") 
            for capaIdx, capa in enumerate(capas):
                # RECIBIR 
                # pesos
                if args.PRINT_PAQUETS : print(f"\n=======> ESPERANDO CAPA DE MAESTRO: capa {capaIdx+1}/{len(capas)}\n")                 
                new_weights_matrix = cliente.receiveLayer(capaIdx)
                
                # INSERTAR
                # pesos
                if args.PRINT_PAQUETS : print("\n=======> INSERTANDO CAPAS\n") 
                capa.weight.data.copy_(torch.from_numpy(np.asarray(new_weights_matrix)))
                
                # RECIBIR 
                # bias
                if args.PRINT_PAQUETS : print(f"\n=======> ESPERANDO BIAS DE MAESTRO: capa {capaIdx+1}/{len(capas)}\n")                 
                new_bias_matrix = cliente.receiveLayer(capaIdx + 50)
                
                # INSERTAR
                # bias
                if args.PRINT_PAQUETS : print("\n=======> INSERTANDO BIAS\n") 
                capa.bias.data.copy_(torch.from_numpy(np.asarray(new_bias_matrix)).view(-1))
        #####################################################
        #####################################################
            
        # ENTRENAR
        if args.PRINT_PAQUETS : print(f"\n=======> ENTRENANDO: batch {batchIdx+1}/{len(train_loader)}\n")           
        logits, log_vars = model(batch_x)
        loss = criterion(logits, batch_y)
        loss.backward()
        optimizer.step()

        #####################################################
        #####################################################
        with torch.no_grad():
            # ENVIAR CAPAS
            if args.PRINT_PAQUETS : print(f"\n=======> EMPEZANDO A ENVIAR CAPAS A MAESTRO\n")  
            for capaIdx, capa in enumerate(capas):
                # EXTRAER
                original_weights_matrix = np.matrix(capa.weight.data.cpu().numpy(), dtype=np.float32)
                original_bias_matrix = np.matrix(capa.bias.data.cpu().numpy(), dtype=np.float32)

                # ENVIAR
                # pesos
                if args.PRINT_PAQUETS : print(f"\n=======> ENVIANDO CAPA A MAESTRO: capa {capaIdx+1}/{len(capas)}\n")  
                cliente.sendLayer(original_weights_matrix, capaIdx)
                
                if args.PRINT_PAQUETS : time.sleep(0.001)
                
                # bias
                if args.PRINT_PAQUETS : print(f"\n=======> ENVIANDO BIAS A MAESTRO: capa {capaIdx+1}/{len(capas)}\n")  
                cliente.sendLayer(original_bias_matrix, capaIdx + 50)
                
                if args.PRINT_PAQUETS : time.sleep(0.001)
                
        #####################################################
        #####################################################
        
        epoch_loss += loss.item()
        
    train_tracker.append(epoch_loss / len(train_loader))
    # print(f"Epoch {epoch+1}/{args.NUM_EPOCHS}, Loss: {train_tracker[-1]:.4f} | ",end="")
    print(f"Epoch {epoch+1}/{args.NUM_EPOCHS}, Loss: {train_tracker[-1]:.4f} | ")

# Print classification metrics
print("\nClassification Report:")
