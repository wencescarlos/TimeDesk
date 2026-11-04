#!/usr/bin/env python3
"""Firma firmware.bin en el formato que verifica el Updater del core ESP8266.

Formato: <binario> + <firma RSA-SHA256> + <longitud de la firma, u32 little-endian>
(igual que tools/signing.py del core esp8266/Arduino).

Uso: python3 tools/sign_firmware.py <clave_privada.pem> <firmware.bin>
El fichero se firma en el sitio.
"""
import struct
import subprocess
import sys


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    key, path = sys.argv[1], sys.argv[2]
    with open(path, "rb") as f:
        data = f.read()
    sig = subprocess.run(["openssl", "dgst", "-sha256", "-sign", key],
                         input=data, capture_output=True, check=True).stdout
    with open(path, "wb") as f:
        f.write(data + sig + struct.pack("<L", len(sig)))
    print(f"Firmado {path}: {len(data)} bytes + firma de {len(sig)} bytes")


if __name__ == "__main__":
    main()
