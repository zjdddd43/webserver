## 生成秘钥和数字证书
openssl genrsa -out privkey.pem 2048

openssl req -new -x509 -key privkey.pem -out cacert.pem -days 1095 
