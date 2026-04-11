import subprocess


if __name__ == "__main__":
    subprocess.run(
        [r"build\msvc2022\Release\Release\EnhanceVision.exe"],
        check=False
    )
