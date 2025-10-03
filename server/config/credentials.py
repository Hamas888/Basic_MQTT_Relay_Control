# config/credentials.py

import  os
from    dotenv import load_dotenv

load_dotenv("variables.env")                                            # load variables from .env file

ROOT_CA                     = "certs\ca.crt"
CLIENT_CERT                 = "certs\client.crt"
PRIVATE_KEY                 = "certs\client.key"