import socket
import threading
import tkinter as tk
from tkinter import messagebox, scrolledtext

# -------------------------------------------------------------
# CONFIGURACIÓN
# -------------------------------------------------------------

ESP_IP = "192.168.4.1"       # Cambia si el ESP está en otra red
ESP_PORT = 2323              # Puerto del ESP-link (si usas TCP)

USE_TCP = True               # True = WiFi (ESP01) / False = Serial por COM

# -------------------------------------------------------------
# MAPA DE COMANDOS REMOTOS (ADAPTADO A TU STM32)
# -------------------------------------------------------------

CMD_MAP = {
    "FORCE_OPEN:1": b"OPEN\n",
    "FORCE_OPEN:0": b"CLOSE\n",
    "FORCE_LOCKOUT": b"LOCKOUT\n",
    "GET_STATUS": b"STATUS\n",
}

# -------------------------------------------------------------
# CLASE PRINCIPAL DE LA APP
# -------------------------------------------------------------

class DoorControlApp:
    def __init__(self, root):
        self.root = root
        root.title("Room Control Remote Panel")
        root.geometry("650x500")

        self.is_connected = False
        self.client_socket = None

        # ---------------------------
        # BOTÓN CONECTAR / DESCONECTAR
        # ---------------------------

        self.btn_connect = tk.Button(root, text="Conectar", bg="#CCFFCC",
                                     command=self.toggle_connection, width=20)
        self.btn_connect.pack(pady=10)

        # ---------------------------
        # LOGS DEL SISTEMA
        # ---------------------------

        self.log_display = scrolledtext.ScrolledText(root, width=80, height=20)
        self.log_display.pack(pady=10)

        # ---------------------------
        # BOTONES DE CONTROL
        # ---------------------------

        frame = tk.Frame(root)
        frame.pack()

        tk.Button(frame, text="ABRIR PUERTA", width=20, command=lambda: self.send_cmd("FORCE_OPEN:1")).grid(row=0, column=0, padx=5, pady=5)
        tk.Button(frame, text="CERRAR PUERTA", width=20, command=lambda: self.send_cmd("FORCE_OPEN:0")).grid(row=0, column=1, padx=5, pady=5)
        tk.Button(frame, text="LOCKOUT", width=20, command=lambda: self.send_cmd("FORCE_LOCKOUT")).grid(row=1, column=0, padx=5, pady=5)
        tk.Button(frame, text="ESTADO", width=20, command=lambda: self.send_cmd("GET_STATUS")).grid(row=1, column=1, padx=5, pady=5)

        # ---------------------------
        # CAMBIO DE CLAVE
        # ---------------------------

        self.pass_entry = tk.Entry(root, width=10)
        self.pass_entry.pack()

        self.btn_pass = tk.Button(root, text="Cambiar Clave (4 dígitos)",
                                  command=self.send_new_pass)
        self.btn_pass.pack(pady=10)

        # Cierra limpiamente
        root.protocol("WM_DELETE_WINDOW", self.on_close)

    # -------------------------------------------------------------
    # MANEJO DE CONEXIÓN TCP
    # -------------------------------------------------------------

    def toggle_connection(self):
        if not self.is_connected:
            self.connect_tcp()
        else:
            self.disconnect()

    def connect_tcp(self):
        try:
            self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.client_socket.connect((ESP_IP, ESP_PORT))
            self.is_connected = True

            self.btn_connect.config(text="Desconectar", bg="#FFCCCC")
            self.log("Conectado a ESP01")

            # Hilo para escuchar datos del STM32
            threading.Thread(target=self.listen_thread, daemon=True).start()

        except Exception as e:
            messagebox.showerror("Error", f"No se pudo conectar al ESP01:\n{e}")

    def disconnect(self):
        self.is_connected = False
        try:
            if self.client_socket:
                self.client_socket.close()
        except:
            pass

        self.btn_connect.config(text="Conectar", bg="#CCFFCC")
        self.log("Desconectado")

    # -------------------------------------------------------------
    # ENVÍO DE COMANDOS
    # -------------------------------------------------------------

    def send_cmd(self, cmd):
        if not self.is_connected:
            messagebox.showwarning("Advertencia", "No hay conexión con el ESP01.")
            return

        try:
            if cmd in CMD_MAP:
                self.client_socket.send(CMD_MAP[cmd])
                self.log(f"Enviado: {cmd}")
                return

        except Exception as e:
            messagebox.showerror("Error enviando comando", str(e))
            self.disconnect()

    def send_new_pass(self):
        pw = self.pass_entry.get().strip()

        if len(pw) != 4 or not pw.isdigit():
            messagebox.showerror("Error", "La clave debe ser de 4 dígitos.")
            return

        try:
            packet = f"SET_PASS:{pw}\n".encode()
            self.client_socket.send(packet)
            self.log(f"Clave enviada: {pw}")

        except Exception as e:
            messagebox.showerror("Error enviando clave", str(e))
            self.disconnect()

    # -------------------------------------------------------------
    # RECEPCIÓN DE DATOS
    # -------------------------------------------------------------

    def listen_thread(self):
        while self.is_connected:
            try:
                data = self.client_socket.recv(128)
                if not data:
                    continue

                text = data.decode("utf-8", errors="ignore")
                for line in text.split("\n"):
                    if line.strip():
                        self.log(f"[STM32] {line.strip()}")

            except:
                break

        self.disconnect()

    # -------------------------------------------------------------
    # UTILIDADES
    # -------------------------------------------------------------

    def log(self, msg):
        self.log_display.insert(tk.END, msg + "\n")
        self.log_display.see(tk.END)

    def on_close(self):
        self.disconnect()
        self.root.destroy()


# -------------------------------------------------------------
# EJECUCIÓN DEL PROGRAMA
# -------------------------------------------------------------

if __name__ == "__main__":
    root = tk.Tk()
    app = DoorControlApp(root)
    root.mainloop()
