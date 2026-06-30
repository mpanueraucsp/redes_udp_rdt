import torch
import torch.nn as nn
import torch.nn.functional as F

# configuracion de la red
UDP_IP = "127.0.0.1"
UDP_PORT = 45000
NUM_ESCLAVOS = 3
PRINT_PAQUETS = True

# configuracion del dataset de diabetes
CSV_PATH = "./Dataset of Diabetes .csv"
INPUT_DIM = 14
NUM_CLASSES = 3
NUM_SAMPLES = 1000

# parametros de la red neuronal
SEED = 42
BATCH_SIZE = 50 # 50 original
NUM_EPOCHS = 5 # 360 original
LEARNING_RATE = 0.001
TIME_WAIT = 2.1
TIME_WAIT_FILE = 0.5

# tamaño de las capas ocultas
HIDDEN1 = 128 # 128 original
HIDDEN2 = 64 # 64 original
HIDDEN3 = 200
HIDDEN4 = 200 

# definicion de la red neuronal / dos capas original
class MulticlassClassifier(nn.Module):
    def __init__(self, input_dim=INPUT_DIM, num_classes=NUM_CLASSES):
        super(MulticlassClassifier, self).__init__()
        self.fc1 = nn.Linear(input_dim, HIDDEN1) # entrada - capa oculta 1
        self.fc2 = nn.Linear(HIDDEN1, HIDDEN2) # capa oculta - capa oculta 2
        # self.fc3 = nn.Linear(HIDDEN2, HIDDEN3)
        # self.fc4 = nn.Linear(HIDDEN3, HIDDEN4)
        
        # capas de salida
        self.class_logits = nn.Linear(HIDDEN2, num_classes) # predice los puntajes de las clases
        self.class_log_vars = nn.Linear(HIDDEN2, num_classes) # predice la incertidumbre de la red

    # forward pass
    def forward(self, x: torch.Tensor):
        x = F.relu(self.fc1(x))
        x = F.relu(self.fc2(x))
        # x = F.relu(self.fc3(x))
        # x = F.relu(self.fc4(x))
        
        logits = self.class_logits(x)
        log_vars = self.class_log_vars(x)
        return logits, log_vars

    # funcion auxiliar para empaquetar las capas en una lista
    def getCapas(self):
        return [self.fc1, 
                self.fc2, 
                # self.fc3, 
                # self.fc4, 
                self.class_logits, 
                self.class_log_vars]