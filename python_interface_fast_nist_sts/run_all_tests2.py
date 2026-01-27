#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Skrypt uruchamiający RÓWNOLEGŁE testy NIST STS dla wielu plików szyfrów.

Uruchamia testy dla:
1. DES: ../data_weak_ciphers/des/des_from_text_12347.bin (instancja fast_nist_sts_instance5)
2. CAST: ../data_weak_ciphers/cast/cast_from_text_12347.bin (instancja fast_nist_sts_instance6)
3. Blowfish: ../data_weak_ciphers/blowfish/blowfish_from_text_12347.bin (instancja fast_nist_sts_instance7)
4. RC4: ../data_weak_ciphers/rc4/rc4_from_text_12347.bin (instancja fast_nist_sts_instance8)

Każdy test działa w osobnym wątku z własną instancją oprogramowania fast_nist_sts.
Wszystkie testy rozpoczynają się równocześnie i działają równolegle.
"""

from __future__ import annotations

import sys
import shutil
import threading
import time
from pathlib import Path
from typing import List, Dict, Any

# Dodaj katalog bieżący do ścieżki, aby móc zaimportować moduł
sys.path.insert(0, str(Path(__file__).parent))

from python_interface_fast_nist_sts import run_test_on_8GB_sample


def copy_experiment_results(source_dir: Path, dest_dir: Path, filename: str) -> bool:
    """
    Kopiuje pliki wynikowe z katalogu eksperymentów do katalogu logów.
    
    Args:
        source_dir: Katalog źródłowy z wynikami (fast_nist_sts/experiments/AlgorithmTesting)
        dest_dir: Katalog docelowy dla logów
        filename: Nazwa pliku testowego (używana do logowania)
    
    Returns:
        True jeśli kopiowanie zakończone pomyślnie, False w przeciwnym razie
    """
    try:
        # Sprawdź czy katalog źródłowy istnieje
        if not source_dir.exists():
            print(f"⚠ Ostrzeżenie: Katalog źródłowy nie istnieje: {source_dir}", file=sys.stderr)
            return False
        
        # Utwórz katalog docelowy jeśli nie istnieje
        dest_dir.mkdir(parents=True, exist_ok=True)
        
        # Kopiuj wszystkie pliki z katalogu źródłowego
        copied_files = []
        for item in source_dir.iterdir():
            if item.is_file():
                dest_file = dest_dir / item.name
                shutil.copy2(item, dest_file)
                copied_files.append(item.name)
            elif item.is_dir():
                # Kopiuj również podkatalogi
                dest_subdir = dest_dir / item.name
                shutil.copytree(item, dest_subdir, dirs_exist_ok=True)
                copied_files.append(f"{item.name}/ (katalog)")
        
        if copied_files:
            print(f"✓ Skopiowano {len(copied_files)} plików/ów do: {dest_dir}", file=sys.stderr)
            return True
        else:
            print(f"⚠ Brak plików do skopiowania w: {source_dir}", file=sys.stderr)
            return False
            
    except Exception as e:
        print(f"✗ Błąd podczas kopiowania wyników dla {filename}: {e}", file=sys.stderr)
        return False


# Konfiguracja dla różnych instancji fast_nist_sts (instancje 5-8)
FAST_NIST_INSTANCES = {
    "des_from_text_12347.bin": {
        "base_dir": Path("/home/paugustynowicz/cipher_recognition/fast_nist_sts_instance5"),
        "experiments_dir": Path("/home/paugustynowicz/cipher_recognition/fast_nist_sts_instance5/experiments/AlgorithmTesting"),
        "log_dir": Path("/home/paugustynowicz/cipher_recognition/logs/des/des_from_text_12347"),
    },
    "cast_from_text_12347.bin": {
        "base_dir": Path("/home/paugustynowicz/cipher_recognition/fast_nist_sts_instance6"),
        "experiments_dir": Path("/home/paugustynowicz/cipher_recognition/fast_nist_sts_instance6/experiments/AlgorithmTesting"),
        "log_dir": Path("/home/paugustynowicz/cipher_recognition/logs/cast/cast_from_text_12347"),
    },
    "rc4_from_text_12347.bin": {
        "base_dir": Path("/home/paugustynowicz/cipher_recognition/fast_nist_sts_instance7"),
        "experiments_dir": Path("/home/paugustynowicz/cipher_recognition/fast_nist_sts_instance7/experiments/AlgorithmTesting"),
        "log_dir": Path("/home/paugustynowicz/cipher_recognition/logs/rc4/rc4_from_text_12347"),
    },
    "blowfish_from_text_12347.bin": {
        "base_dir": Path("/home/paugustynowicz/cipher_recognition/fast_nist_sts_instance8"),
        "experiments_dir": Path("/home/paugustynowicz/cipher_recognition/fast_nist_sts_instance8/experiments/AlgorithmTesting"),
        "log_dir": Path("/home/paugustynowicz/cipher_recognition/logs/blowfish/blowfish_from_text_12347"),
    },
}


def run_single_test(filename: str, input_dir: Path, test_id: int, results: List[Dict[str, Any]]) -> None:
    """
    Uruchamia pojedynczy test w osobnym wątku.

    Args:
        filename: Nazwa pliku testowego
        input_dir: Katalog zawierający plik wejściowy
        test_id: Identyfikator testu (dla logowania)
        results: Lista wyników (współdzielona między wątkami)
    """
    thread_name = threading.current_thread().name
    print(f"\n{'='*80}", file=sys.stderr)
    print(f"Test {test_id}/4 [{thread_name}]: Uruchamianie testu dla: {input_dir}/{filename}", file=sys.stderr)
    print(f"{'='*80}", file=sys.stderr)
    print(f"RÓWNOLEGŁE URUCHAMIANIE - Test {test_id} działa w tle...\n", file=sys.stderr)

    try:
        # Pobierz konfigurację dla tej instancji
        if filename not in FAST_NIST_INSTANCES:
            raise ValueError(f"Brak konfiguracji dla pliku: {filename}")

        config = FAST_NIST_INSTANCES[filename]
        base_dir = config["base_dir"]
        experiments_dir = config["experiments_dir"]

        # Uruchom test z określoną instancją fast_nist_sts
        result = run_test_on_8GB_sample(
            filename=filename,
            input_dir=input_dir,
            base_dir=base_dir,
            print_cmd=True,
        )

        # Zapisz wynik
        results.append({
            "filename": filename,
            "input_dir": str(input_dir),
            "base_dir": str(base_dir),
            "returncode": result.returncode,
            "success": result.returncode == 0,
            "thread": thread_name,
        })

        # Wypisz wyniki dla tego testu
        if result.stdout:
            sys.stdout.write(f"=== STDOUT dla {filename} (wątek: {thread_name}) ===\n")
            sys.stdout.write(result.stdout)
            if not result.stdout.endswith("\n"):
                sys.stdout.write("\n")

        if result.stderr:
            sys.stderr.write(f"=== STDERR dla {filename} (wątek: {thread_name}) ===\n")
            sys.stderr.write(result.stderr)
            if not result.stderr.endswith("\n"):
                sys.stderr.write("\n")

        if result.returncode == 0:
            print(f"\n✓ Test dla {filename} zakończony pomyślnie (wątek: {thread_name})\n", file=sys.stderr)
        else:
            print(f"\n✗ Test dla {filename} zakończony z kodem błędu: {result.returncode} (wątek: {thread_name})\n", file=sys.stderr)

        # Kopiuj wyniki eksperymentów do katalogu logów
        log_dir = config["log_dir"]
        print(f"Kopiowanie wyników eksperymentów dla {filename} (wątek: {thread_name})...", file=sys.stderr)
        copy_success = copy_experiment_results(experiments_dir, log_dir, filename)
        if not copy_success:
            print(f"⚠ Ostrzeżenie: Nie udało się skopiować wyników dla {filename} (wątek: {thread_name})", file=sys.stderr)

    except Exception as e:
        print(f"\n✗ Błąd podczas uruchamiania testu dla {filename} (wątek: {thread_name}): {e}\n", file=sys.stderr)
        results.append({
            "filename": filename,
            "input_dir": str(input_dir),
            "returncode": -1,
            "success": False,
            "error": str(e),
            "thread": thread_name,
        })

        # Próba skopiowania wyników nawet po błędzie
        if filename in FAST_NIST_INSTANCES:
            config = FAST_NIST_INSTANCES[filename]
            experiments_dir = config["experiments_dir"]
            log_dir = config["log_dir"]
            print(f"Próba kopiowania wyników eksperymentów dla {filename} pomimo błędu (wątek: {thread_name})...", file=sys.stderr)
            copy_experiment_results(experiments_dir, log_dir, filename)


def main() -> int:
    """
    Uruchamia testy dla wszystkich zdefiniowanych plików równolegle.

    UWAGA: Testy są uruchamiane RÓWNOLEGLE (wszystkie naraz) w osobnych wątkach.
    Każdy test używa innej instancji oprogramowania fast_nist_sts (instancje 5-8).
    """

    print(f"\n{'='*100}", file=sys.stderr)
    print("URUCHAMIANIE RÓWNOLEGŁYCH TESTÓW NIST STS (INSTANCJE 5-8)", file=sys.stderr)
    print(f"{'='*100}", file=sys.stderr)
    print("Każdy test będzie działał w osobnym wątku z własną instancją fast_nist_sts", file=sys.stderr)
    print("Wszystkie 4 testy rozpoczną się równocześnie!\n", file=sys.stderr)

    # Lista testów do wykonania: (nazwa_pliku, katalog)
    tests = [
        ("des_from_text_12347.bin", Path("../data_weak_ciphers/des")),
        ("cast_from_text_12347.bin", Path("../data_weak_ciphers/cast")),
        ("blowfish_from_text_12347.bin", Path("../data_weak_ciphers/blowfish")),
        ("rc4_from_text_12347.bin", Path("../data_weak_ciphers/rc4")),
    ]

    # Lista wyników współdzielona między wątkami
    results = []
    threads = []

    # Uruchom wszystkie testy równolegle
    for idx, (filename, input_dir) in enumerate(tests, 1):
        thread_name = f"Test-{idx}-{filename.split('.')[0]}"
        thread = threading.Thread(
            target=run_single_test,
            args=(filename, input_dir, idx, results),
            name=thread_name
        )
        threads.append(thread)
        thread.start()
        print(f"✓ Uruchomiono wątek {thread_name} dla testu {idx}/4", file=sys.stderr)

    print(f"\n{'='*50} WSZYSTKIE TESTY URUCHOMIONE {'='*50}", file=sys.stderr)
    print("Czekam na zakończenie wszystkich testów...\n", file=sys.stderr)

    # Czekaj na zakończenie wszystkich wątków
    start_time = time.time()
    for thread in threads:
        thread.join()

    end_time = time.time()
    duration = end_time - start_time

    # Podsumowanie
    print(f"\n{'='*100}", file=sys.stderr)
    print("PODSUMOWANIE RÓWNOLEGŁYCH TESTÓW (INSTANCJE 5-8)", file=sys.stderr)
    print(f"{'='*100}", file=sys.stderr)
    print(f"Czas wykonania: {duration:.2f} sekund", file=sys.stderr)

    successful = sum(1 for r in results if r["success"])
    total = len(results)

    for r in results:
        status = "✓" if r["success"] else "✗"
        thread_info = r.get("thread", "unknown")
        base_dir_info = r.get("base_dir", "unknown")
        print(f"{status} {r['filename']} (wątek: {thread_info}, instancja: {base_dir_info}) - kod: {r['returncode']}", file=sys.stderr)

    print(f"\nZakończono: {successful}/{total} testów pomyślnie", file=sys.stderr)
    print(f"Czas całkowity: {duration:.2f} sekund", file=sys.stderr)

    # Zwróć kod błędu jeśli którykolwiek test się nie powiódł
    return 0 if successful == total else 1


if __name__ == "__main__":
    raise SystemExit(main())
