#!/usr/bin/env python3
import asyncio
import argparse
import json
import struct

try:
    import websockets
except Exception:
    print("websockets package missing; run 'pip install websockets'")
    raise

async def handle_ws(websocket, path, server_host, server_port):
    try:
        print('ws client connected, opening tcp to', server_host, server_port)
        reader, writer = await asyncio.open_connection(server_host, server_port)
        print('tcp connection established to', server_host, server_port)
    except Exception as e:
        print('failed to open tcp connection to', server_host, server_port, 'error:', e)
        try:
            await websocket.send(json.dumps({'type': 'error', 'message': f'backend unreachable: {e}'}))
        except Exception:
            pass
        try:
            await websocket.close()
        except Exception:
            pass
        return

    async def ws_to_tcp():
        async for msg in websocket:
            try:
                data = json.loads(msg)
            except Exception:
                continue
            if data.get('type') == 'join':
                room = data.get('room', 0)
                username = data.get('username', 'guest')
                body = f"CHAT|1|JOIN|{room}|{username}"
            elif data.get('type') == 'register':
                username = data.get('username', 'guest')
                password = data.get('password', '')
                body = f"CHAT|1|REGISTER|{username}|{password}"
            elif data.get('type') == 'login':
                username = data.get('username', 'guest')
                password = data.get('password', '')
                body = f"CHAT|1|LOGIN|{username}|{password}"
            elif data.get('type') == 'msg':
                text = data.get('text', '')
                body = f"CHAT|1|MSG|{text}"
            else:
                # unknown action: ignore
                continue
            payload = body.encode('utf-8')
            writer.write(len(payload).to_bytes(4, 'big') + payload)
            await writer.drain()

    async def tcp_to_ws():
        try:
            while True:
                header = await reader.readexactly(4)
                length = int.from_bytes(header, 'big')
                body = await reader.readexactly(length)
                try:
                    await websocket.send(json.dumps({'type':'frame', 'body': body.decode('utf-8', errors='replace')}))
                except Exception:
                    break
        except asyncio.IncompleteReadError:
            pass
        except Exception:
            print('tcp->ws loop error', flush=True)
        try:
            await websocket.close()
        except Exception:
            pass

    # run both tasks until one completes
    t1 = asyncio.create_task(ws_to_tcp())
    t2 = asyncio.create_task(tcp_to_ws())
    done, pending = await asyncio.wait([t1, t2], return_when=asyncio.FIRST_COMPLETED)
    for p in pending:
        p.cancel()
    try:
        writer.close()
        await writer.wait_closed()
    except Exception:
        pass
    print('ws handler finished, connection closed')

async def main():
    parser = argparse.ArgumentParser(description='WebSocket <-> TCP bridge for chat_server')
    parser.add_argument('--server-host', default='127.0.0.1')
    parser.add_argument('--server-port', type=int, default=8080)
    parser.add_argument('--ws-host', default='127.0.0.1')
    parser.add_argument('--ws-port', type=int, default=8765)
    args = parser.parse_args()

    async def handler(ws, path=None):
        await handle_ws(ws, path, args.server_host, args.server_port)

    print(f"Starting WS bridge on ws://{args.ws_host}:{args.ws_port} -> {args.server_host}:{args.server_port}")
    async with websockets.serve(handler, args.ws_host, args.ws_port):
        await asyncio.Future()  # run forever

if __name__ == '__main__':
    asyncio.run(main())
