"""Convert robsoncouto/arduino-songs sketches to SoundCloc JSON files."""

import json
import re
import sys
from pathlib import Path

SOURCE = "robsoncouto/arduino-songs"
MELODY_PATTERN = re.compile(
    r"(?:const\s+)?(?:PROGMEM\s+)?int\s+melody\[\]"
    r"(?:\s+PROGMEM)?\s*=\s*\{(.*?)\};",
    re.DOTALL,
)


def convert(sketch: Path) -> tuple[str, dict]:
    text = sketch.read_text(encoding="utf-8-sig", errors="replace")
    title_match = re.search(r"/\*\s*\r?\n\s*([^\r\n]+)", text)
    title = title_match.group(1).strip() if title_match else sketch.stem
    tempo = int(re.search(r"\bint\s+tempo\s*=\s*(\d+)", text).group(1))
    definitions = {
        name: int(value)
        for name, value in re.findall(
            r"#define\s+(NOTE_[A-Z0-9]+|REST)\s+(-?\d+)", text
        )
    }
    body = MELODY_PATTERN.search(text).group(1)
    body = re.sub(r"/\*.*?\*/|//[^\r\n]*", "", body, flags=re.DOTALL)
    tokens = [token.strip() for token in body.split(",") if token.strip()]
    notes = [
        definitions.get(tokens[index], int(tokens[index])
                        if re.fullmatch(r"-?\d+", tokens[index]) else 0)
        for index in range(0, len(tokens), 2)
    ]
    durations = [int(tokens[index]) for index in range(1, len(tokens), 2)]
    return title, {
        "name": title,
        "tempo": tempo,
        "notes": notes,
        "durations": durations,
        "source": SOURCE,
    }


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: convert_arduino_songs.py REPOSITORY DATA_DIR")

    repository = Path(sys.argv[1])
    data_dir = Path(sys.argv[2])
    catalog_path = data_dir / "sounds.json"
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    catalog["sounds"] = [
        sound for sound in catalog["sounds"] if sound.get("source") != SOURCE
    ]

    songs_dir = data_dir / "songs"
    songs_dir.mkdir(exist_ok=True)
    converted = []
    for sketch in sorted(repository.rglob("*.ino")):
        title, song = convert(sketch)
        filename = sketch.parent.name.lower() + ".json"
        (songs_dir / filename).write_text(
            json.dumps(song, separators=(",", ":")) + "\n", encoding="utf-8"
        )
        converted.append(
            {"name": title, "file": f"/songs/{filename}", "source": SOURCE}
        )

    catalog["sounds"].extend(converted)
    catalog_path.write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")
    print(f"Converted {len(converted)} songs")


if __name__ == "__main__":
    main()
