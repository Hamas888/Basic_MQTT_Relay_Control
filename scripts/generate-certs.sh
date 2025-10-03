#!/bin/bash
set -e

CERT_DIR="../server/certs"
mkdir -p "$CERT_DIR"
cd "$CERT_DIR"

echo "Generating OpenSSL extension config files..."

cat > server.cnf <<EOF
[server_ext]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid,issuer
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = localhost
DNS.2 = 192.168.0.109
EOF

cat > client.cnf <<EOF
[client_ext]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid,issuer
basicConstraints = CA:FALSE
keyUsage = digitalSignature
extendedKeyUsage = clientAuth
EOF

echo "Generating CA key and certificate..."
openssl genrsa -out ca.key 2048
openssl req -x509 -new -nodes -key ca.key -sha256 -days 1024 -out ca.crt -subj "/CN=MyIoTCA"

echo "Generating Server key and CSR..."
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr -subj "/CN=192.168.0.109"

echo "Signing server certificate with extensions..."
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -days 500 -sha256 -extfile server.cnf -extensions server_ext

echo "Generating Client key and CSR..."
openssl genrsa -out client.key 2048
openssl req -new -key client.key -out client.csr -subj "/CN=fastapi-client"

echo "Signing client certificate with extensions..."
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out client.crt -days 500 -sha256 -extfile client.cnf -extensions client_ext

echo "Certificates generated successfully in '$CERT_DIR'"