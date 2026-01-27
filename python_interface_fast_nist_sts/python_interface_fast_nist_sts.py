#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Runner dla programu 'assess' (v6) uruchamianego jako subprocess.

Przykłady:
  1) Test pliku ASCII:
     python run_assess.py --assess ./assess --file data_bits.txt --ascii --length 1000000 --streams 1 --onlymem

  2) Test pliku binarnego:
     python run_assess.py --assess ./assess --file rnd.bin --binary --length 8000000 --streams 8 --fast --defaultpar --fileoutput

  3) Tryb bez pliku (jeśli assess to wspiera w Twojej dystrybucji):
     python run_assess.py --assess ./assess --length 1000000 --streams 10 --tests 111111111111111 --onlymem
"""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Sequence, Dict, Any, Tuple


@dataclass
class AssessResult:
    returncode: int
    stdout: str
    stderr: str
    cmd: Sequence[str]
    parsed: Optional[Dict[str, Any]] = None


def build_assess_cmd(
    assess_path: Path,
    length_bits: int,
    fast: bool = False,
    file_path: Optional[Path] = None,
    streams: Optional[int] = None,
    tests_mask: Optional[str] = None,
    blockfreqpar: Optional[int] = None,
    nonoverpar: Optional[int] = None,
    overpar: Optional[int] = None,
    approxpar: Optional[int] = None,
    serialpar: Optional[int] = None,
    linearpar: Optional[int] = None,
    defaultpar: bool = False,
    fileoutput: bool = False,
    onlymem: bool = False,
    ascii_input: bool = False,
    binary_input: bool = False,
    selftest: bool = False,
) -> list[str]:
    if length_bits <= 0:
        raise ValueError("length_bits musi być > 0")

    if ascii_input and binary_input:
        raise ValueError("Nie można jednocześnie ustawić --ascii i --binary")

    if (ascii_input or binary_input) and file_path is None:
        raise ValueError("--ascii/--binary wymagają podania --file")

    cmd: list[str] = [str(assess_path), str(length_bits)]

    if fast:
        cmd.append("--fast")

    if file_path is not None:
        cmd.extend(["--file", str(file_path)])

    if streams is not None:
        if streams <= 0:
            raise ValueError("--streams musi być > 0")
        cmd.extend(["--streams", str(streams)])

    if tests_mask is not None:
        # Interfejs mówi: np. 111111111111111
        if not all(c in "01" for c in tests_mask) or len(tests_mask) == 0:
            raise ValueError("--tests musi być maską bitową złożoną z 0/1")
        cmd.extend(["--tests", tests_mask])

    # Parametry testów
    if blockfreqpar is not None:
        cmd.extend(["--blockfreqpar", str(blockfreqpar)])
    if nonoverpar is not None:
        cmd.extend(["--nonoverpar", str(nonoverpar)])
    if overpar is not None:
        cmd.extend(["--overpar", str(overpar)])
    if approxpar is not None:
        cmd.extend(["--approxpar", str(approxpar)])
    if serialpar is not None:
        cmd.extend(["--serialpar", str(serialpar)])
    if linearpar is not None:
        cmd.extend(["--linearpar", str(linearpar)])

    if defaultpar:
        cmd.append("--defaultpar")

    if fileoutput:
        cmd.append("--fileoutput")

    if onlymem:
        cmd.append("--onlymem")

    if ascii_input:
        cmd.append("--ascii")
    if binary_input:
        cmd.append("--binary")

    if selftest:
        cmd.append("--selftest")

    return cmd


def parse_onlymem_output(stdout: str) -> Dict[str, Any]:
    """
    Parser "best-effort" dla uproszczonego wyjścia (--onlymem).
    Ponieważ format nie jest podany, robimy bezpieczne heurystyki:
    - zbieramy linie i próbujemy wyłuskać pary klucz: wartość / klucz=wartość.
    - zachowujemy też pełną listę linii.
    """
    result: Dict[str, Any] = {"lines": []}
    kv: Dict[str, str] = {}

    for raw in stdout.splitlines():
        line = raw.strip()
        if not line:
            continue
        result["lines"].append(line)

        # Klucz: wartość
        if ":" in line:
            left, right = line.split(":", 1)
            key = left.strip()
            val = right.strip()
            if key:
                kv[key] = val
                continue

        # Klucz=wartość
        if "=" in line:
            left, right = line.split("=", 1)
            key = left.strip()
            val = right.strip()
            if key:
                kv[key] = val
                continue

    if kv:
        result["kv"] = kv
    return result


def run_assess(cmd: Sequence[str], cwd: Optional[Path] = None, timeout_s: Optional[int] = None) -> AssessResult:
    try:
        completed = subprocess.run(
            list(cmd),
            cwd=str(cwd) if cwd else None,
            capture_output=True,
            text=True,
            timeout=timeout_s,
            check=False,
        )
    except FileNotFoundError as e:
        raise RuntimeError(f"Nie znaleziono programu: {cmd[0]}") from e
    except subprocess.TimeoutExpired as e:
        # Zachowaj częściowe wyjście
        stdout = e.stdout or ""
        stderr = e.stderr or ""
        return AssessResult(returncode=124, stdout=stdout, stderr=stderr + "\nTIMEOUT", cmd=cmd, parsed=None)

    parsed = None
    if "--onlymem" in cmd:
        parsed = parse_onlymem_output(completed.stdout)

    return AssessResult(
        returncode=completed.returncode,
        stdout=completed.stdout,
        stderr=completed.stderr,
        cmd=cmd,
        parsed=parsed,
    )


def run_test_on_8GB_sample(
    filename: str,
    input_dir: Path,
    base_dir: Optional[Path] = None,
    timeout_s: Optional[int] = None,
    print_cmd: bool = True,
) -> AssessResult:
    """
    Uruchamia program assess na dowolnym pliku binarnym o rozmiarze 8 GB.

    Konfiguracja:
    - Program assess: <base_dir>/assess
    - Plik wejściowy: <input_dir>/<filename>
    - Format: binarny
    - Rozmiar: 1 MB na strumień, 8000 strumieni = 8 GB łącznie
    - Liczba strumieni: 8000
    - Wszystkie testy NIST STS

    Args:
        filename: Nazwa pliku wejściowego (np. "des_12345.bin")
        input_dir: Katalog zawierający plik wejściowy
        base_dir: Katalog zawierający program assess (domyślnie katalog bieżący)
        timeout_s: Opcjonalny timeout w sekundach
        print_cmd: Czy wypisać komendę przed uruchomieniem

    Returns:
        AssessResult z wynikami wykonania
    """
    if base_dir is None:
        base_dir = Path.cwd()
    
    # Ścieżki do plików
    # Użyj base_dir bezpośrednio jako katalogu z programem assess
    assess_path = base_dir / "assess"
    
    # Ścieżka do pliku wejściowego: input_dir/filename
    if isinstance(input_dir, str):
        input_dir = Path(input_dir)
    file_path = input_dir.resolve() / filename
    
    # Rozmiar jednego strumienia w bitach
    length_bits = 8 * 1024 * 1024   #  1 MB
    streams = 8000
    
    # Budowanie komendy - wszystkie testy (nie podajemy --tests, co oznacza wszystkie)
    cmd = build_assess_cmd(
        assess_path=assess_path,
        fast=True,
        length_bits=length_bits,
        file_path=file_path,
        streams=streams,
        binary_input=True,
        tests_mask= '111111111111111',
        defaultpar=True,
        fileoutput=True
    )
    
    print(cmd)
    
    if print_cmd:
        print("CMD:", " ".join(shlex.quote(str(c)) for c in cmd), file=sys.stderr)
    
    # Uruchomienie assess
    result = run_assess(cmd, cwd=base_dir if base_dir.exists() else None, timeout_s=timeout_s)
    
    return result


def cli(argv: Optional[Sequence[str]] = None) -> int:
    p = argparse.ArgumentParser(description="Uruchom 'assess' jako subprocess z wygodnym wrapperem.")
    p.add_argument("--assess", required=True, type=Path, help="Ścieżka do wykonywalnego 'assess' (np. ./assess).")

    p.add_argument("--length", required=True, type=int, help="Długość testowanej sekwencji w bitach (argument pozycyjny <Number>).")
    p.add_argument("--fast", action="store_true", help="--fast")
    p.add_argument("--file", type=Path, help="--file <Path>")
    p.add_argument("--streams", type=int, help="--streams <Number>")
    p.add_argument("--tests", type=str, help="--tests <maska 0/1>, np. 111111111111111")

    p.add_argument("--blockfreqpar", type=int, help="--blockfreqpar <Number>")
    p.add_argument("--nonoverpar", type=int, help="--nonoverpar <Number>")
    p.add_argument("--overpar", type=int, help="--overpar <Number>")
    p.add_argument("--approxpar", type=int, help="--approxpar <Number>")
    p.add_argument("--serialpar", type=int, help="--serialpar <Number>")
    p.add_argument("--linearpar", type=int, help="--linearpar <Number>")

    p.add_argument("--defaultpar", action="store_true", help="--defaultpar")
    p.add_argument("--fileoutput", action="store_true", help="--fileoutput")
    p.add_argument("--onlymem", action="store_true", help="--onlymem")
    p.add_argument("--ascii", action="store_true", help="--ascii (wymaga --file)")
    p.add_argument("--binary", action="store_true", help="--binary (wymaga --file)")
    p.add_argument("--selftest", action="store_true", help="--selftest")

    p.add_argument("--cwd", type=Path, help="Katalog roboczy dla procesu assess.")
    p.add_argument("--timeout", type=int, help="Timeout w sekundach (opcjonalnie).")
    p.add_argument("--print-cmd", action="store_true", help="Wypisz komendę przed uruchomieniem.")
    p.add_argument("--json", action="store_true", help="Wypisz wynik w formacie zbliżonym do JSON (stdout/stderr wciąż tekst).")

    args = p.parse_args(argv)

    cmd = build_assess_cmd(
        assess_path=args.assess,
        length_bits=args.length,
        fast=args.fast,
        file_path=args.file,
        streams=args.streams,
        tests_mask=args.tests,
        blockfreqpar=args.blockfreqpar,
        nonoverpar=args.nonoverpar,
        overpar=args.overpar,
        approxpar=args.approxpar,
        serialpar=args.serialpar,
        linearpar=args.linearpar,
        defaultpar=args.defaultpar,
        fileoutput=args.fileoutput,
        onlymem=args.onlymem,
        ascii_input=args.ascii,
        binary_input=args.binary,
        selftest=args.selftest,
    )

    if args.print_cmd:
        print("CMD:", " ".join(shlex.quote(c) for c in cmd), file=sys.stderr)

    res = run_assess(cmd, cwd=args.cwd, timeout_s=args.timeout)

    if args.json:
        # Prosty wydruk struktury; bez zależności od json (żeby nie uciekać w escapowanie megabajtów).
        print("{")
        print(f'  "returncode": {res.returncode},')
        print(f'  "cmd": {list(res.cmd)!r},')
        if res.parsed is not None:
            print(f'  "parsed": {res.parsed!r},')
        print('  "stdout": """')
        print(res.stdout, end="" if res.stdout.endswith("\n") else "\n")
        print('  """,')
        print('  "stderr": """')
        print(res.stderr, end="" if res.stderr.endswith("\n") else "\n")
        print('  """')
        print("}")
    else:
        # Standardowo: stdout na stdout, stderr na stderr
        if res.stdout:
            sys.stdout.write(res.stdout)
        if res.stderr:
            sys.stderr.write(res.stderr)

    return res.returncode


if __name__ == "__main__":
    # Domyślnie uruchamiamy test dla des_12345.bin
    # Aby użyć CLI, uruchom: python python_interface_fast_nist_sts.py --cli [opcje]
    if len(sys.argv) > 1 and sys.argv[1] == "--cli":
        # Tryb CLI z argumentami (pomijamy --cli)
        raise SystemExit(cli(sys.argv[2:]))
    else:
        # Domyślne uruchomienie run_test_on_8GB_sample z plikiem des_12345.bin
        # Można podać nazwę pliku i katalog jako argumenty: 
        # python python_interface_fast_nist_sts.py nazwa_pliku.bin ../data_weak_ciphers/des
        base_dir = Path.cwd()
        filename = sys.argv[1] if len(sys.argv) > 1 else "des_12345.bin"
        input_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else base_dir / "../data_weak_ciphers/des"
        result = run_test_on_8GB_sample(filename, input_dir)
        # Wypisz wyniki
        if result.stdout:
            sys.stdout.write(result.stdout)
        if result.stderr:
            sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)
