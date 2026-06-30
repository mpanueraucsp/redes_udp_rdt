# Entrenamiento Distribuido de Redes Neuronales sobre UDP con Transferencia Confiable

## Objetivo

Implementar un sistema de aprendizaje federado (Federated Learning) que distribuye el entrenamiento de una red neuronal clasificadora entre un nodo maestro y múltiples nodos esclavos, comunicándose mediante un protocolo confiable construido sobre UDP (RDT - Reliable Data Transfer).

## Dataset

Se utiliza el dataset "Dataset of Diabetes" con 14 características de entrada y 3 clases de salida (tipos de diabetes), codificadas en one-hot. El dataset se divide en 80% entrenamiento y 20% prueba; la porción de entrenamiento se reparte equitativamente entre el maestro y los esclavos.

## Arquitectura del Sistema

El sistema sigue un modelo **maestro-esclavo** con 1 servidor maestro y N clientes esclavos (por defecto 3). La lógica de machine learning está escrita en Python (PyTorch), mientras que toda la comunicación de red está implementada en C++ y expuesta a Python mediante pybind11.

## Protocolo de Comunicación

Se diseñó un protocolo propio sobre UDP con paquetes de 500 bytes y un header de 24 bytes que contiene: tipo (1B), número de secuencia (4B), checksum (5B), total de paquetes (4B), paquete actual (4B), capa (2B) y tamaño de datos (4B). Los mensajes grandes se fragmentan automáticamente en múltiples paquetes.

## Transferencia Confiable (RDT)

Sobre el protocolo UDP se implementa una capa de confiabilidad que incluye:

- **Checksum** por paquete para detectar corrupción de datos.
- **ACK/NACK** para confirmar o rechazar la recepción de cada paquete.
- **Timeout con reintentos** (hasta 10 intentos) para manejar pérdidas.
- **Detección de duplicados** mediante un buffer circular de números de secuencia.
- **Simulación de anomalías** configurable: corrupción de paquetes, pérdida de paquetes y pérdida de ACK, con control de intervalo, secuencia y límite máximo.

## Flujo de Entrenamiento

En cada batch de cada época se ejecuta el siguiente ciclo:

1. El maestro entrena su batch local (forward, backward, optimizer step).
2. El maestro envía los pesos y bias de todas las capas a cada esclavo (en paralelo, un hilo por esclavo).
3. Cada esclavo recibe los pesos, los carga en su modelo local, entrena su propio batch y devuelve los pesos actualizados.
4. El maestro recibe los pesos de todos los esclavos, calcula el **promedio** por capa y lo aplica a su modelo.

Este proceso sincroniza los modelos en cada iteración, convergiendo hacia una solución global.

## Red Neuronal

Clasificador multiclase implementado en PyTorch con la siguiente estructura: capa de entrada (14 neuronas), dos capas ocultas con ReLU (128 y 64 neuronas), y dos cabezas de salida: logits de clase (3 neuronas) y log-varianza por clase. Se entrena con CrossEntropyLoss y optimizador Adam (lr=0.001).

## Tecnologías

| Componente | Tecnología |
|---|---|
| Machine Learning | Python, PyTorch, scikit-learn, pandas, NumPy |
| Comunicación de red | C++ (sockets UDP, threads, mutex) |
| Integración Python-C++ | pybind11 |
| Visualización | Matplotlib (curvas de pérdida, matriz de confusión) |

## Ejecución

Se compila el binding C++ con `python setup.py build_ext --inplace`, se ejecuta primero `master.py` (que inicia el servidor y espera conexiones), y luego se ejecuta `slave.py` desde cada carpeta de cliente (c1, c2, c3).
