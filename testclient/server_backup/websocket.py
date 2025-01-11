import asyncio
import websockets
import ssl

# 配置部分
WEBSOCKET_HOST = "0.0.0.0"  # 监听所有网络接口
WEBSOCKET_PORT = 2443  # WebSocket服务器端口


# SSL/TLS 配置
SSL_CERT_PATH = "/home/qnwang/worknew/cert/fullchain.pem"  # 替换为您的证书路径
SSL_KEY_PATH = "/home/qnwang/worknew/cert/privkey.pem"  # 替换为您的私钥路径

# 存储所有连接的客户端
connected_clients = set()


async def register(websocket):
    connected_clients.add(websocket)
    print(f"客户端已连接：{websocket.remote_address}")


async def unregister(websocket):
    connected_clients.remove(websocket)
    print(f"客户端已断开：{websocket.remote_address}")


async def send_to_clients(message):
    if connected_clients:  # 确保有客户端连接
        await asyncio.gather(*[client.send(message) for client in connected_clients])


# 接受函数
async def websocket_handler(websocket, path):
    await register(websocket)
    try:
        async for message in websocket:
            print(f"收到来自客户端的消息：{message}")
            # 这里可以处理来自客户端的消息（如果需要）
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        await unregister(websocket)


# def monitor_file(loop):
#     inotify = INotify()
#     watch_flags = flags.MODIFY
#     wd = inotify.add_watch(FILE_PATH, watch_flags)

#     while True:
#         for event in inotify.read():
#             if flags.MODIFY in event:  # 直接检查 event 是否包含 MODIFY
#                 try:
#                     with open(FILE_PATH, "r") as f:
#                         content = f.read()

#                     # 获取当前时间，并按照指定格式附加到内容末尾
#                     current_time = datetime.now().strftime("time1:%H.%M.%S.%f")[:-3]
#                     content_with_time = f"{content.strip()}|{current_time}|"
#                     # 将内容推送给所有连接的客户端
#                     asyncio.run_coroutine_threadsafe(
#                         send_to_clients(content_with_time), loop
#                     )
#                 except Exception as e:
#                     print(f"读取文件时出错：{e}")


async def main():
    # 创建 SSL 上下文
    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ssl_context.load_cert_chain(certfile=SSL_CERT_PATH, keyfile=SSL_KEY_PATH)

    # 启动WebSocket服务器，启用 SSL
    server = await websockets.serve(
        websocket_handler, WEBSOCKET_HOST, WEBSOCKET_PORT, ssl=ssl_context
    )
    print(f"WebSocket服务器已启动,监听 {WEBSOCKET_HOST}:{WEBSOCKET_PORT} (WSS)")

    # 运行 WebSocket 服务器
    await server.wait_closed()


# 运行服务器
if __name__ == "__main__":
    asyncio.run(main())
