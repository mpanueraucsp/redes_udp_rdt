crear env en virtual box
- python3 -m venv ~/tarea
Activar entorno virtual
- source ~/tarea/bin/activate

- pip install -r requirements.txt
instalar tinker
- sudo apt install -y python3-tk



REALIZAR BINDING 
- python setup.py build_ext --inplace
Para re-compilar
- python setup.py build_ext --inplace --force


EJECUTAR EL PROGRAMA
- Ejecuta master.py 
- luego en cada carpeta de cliente c1, c2, c3 ejecutar slave.py
Y listo ya estaria :u
- Si quieres mas slaves asegurate de modificar el globals.py para agregar N slaves

