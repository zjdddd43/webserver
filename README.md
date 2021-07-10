## WebServer

### 实现功能： 

• 基于线程池、非阻塞 socket、epoll（ET 模式）、模拟 Proactor 的并发模型。 

• 状态机解析 HTTP 请求报文（GET、POST 请求）。 

• 通过 openssl 生成的密钥和证书加入 SSL。 

• 异步日志系统。 

• 数据库连接查询用户信息。 

• 提供 Web 端用户注册、登陆功能，文件管理、上传功能，m3u8 视频播放功能。



### 运行webserver

./webserver 192.168.3.134 12345 key/cacert.pem key/privkey.pem 