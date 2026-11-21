import os
import re
from pathlib import Path
import glob
import sys
import subprocess
import time
from multiprocessing import Pool

SUCCEEDED = "\033[32msucceeded\033[0m"
FAILED = "\033[31mfailed\033[0m"

build_format = '| {:32} | {:18} | {:5} | {:6} | {:6} |'
build_separator = '-' * 74


def build_board(target):
    board, profile = target
    start_time = time.monotonic()
    make_result = subprocess.run(["make", "-j", "BOARD={}".format(board), "GFSK_DFU_ENABLED=1", f"GDFU_PROFILE={profile}", "all"],
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT)
    if make_result.returncode == 0:
        make_result = subprocess.run(
            ["make", f"BOARD={board}", "GFSK_DFU_ENABLED=1", f"GDFU_PROFILE={profile}", "copy-artifact"],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    build_duration = time.monotonic() - start_time

    flash_size = "-"
    sram_size = "-"
    succeeded = 0

    if make_result.returncode == 0:
        succeeded = 1
        out_file = glob.glob(f'_build/build-{board}-gfsk-profile{profile}-*/*.out')[0]
        size_output = subprocess.run('size {}'.format(out_file), shell=True, stdout=subprocess.PIPE).stdout.decode(
            "utf-8")
        size_list = size_output.split('\n')[1].split('\t')
        flash_size = int(size_list[0])
        sram_size = int(size_list[1]) + int(size_list[2])

    print(build_format.format(f'{board}_{433 if profile == 1 else 868}', SUCCEEDED if succeeded else FAILED, "{:.2f}s".format(build_duration), flash_size,
                              sram_size))

    if make_result.returncode != 0:
        print(make_result.stdout.decode("utf-8"))

    return succeeded


if __name__ == '__main__':
    # remove build folder first
    subprocess.run("rm -rf _build/", shell=True)

    # Only boards that have the GFSK profile (opt-in in board.mk)
    all_boards = []
    for entry in os.scandir("src/boards"):
        board_mk = Path(entry.path, "board.mk")
        # Same rule as the CI matrix grep (.github/workflows/githubci.yml)
        if board_mk.is_file() and re.search(r"^\s*CFLAGS\s*\+=\s*-DGFSK_DFU_ENABLED=1\s*$", board_mk.read_text(), re.M):
            all_boards.append(entry.name)
    all_boards.sort()

    print(build_separator)
    print(build_format.format('Board', '\033[39mResult\033[0m', 'Time', 'Flash', 'SRAM'))
    print(build_separator)

    success_count = 0
    total_time = time.monotonic()

    targets = [(board, profile) for board in all_boards for profile in (1, 2)]
    with Pool(processes=os.cpu_count()) as pool:
        success_count = sum(pool.map(build_board, targets))

    total_time = time.monotonic() - total_time
    fail_count = len(targets) - success_count

    # Build Summary
    print(build_separator)
    print(
        "Build Summary: {} {}, {} {} and took {:.2f}s".format(success_count, SUCCEEDED, fail_count, FAILED, total_time))
    print(build_separator)

    sys.exit(fail_count)
