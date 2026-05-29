"""
preprocess.py — Convert games.csv to a compact binary format (optional).
"""

import struct
import csv
import pathlib

DATA_DIR = pathlib.Path(__file__).parent.parent / "data"
SRC_CSV  = DATA_DIR / "games.csv"
DST_BIN  = DATA_DIR / "games.bin"


def convert(src: pathlib.Path, dst: pathlib.Path) -> None:
    # TODO: read CSV, write each row as fixed-width binary record
    with src.open() as f:
        reader = csv.DictReader(f)
        with dst.open("wb") as out:
            for row in reader:
                # stub: pack 10 champion IDs + winner as 11 × int16
                pass
    print(f"Written {dst}")


if __name__ == "__main__":
    convert(SRC_CSV, DST_BIN)
