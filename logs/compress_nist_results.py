#!/usr/bin/env python3
"""
Kompresja wyników testów statystycznych NIST STS z katalogów blowfish, cast, des, rc4.

Parsuje pliki finalAnalysisReport.txt z każdego przebiegu testowego i wyciąga
istotne informacje: p-value (uniformity), p-value (KS), proportion oraz
status pass/fail dla każdego testu. Wyniki zapisuje w jednym pliku JSON.
"""

import json
import os
import re
import sys
from pathlib import Path


LOGS_DIR = Path(__file__).parent
CIPHERS = ["blowfish", "cast", "des", "rc4"]

# Lista wszystkich testów NIST STS (w kolejności raportowania)
NIST_TESTS = [
    "Frequency",
    "BlockFrequency",
    "CumulativeSums",
    "Runs",
    "LongestRun",
    "Rank",
    "FFT",
    "NonOverlappingTemplate",
    "OverlappingTemplate",
    "Universal",
    "ApproximateEntropy",
    "RandomExcursions",
    "RandomExcursionsVariant",
    "Serial",
    "LinearComplexity",
]


def parse_final_analysis_report(filepath: Path) -> dict:
    """
    Parsuje plik finalAnalysisReport.txt i wyciąga wyniki dla każdego testu.

    Zwraca dict z kluczami:
      - generator: ścieżka do pliku generatora
      - sample_size: rozmiar próby (liczba sekwencji binarnych)
      - sample_size_random_excursion: rozmiar próby dla testu random excursion
      - min_pass_rate: minimalny próg proporcji przechodzących sekwencji
      - min_pass_rate_random_excursion: próg dla random excursion
      - tests: lista wyników testów
    """
    with open(filepath, "r") as f:
        content = f.read()

    result = {
        "generator": None,
        "sample_size": None,
        "sample_size_random_excursion": None,
        "min_pass_rate": None,
        "min_pass_rate_random_excursion": None,
        "tests": [],
    }

    # Wyciągnij ścieżkę generatora
    gen_match = re.search(r"generator is <(.+?)>", content)
    if gen_match:
        result["generator"] = gen_match.group(1)

    # Wyciągnij minimalny pass rate i sample size
    rate_match = re.search(
        r"minimum pass rate.*?approximately\s*=?\s*([\d.]+)\s*for a\s+sample size\s*=\s*(\d+)",
        content,
        re.DOTALL,
    )
    if rate_match:
        result["min_pass_rate"] = float(rate_match.group(1))
        result["sample_size"] = int(rate_match.group(2))

    rate_re_match = re.search(
        r"minimum pass rate for the random excursion.*?approximately\s*=?\s*([\d.]+)\s*for a sample size\s*=\s*(\d+)",
        content,
        re.DOTALL,
    )
    if rate_re_match:
        result["min_pass_rate_random_excursion"] = float(rate_re_match.group(1))
        result["sample_size_random_excursion"] = int(rate_re_match.group(2))

    # Parsuj wiersze z wynikami testów
    # Format: C1 C2 ... C10  P-VALUE  P-value(KS) PROPORTION  TEST_NAME
    # Wartości mogą być liczbami lub "----", mogą mieć gwiazdkę "*" oznaczającą niepowodzenie
    lines = content.split("\n")

    for line in lines:
        line = line.rstrip()
        if not line:
            continue

        # Pomiń linie nagłówkowe i separatory
        if line.startswith("-") or line.startswith("=") or "RESULTS FOR" in line:
            continue
        if "generator is" in line:
            continue
        if "C1" in line and "C2" in line and "STATISTICAL TEST" in line:
            continue
        if "minimum pass rate" in line:
            continue
        if "sample size" in line:
            continue
        if "further guidelines" in line or "MAPLE" in line or "addendum" in line:
            continue

        # Sprawdź, czy wiersz zawiera nazwę testu NIST
        test_name = None
        for tn in NIST_TESTS:
            if line.rstrip().endswith(tn):
                test_name = tn
                break

        if test_name is None:
            continue

        # Wyciągnij dane z wiersza
        test_entry = parse_test_line(line, test_name)
        if test_entry:
            result["tests"].append(test_entry)

    return result


def parse_test_line(line: str, test_name: str) -> dict:
    """
    Parsuje pojedynczy wiersz wyników testu z finalAnalysisReport.

    Zwraca dict z:
      - test_name: nazwa testu
      - c1..c10: rozkład p-wartości w 10 przedziałach
      - p_value_uniformity: p-wartość testu uniformity (chi-kwadrat)
      - p_value_uniformity_passed: bool
      - p_value_ks: p-wartość testu Kołmogorowa-Smirnowa
      - p_value_ks_passed: bool
      - proportion: proporcja przechodzących sekwencji
      - proportion_passed: bool
      - data_available: czy test zwrócił dane (nie "----")
    """
    # Usuń nazwę testu z końca
    data_part = line[: line.rfind(test_name)].rstrip()

    # Sprawdź, czy dane są dostępne
    if "----" in data_part and all(
        c in "- 0\t\n" for c in data_part.replace("----", "")
    ):
        # Test nie zwrócił danych (np. "0 0 0 0 0 0 0 0 0 0  ----  ----  ----")
        # Sprawdzamy C1-C10 — czy wszystkie zera
        nums = re.findall(r"\d+", data_part.split("----")[0])
        c_values = [int(x) for x in nums] if nums else [0] * 10
        while len(c_values) < 10:
            c_values.append(0)

        if sum(c_values[:10]) == 0:
            return {
                "test_name": test_name,
                "c1_c10": [0] * 10,
                "p_value_uniformity": None,
                "p_value_uniformity_passed": None,
                "p_value_ks": None,
                "p_value_ks_passed": None,
                "proportion": None,
                "proportion_passed": None,
                "data_available": False,
            }

    # Parsowanie z gwiazdkami — trudniejszy format
    # Strategia: wyciągnij tokeny, rozpoznaj gwiazdki
    # Wzorzec: <10 intów> <float> [*] <float> [*] <float> [*]

    # Zamień gwiazdki na specjalne tokeny
    # Wzorzec: liczba ewentualnie z gwiazdką
    tokens = data_part.split()

    if len(tokens) < 13:
        # Za mało tokenów — nie da się sparsować
        return None

    try:
        # Pierwsze 10 tokenów to C1-C10
        c_values = []
        idx = 0
        for i in range(10):
            c_values.append(int(tokens[idx]))
            idx += 1

        # Następne tokeny: p_value [*] p_value_ks [*] proportion [*]
        remaining = tokens[idx:]

        p_val_uni = None
        p_val_uni_passed = True
        p_val_ks = None
        p_val_ks_passed = True
        proportion = None
        proportion_passed = True

        r_idx = 0

        # P-VALUE uniformity
        p_val_uni = float(remaining[r_idx])
        r_idx += 1
        if r_idx < len(remaining) and remaining[r_idx] == "*":
            p_val_uni_passed = False
            r_idx += 1

        # P-value(KS)
        p_val_ks = float(remaining[r_idx])
        r_idx += 1
        if r_idx < len(remaining) and remaining[r_idx] == "*":
            p_val_ks_passed = False
            r_idx += 1

        # PROPORTION
        proportion = float(remaining[r_idx])
        r_idx += 1
        if r_idx < len(remaining) and remaining[r_idx] == "*":
            proportion_passed = False

        return {
            "test_name": test_name,
            "c1_c10": c_values,
            "p_value_uniformity": p_val_uni,
            "p_value_uniformity_passed": p_val_uni_passed,
            "p_value_ks": p_val_ks,
            "p_value_ks_passed": p_val_ks_passed,
            "proportion": proportion,
            "proportion_passed": proportion_passed,
            "data_available": True,
        }

    except (ValueError, IndexError):
        return None


def parse_freq_file(filepath: Path) -> dict:
    """
    Parsuje plik freq.txt — wyciąga statystyki bitowe.

    Zwraca dict z:
      - alpha: poziom istotności
      - sequences: lista dict z bitsread, zeros, ones
    """
    result = {"alpha": None, "sequences": []}

    if not filepath.exists():
        return result

    with open(filepath, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            # Wyciągnij ALPHA
            alpha_match = re.search(r"ALPHA\s*=\s*([\d.]+)", line)
            if alpha_match and result["alpha"] is None:
                result["alpha"] = float(alpha_match.group(1))

            # Wyciągnij BITSREAD, 0s, 1s
            bits_match = re.search(
                r"BITSREAD\s*=\s*(\d+)\s+0s\s*=\s*(\d+)\s+1s\s*=\s*(\d+)", line
            )
            if bits_match:
                result["sequences"].append(
                    {
                        "bitsread": int(bits_match.group(1)),
                        "zeros": int(bits_match.group(2)),
                        "ones": int(bits_match.group(3)),
                    }
                )

    return result


def extract_run_info(run_dir_name: str) -> dict:
    """
    Wyciąga informacje z nazwy katalogu przebiegu.

    Przykłady:
      encrypted_blowfish_12345 -> cipher=blowfish, source=encrypted, seed=12345
      blowfish_from_text_12350 -> cipher=blowfish, source=from_text, seed=12350
    """
    # Wzorzec: encrypted_<cipher>_<seed>
    m = re.match(r"encrypted_(\w+?)_(\d+)$", run_dir_name)
    if m:
        return {
            "cipher": m.group(1),
            "source_type": "encrypted",
            "seed": int(m.group(2)),
        }

    # Wzorzec: <cipher>_from_text_<seed>
    m = re.match(r"(\w+?)_from_text_(\d+)$", run_dir_name)
    if m:
        return {
            "cipher": m.group(1),
            "source_type": "from_text",
            "seed": int(m.group(2)),
        }

    return {"cipher": None, "source_type": None, "seed": None}


def compute_summary(tests: list) -> dict:
    """
    Wylicza podsumowanie dla listy wyników testów.

    Zwraca:
      - total_tests: łączna liczba wierszy testów (z danymi)
      - tests_passed_all: liczba testów, które przeszły wszystkie kryteria
      - tests_failed_any: liczba testów, które nie przeszły przynajmniej jednego kryterium
      - tests_no_data: liczba testów bez danych
      - pass_rate: stosunek przechodzących do wszystkich (z danymi)
      - per_test_summary: podsumowanie per test (zagregowane po nazwie)
    """
    total = len(tests)
    with_data = [t for t in tests if t["data_available"]]
    no_data = [t for t in tests if not t["data_available"]]

    passed_all = 0
    failed_any = 0

    for t in with_data:
        all_pass = (
            t["p_value_uniformity_passed"]
            and t["p_value_ks_passed"]
            and t["proportion_passed"]
        )
        if all_pass:
            passed_all += 1
        else:
            failed_any += 1

    # Podsumowanie per test (zagregowane po nazwie)
    per_test = {}
    for t in tests:
        name = t["test_name"]
        if name not in per_test:
            per_test[name] = {
                "count": 0,
                "with_data": 0,
                "passed_all_criteria": 0,
                "failed_uniformity": 0,
                "failed_ks": 0,
                "failed_proportion": 0,
                "avg_proportion": 0.0,
                "avg_p_value_ks": 0.0,
            }
        per_test[name]["count"] += 1

        if t["data_available"]:
            per_test[name]["with_data"] += 1

            all_pass = (
                t["p_value_uniformity_passed"]
                and t["p_value_ks_passed"]
                and t["proportion_passed"]
            )
            if all_pass:
                per_test[name]["passed_all_criteria"] += 1
            if not t["p_value_uniformity_passed"]:
                per_test[name]["failed_uniformity"] += 1
            if not t["p_value_ks_passed"]:
                per_test[name]["failed_ks"] += 1
            if not t["proportion_passed"]:
                per_test[name]["failed_proportion"] += 1

            if t["proportion"] is not None:
                per_test[name]["avg_proportion"] += t["proportion"]
            if t["p_value_ks"] is not None:
                per_test[name]["avg_p_value_ks"] += t["p_value_ks"]

    # Oblicz średnie
    for name, data in per_test.items():
        if data["with_data"] > 0:
            data["avg_proportion"] = round(
                data["avg_proportion"] / data["with_data"], 6
            )
            data["avg_p_value_ks"] = round(
                data["avg_p_value_ks"] / data["with_data"], 6
            )

    return {
        "total_test_rows": total,
        "rows_with_data": len(with_data),
        "rows_no_data": len(no_data),
        "rows_passed_all_criteria": passed_all,
        "rows_failed_any_criterion": failed_any,
        "pass_rate": round(passed_all / len(with_data), 6) if with_data else None,
        "per_test_summary": per_test,
    }


def process_run_directory(run_path: Path) -> dict:
    """Przetwarza jeden katalog przebiegu testowego."""
    run_name = run_path.name
    run_info = extract_run_info(run_name)

    report_path = run_path / "finalAnalysisReport.txt"
    freq_path = run_path / "freq.txt"

    if not report_path.exists():
        return None

    report = parse_final_analysis_report(report_path)
    freq = parse_freq_file(freq_path)

    # Oblicz podsumowanie statystyk bitowych
    freq_summary = None
    if freq["sequences"]:
        total_zeros = sum(s["zeros"] for s in freq["sequences"])
        total_ones = sum(s["ones"] for s in freq["sequences"])
        total_bits = total_zeros + total_ones
        freq_summary = {
            "alpha": freq["alpha"],
            "num_sequences": len(freq["sequences"]),
            "bits_per_sequence": freq["sequences"][0]["bitsread"] if freq["sequences"] else None,
            "total_bits": total_bits,
            "total_zeros": total_zeros,
            "total_ones": total_ones,
            "zero_ratio": round(total_zeros / total_bits, 6) if total_bits > 0 else None,
        }

    summary = compute_summary(report["tests"])

    return {
        "run_name": run_name,
        "cipher": run_info["cipher"],
        "source_type": run_info["source_type"],
        "seed": run_info["seed"],
        "generator": report["generator"],
        "sample_size": report["sample_size"],
        "sample_size_random_excursion": report["sample_size_random_excursion"],
        "min_pass_rate": report["min_pass_rate"],
        "min_pass_rate_random_excursion": report["min_pass_rate_random_excursion"],
        "bit_statistics": freq_summary,
        "summary": summary,
        "tests": report["tests"],
    }


def main():
    all_results = {
        "description": "Skompresowane wyniki testów statystycznych NIST STS",
        "ciphers": CIPHERS,
        "nist_tests": NIST_TESTS,
        "metadata": {
            "source_directory": str(LOGS_DIR),
            "total_runs": 0,
            "runs_per_cipher": {},
        },
        "runs": [],
    }

    total_runs = 0

    for cipher in CIPHERS:
        cipher_dir = LOGS_DIR / cipher
        if not cipher_dir.is_dir():
            print(f"UWAGA: Katalog {cipher_dir} nie istnieje, pomijam.")
            continue

        cipher_runs = 0

        # Posortowane katalogi przebiegów
        run_dirs = sorted(
            [d for d in cipher_dir.iterdir() if d.is_dir()],
            key=lambda x: x.name,
        )

        for run_dir in run_dirs:
            print(f"Przetwarzanie: {cipher}/{run_dir.name}")
            run_data = process_run_directory(run_dir)
            if run_data:
                all_results["runs"].append(run_data)
                cipher_runs += 1
                total_runs += 1

        all_results["metadata"]["runs_per_cipher"][cipher] = cipher_runs
        print(f"  -> {cipher}: {cipher_runs} przebiegów")

    all_results["metadata"]["total_runs"] = total_runs

    # Oblicz globalne podsumowanie per szyfr
    global_summary = {}
    for cipher in CIPHERS:
        cipher_runs = [r for r in all_results["runs"] if r["cipher"] == cipher]
        if not cipher_runs:
            continue

        # Podział na encrypted vs from_text
        for source_type in ["encrypted", "from_text"]:
            source_runs = [r for r in cipher_runs if r["source_type"] == source_type]
            if not source_runs:
                continue

            key = f"{cipher}_{source_type}"
            all_tests_combined = []
            for run in source_runs:
                all_tests_combined.extend(run["tests"])

            global_summary[key] = {
                "cipher": cipher,
                "source_type": source_type,
                "num_runs": len(source_runs),
                "summary": compute_summary(all_tests_combined),
            }

    all_results["global_summary"] = global_summary

    # Zapisz do JSON
    output_path = LOGS_DIR / "nist_results_compressed.json"
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(all_results, f, indent=2, ensure_ascii=False)

    print(f"\nZapisano wyniki do: {output_path}")
    print(f"Łącznie przebiegów: {total_runs}")

    # Pokaż krótkie podsumowanie
    print("\n=== PODSUMOWANIE GLOBALNE ===")
    for key, gs in global_summary.items():
        s = gs["summary"]
        print(
            f"  {key}: "
            f"{s['rows_passed_all_criteria']}/{s['rows_with_data']} wierszy testów przeszło "
            f"(pass_rate={s['pass_rate']})"
        )

    return output_path


if __name__ == "__main__":
    main()
