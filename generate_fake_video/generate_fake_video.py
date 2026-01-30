#!/usr/bin/env python3
"""
Skrypt generujący prawdziwe pliki wideo (raw RGB) o dokładnie 8 GB.
Generacja jest powtarzalna dzięki ziarnu (seed) – ten sam seed daje ten sam plik.
Zawartość klatek jest deterministyczna (funkcja od ziarna), bez użycia modułu random.
Odtwarzanie: ffplay -f rawvideo -pixel_format rgb24 -s 1920x1080 -i plik.raw
"""

import argparse
import sys
import time
from pathlib import Path


SIZE_8GB = 8 * (1024**3)
DEFAULT_WIDTH = 1920
DEFAULT_HEIGHT = 1080
BYTES_PER_PIXEL = 3  # RGB24
# Stała LCG dla deterministycznego, szybkiego generatora (bez random/hashlib)
_MUL = 2654435761


def _pixel_bytes(seed: int, frame: int, x: int, y: int, width: int, height: int) -> bytes:
    """Deterministyczny kolor piksela (R, G, B) z ziarna – szybka funkcja bez random."""
    base = seed + frame * (width * height * 3) + (y * width + x) * 3
    return bytes((((base + c) * _MUL) & 0xFFFFFFFF) >> 16 & 0xFF for c in range(3))


def generate_fake_video(
    output_path: Path,
    size_bytes: int = SIZE_8GB,
    seed: int = 0,
    width: int = DEFAULT_WIDTH,
    height: int = DEFAULT_HEIGHT,
) -> None:
    """
    Generuje plik wideo raw RGB o dokładnie size_bytes bajtów.
    Zawartość klatek jest deterministyczna na podstawie seeda (bez modułu random).

    Args:
        output_path: Ścieżka do zapisu pliku.
        size_bytes: Docelowy rozmiar w bajtach (np. 8 GB).
        seed: Ziarno dla powtarzalności.
        width: Szerokość klatki w pikselach.
        height: Wysokość klatki w pikselach.
    """
    frame_size = width * height * BYTES_PER_PIXEL
    num_full_frames = size_bytes // frame_size
    payload_size = num_full_frames * frame_size
    padding_size = size_bytes - payload_size
    row_size = width * BYTES_PER_PIXEL

    t0 = time.perf_counter()
    with open(output_path, "wb") as f:
        for frame_idx in range(num_full_frames):
            row_buf = bytearray(row_size)
            for y in range(height):
                for x in range(width):
                    row_buf[x * 3 : x * 3 + 3] = _pixel_bytes(
                        seed, frame_idx, x, y, width, height
                    )
                f.write(row_buf)
            if (frame_idx + 1) % 50 == 0 or frame_idx == num_full_frames - 1:
                written = (frame_idx + 1) * frame_size
                print(f"  Zapisano {written / (1024**3):.2f} GB ({frame_idx + 1} klatek)...")

        if padding_size > 0:
            f.write(b"\x00" * padding_size)

    elapsed = time.perf_counter() - t0
    total = payload_size + padding_size
    gb_written = total / (1024**3)
    throughput = gb_written / elapsed if elapsed > 0 else 0

    print(f"Wygenerowano: {output_path}")
    print(f"  Rozmiar: {total:,} bajtów ({gb_written:.2f} GB)")
    print(f"  Klatki: {num_full_frames} ({width}x{height} RGB)")
    print(f"  Czas generacji: {elapsed:.2f} s ({elapsed / 60:.2f} min)")
    print(f"  Przepustowość: {throughput:.2f} GB/s")
    print(f"  Odtwarzanie: ffplay -f rawvideo -pixel_format rgb24 -s {width}x{height} -i {output_path.name}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generuje pliki wideo raw RGB o rozmiarze 8 GB (powtarzalne dzięki seedowi, bez random)."
    )
    parser.add_argument(
        "output_dir",
        type=Path,
        help="Katalog docelowy do zapisu plików",
    )
    parser.add_argument(
        "-n", "--name",
        type=str,
        default="fake_video.raw",
        help="Nazwa pliku (domyślnie: fake_video.raw)",
    )
    parser.add_argument(
        "-s", "--seed",
        type=int,
        default=0,
        help="Ziarno dla powtarzalności (domyślnie: 0)",
    )
    parser.add_argument(
        "--size-gb",
        type=float,
        default=8.0,
        help="Rozmiar pliku w GB (domyślnie: 8)",
    )
    parser.add_argument(
        "-W", "--width",
        type=int,
        default=DEFAULT_WIDTH,
        help=f"Szerokość klatki w pikselach (domyślnie: {DEFAULT_WIDTH})",
    )
    parser.add_argument(
        "-H", "--height",
        type=int,
        default=DEFAULT_HEIGHT,
        help=f"Wysokość klatki w pikselach (domyślnie: {DEFAULT_HEIGHT})",
    )
    args = parser.parse_args()

    output_dir = args.output_dir.resolve()
    if not output_dir.is_dir():
        print(f"Błąd: Katalog nie istnieje: {output_dir}", file=sys.stderr)
        sys.exit(1)

    output_path = output_dir / args.name
    size_bytes = int(args.size_gb * (1024**3))

    print(f"Ziarno: {args.seed}")
    print(f"Rozmiar docelowy: {size_bytes:,} bajtów ({size_bytes / (1024**3):.2f} GB)")
    print(f"Rozdzielczość: {args.width}x{args.height}")
    print(f"Zapis do: {output_path}")
    print(f"Rozpoczęto: {time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())}")

    generate_fake_video(
        output_path,
        size_bytes=size_bytes,
        seed=args.seed,
        width=args.width,
        height=args.height,
    )


if __name__ == "__main__":
    main()
