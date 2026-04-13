# Scoreboard Digit Positions

## Physical Layout

```
[ Bat A  ]  [ Total  ]  [ Bat B  ]
[ Target ]  [        ]  [ Wkts   ]
[ DLS    ]  [        ]  [ Overs  ]
```

## Chain & Digit Index Map

| Display | Digits | Chain | Indices |
|---------|--------|-------|---------|
| Bat A   | 3      | 1     | 0, 1, 2 |
| Total   | 3      | 1     | 3, 4, 5 |
| Bat B   | 3      | 2     | 6, 7, 8 |
| Wkts    | 1      | 2     | 9       |
| Overs   | 2      | 2     | 10, 11  |
| Target  | 3      | 3     | 12, 13, 14 |
| DLS     | 3      | 3     | 15, 16, 17 |

## Chain Pins

| Chain | SRCK | SERIN | RCK |
|-------|------|-------|-----|
| 1     | D2   | D3    | D4  |
| 2     | D5   | D6    | D7  |
| 3     | D8   | D9    | D10 |

## Useful Serial Commands (57600 baud, # terminated)

| Command | Description |
|---------|-------------|
| `alltest#` | All 18 digits show 8 |
| `walk#` | Steps an 8 through each digit (2s each), prints index |
| `digit,9,5#` | Set digit 9 (Wkts) to show 5 |
| `clear#` | Turn all digits off |
| `status#` | Print last known score values |
| `help#` | Print command reference |

## Score Command Format

```
4,<batA(3)>,<total(3)>,<batB(3)>,<target(3)>,<wkts(1)>,<overs(2)>,<dls(3)>#
```

Example: `4,--5,123,--8,--0,3,12,--0#`

Use `-` for blank/leading blank digits.
