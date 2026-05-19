import os
import socket
import time

PORT = int(os.environ.get('CHAT_SERVER_TEST_PORT', '19091'))

def send_frame(s, payload):
    data = len(payload).to_bytes(4, 'big') + payload.encode('utf-8')
    s.sendall(data)

def recv_frame(s, timeout=1.0):
    s.settimeout(timeout)
    try:
        header = s.recv(4)
        if not header or len(header) < 4:
            return None
        length = int.from_bytes(header, 'big')
        body = b''
        while len(body) < length:
            chunk = s.recv(length - len(body))
            if not chunk:
                return None
            body += chunk
        return body.decode('utf-8')
    except socket.timeout:
        return None

if __name__ == '__main__':
    s1 = socket.create_connection(('127.0.0.1', PORT))
    s2 = socket.create_connection(('127.0.0.1', PORT))
    send_frame(s1, 'CHAT|1|JOIN|100|alice')
    send_frame(s2, 'CHAT|1|JOIN|100|bob')
    # expect welcome
    print('recv1', recv_frame(s1))
    print('recv2', recv_frame(s2))
    send_frame(s1, 'CHAT|1|MSG|hello')
    time.sleep(0.1)
    print('recv2 after msg', recv_frame(s2))
    s1.close()
    s2.close()
