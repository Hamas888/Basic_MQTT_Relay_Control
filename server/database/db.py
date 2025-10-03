# server/database/db.py

import os
from sqlmodel import SQLModel, Session, create_engine

# Get the absolute path to the database file
current_dir = os.path.dirname(os.path.abspath(__file__))
db_path = os.path.join(current_dir, "devices.db")
DATABASE_URL = f"sqlite:///{db_path}"

engine = create_engine(DATABASE_URL, echo=False)

def init_db():
    # Ensure the database directory exists
    os.makedirs(os.path.dirname(db_path), exist_ok=True)
    SQLModel.metadata.create_all(engine)

def get_session():
    return Session(engine)
