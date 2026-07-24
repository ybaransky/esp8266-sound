# Sound Format

Sounds live in `/sounds.json` on the device's LittleFS filesystem and are parsed
by `play()` in [src/sound_player.cpp](src/sound_player.cpp). If the file is
missing at boot, a default catalog is created automatically.

## Structure

The file is a JSON object with a `sounds` array. Each entry is one sound:

```json
{
  "sounds": [
    { "name": "Doorbell", "notes": [659, 523], "durations": [4, 2] }
  ]
}
```

## Fields

| Field       | Required | Description                                                        |
| ----------- | -------- | ------------------------------------------------------------------ |
| `name`      | yes      | Label shown in the dropdown and used to select/play the sound.     |
| `notes`     | yes      | Array of frequencies in Hz. `0` is a rest (silence).               |
| `durations` | yes      | Array parallel to `notes`. Each value is a note-length divisor.    |
| `tempo`     | no       | Beats per minute. Its presence switches the timing mode (see below). |

### notes

Frequencies in Hz. `440` is A4, `262` is middle C, and so on. A value of `0`
plays nothing but still consumes its time slot, which is how a beep-rest-beep
pattern is made. Note-name constants are available in
[src/pitches.h](src/pitches.h).

### durations

Each entry is a **divisor of a whole note**, so a larger number is a shorter
note:

| Value | Note          |
| ----- | ------------- |
| `1`   | whole note    |
| `2`   | half note     |
| `4`   | quarter note  |
| `8`   | eighth note   |
| `16`  | sixteenth note |

`notes` and `durations` are matched up index by index, and playback stops at the
end of the shorter array. Keep them the same length.

## Timing modes

The presence of `tempo` selects one of two modes.

### Simple mode (no `tempo`)

Used by all the default sounds. Duration in milliseconds is `1000 / divisor`,
independent of any musical tempo:

- `4` (quarter) = 250 ms
- `8` (eighth) = 125 ms
- `2` (half) = 500 ms

Each note is followed by a gap of 30% of its length.

```json
{ "name": "Alert", "notes": [880, 0, 880, 0, 880], "durations": [8, 16, 8, 16, 8] }
```

### Musical mode (`tempo` present, > 0)

A whole note lasts `(60000 * 4) / tempo` ms, and each note is
`wholeNote / divisor`. Each note sounds for 90% of its slot with a 10% gap.

In this mode a **negative divisor is a dotted note** (1.5x the length):

- `-4` = dotted quarter note
- `-8` = dotted eighth note

```json
{
  "name": "March",
  "tempo": 120,
  "notes":     [392, 392, 392, 311, 466],
  "durations": [-8,  16,  4,   4,   4]
}
```

## Defaults and edge cases

- A missing or `0` divisor defaults to a quarter note (`4`).
- A negative divisor is a dotted note only in musical mode; in simple mode it is
  treated as a quarter note.
- A `notes`/`durations` length mismatch is safe: playback simply ends at the
  shorter array.

## Adding a sound

Append an object to the `sounds` array with a `name` and equal-length `notes`
and `durations` arrays. Add `tempo` only if you want BPM-based timing and dotted
notes.

You can edit `/sounds.json` on the device directly, or change the `DEFAULT_SOUNDS`
catalog in [src/sound_player.cpp](src/sound_player.cpp) and re-upload (delete the
existing `/sounds.json` first, since the default is only written when the file is
missing).
