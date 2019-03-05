import socket
import time

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    rate = 10.1
    s.setblocking(1)
    s.bind(("localhost", 9787))
    s.listen()
    conn, addr = s.accept()
    with conn:
        print('Connected by', addr)
        while True:
            data = conn.recv(1024).decode()

            if not data:
                break
            print("Got data: %s" % data)
            conn.send(str(rate).encode())
