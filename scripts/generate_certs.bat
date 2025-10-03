@echo off
setlocal enabledelayedexpansion
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%\..
set SERVER_DIR=%PROJECT_ROOT%\server
set CERT_DIR=%SERVER_DIR%\certs

echo Creating certs directory...
if not exist "%CERT_DIR%" mkdir "%CERT_DIR%"
cd "%CERT_DIR%"

echo Generating OpenSSL extension config files...

REM --- Server extension config ---
> server.cnf echo [server_ext]
>> server.cnf echo subjectKeyIdentifier = hash
>> server.cnf echo authorityKeyIdentifier = keyid,issuer
>> server.cnf echo basicConstraints = CA:FALSE
>> server.cnf echo keyUsage = digitalSignature, keyEncipherment
>> server.cnf echo extendedKeyUsage = serverAuth
>> server.cnf echo subjectAltName = @alt_names
>> server.cnf echo.
>> server.cnf echo [alt_names]
>> server.cnf echo DNS.1 = localhost
>> server.cnf echo IP.1 = 192.168.0.109

REM --- Client extension config ---
> client.cnf echo [client_ext]
>> client.cnf echo subjectKeyIdentifier = hash
>> client.cnf echo authorityKeyIdentifier = keyid,issuer
>> client.cnf echo basicConstraints = CA:FALSE
>> client.cnf echo keyUsage = digitalSignature
>> client.cnf echo extendedKeyUsage = clientAuth

echo Generating CA key and certificate...
openssl genrsa -out ca.key 2048
openssl req -x509 -new -nodes -key ca.key -sha256 -days 1024 -out ca.crt -subj "/CN=MyIoTCA"

echo Generating Server key and CSR...
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr -subj "/CN=192.168.0.109"

echo Signing server certificate with extensions...
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial ^
  -out server.crt -days 500 -sha256 -extfile server.cnf -extensions server_ext

echo Generating Client key and CSR...
openssl genrsa -out client.key 2048
openssl req -new -key client.key -out client.csr -subj "/CN=fastapi-client"

echo Signing client certificate with extensions...
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial ^
  -out client.crt -days 500 -sha256 -extfile client.cnf -extensions client_ext

echo.
echo Certificates generated successfully in "%CD%"
endlocal
pause