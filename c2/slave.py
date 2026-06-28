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

# setear la semilla aleatoria global para reproducibilidad
torch.manual_seed(args.SEED)
X = torch.randn(args.NUM_SAMPLES, args.INPUT_DIM)  # 1000 samples, 100 inputs

active_indices = torch.randint(0, args.NUM_SAMPLES, (args.NUM_SAMPLES,))
y = torch.nn.functional.one_hot(active_indices, num_classes=args.NUM_SAMPLES).float()


# CONECTARSE AL SERVER MAESTRO

print("\nCONECTANDO\n")
cliente = UDPBind.SlaveClient()
cliente.start(args.UDP_IP, args.UDP_PORT)

# ESPERAR LOTE DE BATCHS CSV CORRESPONDIENTE
print("\nESPERANDO LOTE DEL MAESTRO\n")
csv = cliente.receiveCSV(99)

# CARGAR LOTE RECIBIDO
print("\nCARGANDO LOTE\n")
df = pd.read_csv(io.StringIO(csv), header=None)


# separacion del dataset: primeras 14 columnas son características, últimas 3 son las clases (diabetes)
X_np =        df.iloc[:, :args.INPUT_DIM].values.astype(np.float32)
y_onehot_np = df.iloc[:, -args.NUM_CLASSES:].values.astype(np.float32)

# convertimos los datos empaquetados de numpy a tensores de pytorch
X = torch.tensor(X_np)
y = torch.tensor(y_onehot_np)

# imprimimos informacion en consola para verificar las dimensiones de los datos cargados
print("y ",y[0:10])
print("y ",y.size())
print("X ",X.size())

# creamos el dataset y preparamos el cargador de lotes (DataLoader)
dataset = TensorDataset(X, y)

# configuramos el tamaño del entrenamiento (100% de los datos asignados a este lote)
# train_size = int(0.8 * len(dataset))
train_size = int(len(dataset))
test_size = len(dataset) - train_size
train_dataset, test_dataset = random_split(dataset, [train_size, test_size])

# creamos los loaders para alimentar la red neuronal de 50 en 50 
train_loader = DataLoader(train_dataset, batch_size=args.BATCH_SIZE, shuffle=True)
test_loader = DataLoader(test_dataset, batch_size=args.BATCH_SIZE, shuffle=False)

# configuracion del Modelo de red neuronal
model = args.MulticlassClassifier(input_dim=args.INPUT_DIM, num_classes=args.NUM_CLASSES)

# funcion de perdida (error) y el optimizador Adam
criterion = nn.CrossEntropyLoss()
optimizer = torch.optim.Adam(model.parameters(), lr=args.LEARNING_RATE)

# listas de seguimiento para el entrenamiento
train_tracker, test_tracker, accuracy_tracker = [], [], []

# inicialmente el modelo en modo de evaluacion para limpiar variables internas
model.eval()
y_true = []
y_pred = []
log_vars_all = []

time.sleep(1) # 1 segundo

# bucle de entrenamiento principal
for epoch in range(args.NUM_EPOCHS):
    model.train()
    epoch_loss = 0

    # iteramos por cada lote de 50 filas (batch) que tiene nuestro fragmento de datos
    for batchIdx, (batch_x, batch_y) in enumerate(train_loader):
        optimizer.zero_grad()
        
        with torch.no_grad():            
            capas = model.getCapas()
            
            # RECIBIR CAPAS
            if args.PRINT_PAQUETS : print(f"\n=======> EMPEZANDO A RECIBIR CAPAS Y BIAS DE MAESTRO\n") 
            for capaIdx, capa in enumerate(capas):
                 
                # RECIBE matriz de pesos
                if args.PRINT_PAQUETS : print(f"\n=======> ESPERANDO CAPA DE MAESTRO: capa {capaIdx+1}/{len(capas)}\n")                 
                new_weights_matrix = cliente.receiveLayer(capaIdx)
                
                # INSERTA matriz de pesos
                if args.PRINT_PAQUETS : print("\n=======> INSERTANDO CAPAS\n") 
                capa.weight.data.copy_(torch.from_numpy(np.asarray(new_weights_matrix)))
                
                # RECIBE vector de sesgo (bias)
                if args.PRINT_PAQUETS : print(f"\n=======> ESPERANDO BIAS DE MAESTRO: capa {capaIdx+1}/{len(capas)}\n")                 
                new_bias_matrix = cliente.receiveLayer(capaIdx + 50)
                
                # INSERTA vector de sesgo (bias)
                if args.PRINT_PAQUETS : print("\n=======> INSERTANDO BIAS\n") 
                capa.bias.data.copy_(torch.from_numpy(np.asarray(new_bias_matrix)).view(-1))
        
            
        # ENTRENA el lote localmente
        if args.PRINT_PAQUETS : print(f"\n=======> ENTRENANDO: batch {batchIdx+1}/{len(train_loader)}\n")           
        logits, log_vars = model(batch_x)
        loss = criterion(logits, batch_y)
        loss.backward()
        optimizer.step()

        with torch.no_grad():
            # ENVIAR CAPAS modificadas de vuelta al maestro
            if args.PRINT_PAQUETS : print(f"\n=======> EMPEZANDO A ENVIAR CAPAS A MAESTRO\n")  
            for capaIdx, capa in enumerate(capas):
                # EXTRAER pesos y bias despues del entrenamiento
                original_weights_matrix = np.matrix(capa.weight.data.cpu().numpy(), dtype=np.float32)
                original_bias_matrix = np.matrix(capa.bias.data.cpu().numpy(), dtype=np.float32)

                # ENVIAR pesos corregidos
                if args.PRINT_PAQUETS : print(f"\n=======> ENVIANDO CAPA A MAESTRO: capa {capaIdx+1}/{len(capas)}\n")  
                cliente.sendLayer(original_weights_matrix, capaIdx)
                
                if args.PRINT_PAQUETS : time.sleep(0.001)
                
                # ENVIAR bias corregido
                if args.PRINT_PAQUETS : print(f"\n=======> ENVIANDO BIAS A MAESTRO: capa {capaIdx+1}/{len(capas)}\n")  
                cliente.sendLayer(original_bias_matrix, capaIdx + 50)
                
                if args.PRINT_PAQUETS : time.sleep(0.001)
        

        epoch_loss += loss.item() # acumulamos el error de este lote

    # calculamos y registramos el error medio ponderado de la epoca   
    train_tracker.append(epoch_loss / len(train_loader))
    # print(f"Epoch {epoch+1}/{args.NUM_EPOCHS}, Loss: {train_tracker[-1]:.4f} | ",end="")
    print(f"Epoch {epoch+1}/{args.NUM_EPOCHS}, Loss: {train_tracker[-1]:.4f} | ")

# Print classification metrics
print("\nClassification Report:")
