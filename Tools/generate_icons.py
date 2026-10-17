"""選定済みPNGをWindows用の複数サイズ入りICOへ変換する。"""

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "LineCalc.png"
SIZES = [(size, size) for size in (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)]


def main():
    with Image.open(SOURCE) as source:
        if source.width != source.height:
            raise ValueError("Icon source must be square")
        image = source.convert("RGBA")
        for filename in ("LineCalc.ico", "small.ico"):
            target = ROOT / "LineCalc" / filename
            image.save(target, format="ICO", sizes=SIZES)
            with Image.open(target) as result:
                if result.ico.sizes() != set(SIZES):
                    raise RuntimeError(f"Icon size verification failed: {target}")
            print(f"Created {target} ({len(SIZES)} sizes)")


if __name__ == "__main__":
    main()
