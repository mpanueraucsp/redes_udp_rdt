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

import os
import io
import time
import glob
import UDPBind
import globals as args

X=None
y=None

# Generar datos sinteticos para el entrenamiento
torch.manual_seed(args.SEED)
X = torch.randn(args.NUM_SAMPLES, args.INPUT_DIM)  # 1000 samples, 100 inputs

active_indices = torch.randint(0, args.NUM_SAMPLES, (args.NUM_SAMPLES,))
y = torch.nn.functional.one_hot(active_indices, num_classes=args.NUM_SAMPLES).float()


# Conectar el maestro
print("\nCONECTANDO\n")
servidor = UDPBind.MasterServer()
servidor.start(args.UDP_PORT)
    
# Esperar a que todos los esclavos se conecten
while servidor.getClientsConectedSize() < args.NUM_ESCLAVOS:
    time.sleep(args.TIME_WAIT)
    print(f"\n[Maestro] Esperando {servidor.getClientsConectedSize()}/{args.NUM_ESCLAVOS} esclavos")

print(f"\n[Maestro] {servidor.getClientsConectedSize()}/{args.NUM_ESCLAVOS} ESCALVOS\n")

# Cargar el archivo CSV
print("\nCARGANDO LOTE\n")
df = pd.read_csv(args.CSV_PATH,header=None, skiprows=1)




# Obtener las features de entrada y las etiquetas
X_np =        df.iloc[:, :args.INPUT_DIM].values.astype(np.float32)
y_onehot_np = df.iloc[:, -args.NUM_CLASSES:].values.astype(np.float32)

X = torch.tensor(X_np)
y = torch.tensor(y_onehot_np)
print("y ",y[0:10])
print("y ",y.size())
print("X ",X.size())

# Crear el dataset y dividirlo en entrenamiento y prueba
dataset = TensorDataset(X, y)

# train_size = int(len(dataset))
train_size = int(0.8 * len(dataset))
test_size = len(dataset) - train_size
train_dataset, test_dataset = random_split(dataset, [train_size, test_size])


# Obtener unicamente las muestras del conjunto de entrenamiento
df_train = df.iloc[train_dataset.indices]

# Dividir el conjunto de entrenamiento entre el maestro y los esclavos
loteSize = len(df_train) // (args.NUM_ESCLAVOS + 1)

# Enviar un lote de datos a cada esclavo
print("\nDIVIDENDO LOTE\n")
for i in range(args.NUM_ESCLAVOS):
    begin = loteSize * i
    end = loteSize * (i + 1)
            
    dfSlave = df_train.iloc[begin : end]
    
    print(f"ENVIANDO LOTE a esclavo {i}")
    
    csvSlave = dfSlave.to_csv(index=False, header=False)
    servidor.sendCSV(csvSlave, 99, i)

# Obtener el lote que sera procesado por el maestro
begin = loteSize * args.NUM_ESCLAVOS
dfMaster = df_train.iloc[begin : len(df_train)]

maxBatchsSlaves = math.ceil(loteSize / args.BATCH_SIZE)
print("\nSTART TRAIN\n")

# Preparar los datos del maestro
X_np = dfMaster.iloc[:, :args.INPUT_DIM].values.astype(np.float32)
y_onehot_np = dfMaster.iloc[:, -args.NUM_CLASSES:].values.astype(np.float32)

X = torch.tensor(X_np)
y = torch.tensor(y_onehot_np)

print("y ",y[0:10])
print("y ",y.size())
print("X ",X.size())

# Create dataset and split into training/testing
# dataset = TensorDataset(X, y)
train_dataset = TensorDataset(X, y)

# # train_size = int(len(dataset))
# train_size = int(0.8 * len(dataset))
# test_size = len(dataset) - train_size
# train_dataset, test_dataset = random_split(dataset, [train_size, test_size])

train_loader = DataLoader(train_dataset, batch_size=args.BATCH_SIZE, shuffle=True)
test_loader = DataLoader(test_dataset, batch_size=args.BATCH_SIZE, shuffle=False)

# Configurar el modelo
model = args.MulticlassClassifier(input_dim=args.INPUT_DIM, num_classes=args.NUM_CLASSES)

criterion = nn.CrossEntropyLoss()
optimizer = torch.optim.Adam(model.parameters(), lr=args.LEARNING_RATE)

# Inicializar listas para almacenar las metricas
train_tracker, test_tracker, accuracy_tracker = [], [], []

# Preparar variables para la evaluacion
model.eval()
y_true = []
y_pred = []
log_vars_all = []

time.sleep(1) # 1 segundo

for epoch in range(args.NUM_EPOCHS):
    model.train()
    epoch_loss = 0
    for batchIdx, (batch_x, batch_y) in enumerate(train_loader):
        # Entrenar el modelo con el lote actual 
        if args.PRINT_PAQUETS : print(f"\n=======> ENTRENANDO: batch {batchIdx+1}/{len(train_loader)}\n") 
        optimizer.zero_grad()
        logits, log_vars = model(batch_x)
        loss = criterion(logits, batch_y)
        loss.backward()
        optimizer.step()
        epoch_loss += loss.item()
        
        
        with torch.no_grad():
            if batchIdx < maxBatchsSlaves:
                capas = model.getCapas()
                
                # Enviar las capas del modelo a los esclavos
                if args.PRINT_PAQUETS : print(f"\n=======> EMPEZANDO A ENVIAR CAPAS A ESCLAVOS\n")  
                for capaIdx, capa in enumerate(capas):
                    # Obtener los pesos y el bias de la capa
                    original_weights_matrix = np.matrix(capa.weight.data.cpu().numpy(), dtype=np.float32)
                    original_bias_matrix = np.matrix(capa.bias.data.cpu().numpy(), dtype=np.float32)
                    
                    # Enviar los pesos de la capa
                    if args.PRINT_PAQUETS : print(f"\n=======> ENVIANDO CAPA A ESCLAVOS: capa {capaIdx+1}/{len(capas)}\n")  
                    servidor.sendLayer(original_weights_matrix, capaIdx)
                    
                    if args.PRINT_PAQUETS : time.sleep(0.001) #para correcta impresion
                    
                    # Enviar el bias de la capa a los esclavos
                    if args.PRINT_PAQUETS : print(f"\n=======> ENVIANDO BIAS A ESCLAVOS: capa {capaIdx+1}/{len(capas)}\n")  
                    servidor.sendLayer(original_bias_matrix, capaIdx + 50)
                    
                    if args.PRINT_PAQUETS : time.sleep(0.001)
                
                # Recibir capas con la media de los esclavos
                if args.PRINT_PAQUETS : print(f"\n=======> EMPEZANDO A RECIBIR CAPAS Y BIAS DE ESCLAVOS\n")  
                for capaIdx, capa in enumerate(capas):

                    # Recibir los pesos promediados
                    if args.PRINT_PAQUETS : print(f"\n=======> RECIBIENDO CAPA DE ESCLAVOS: capa {capaIdx+1}/{len(capas)}\n") 
                    
                    avg_weights_matrix = servidor.receiveAvgLayer(capaIdx, args.NUM_ESCLAVOS)
                    
                    # Actualizar los pesos de la capa
                    if args.PRINT_PAQUETS :print("\n=======> INSERTANDO CAPAS\n") 
                    capa.weight.data.copy_(torch.from_numpy(np.asarray(avg_weights_matrix)))
                    
                    # Recibir el bias promediado enviado por los esclavos
                    if args.PRINT_PAQUETS : print(f"\n=======> RECIBIENDO BIAS DE ESCLAVOS: capa {capaIdx+1}/{len(capas)}\n") 
                    
                    avg_bias_matrix = servidor.receiveAvgLayer(capaIdx + 50, args.NUM_ESCLAVOS)
                    
                    # Actualizar el bias de la capa
                    if args.PRINT_PAQUETS : print("\n=======> INSERTANDO BIAS\n") 
                    capa.bias.data.copy_(torch.from_numpy(np.asarray(avg_bias_matrix)).view(-1))
            else:
                # Continuar entrenando unicamente el maestro si ya no quedan lotes en los esclavos
                print(f"\nBATCH SLAVE END. TRAIN ONLY MASTER BATCH IDX: {batchIdx}\n") 
        
        
    train_tracker.append(epoch_loss / len(train_loader))
    print(f"Epoch {epoch+1}/{args.NUM_EPOCHS}, Loss: {train_tracker[-1]:.4f} | ",end="")
    
#with torch.no_grad():
    test_loss = 0
    total = 0
    num_correct = 0
    for batch_x, batch_y in test_loader:
        logits, log_vars = model(batch_x)
        loss = criterion(logits, batch_y)
        test_loss += loss.item()

        predictions = torch.argmax(logits, dim=1)
        total += batch_x.size(0)
        num_correct += (predictions == torch.argmax(batch_y, dim=1)).sum().item()

        predictions = torch.argmax(logits, dim=1)
        y_true.extend(torch.argmax(batch_y, dim=1))
        y_pred.extend(predictions.tolist())
        log_vars_all.append(log_vars)

    test_tracker.append(test_loss/len(test_loader))
    print(f"Test loss: {test_loss/len(test_loader)} | ", end='')
    accuracy_tracker.append(num_correct/total)
    print(f'Accuracy : {num_correct/total}')





# Graficar la perdida de entrenamiento por epoca
plt.figure(figsize=(8, 4))
plt.plot(train_tracker, marker='o')
plt.title("Training Loss Over Epochs")
plt.xlabel("Epoch")
plt.ylabel("Loss")
plt.grid(True)
plt.tight_layout()
plt.show()

import matplotlib.pyplot as plt
# %matplotlib inline
plt.plot(train_tracker, label='Training loss')
plt.plot(test_tracker, label='Test loss')
plt.plot(accuracy_tracker, label='Test accuracy')
plt.legend()


# Mostrar la matriz de confusion
cm = confusion_matrix(y_true, y_pred)
disp = ConfusionMatrixDisplay(confusion_matrix=cm, display_labels=list(range(args.NUM_CLASSES)))
disp.plot(cmap=plt.cm.Blues)
plt.title("Confusion Matrix")
plt.tight_layout()
plt.show()

# Mostrar el reporte de clasificacion
print("\nClassification Report:")
print(classification_report(y_true, y_pred, digits=3))