# Changelog

## Unreleased

- First build. The wait and sleep menu is drawn as a round dial inside the game's own menu:
  - the menu's flat panel, drop shadow and straight bar are hidden; the slider still works, unseen;
  - the dial is a parchment astrolabe face with an hour ring and the day's four marks in medallions (sunrise left,
    noon top, sunset right, midnight bottom);
  - the hours you wait are lit along the rim from the current hour, and a gilt sun sits at the current hour;
  - while the game waits, the lit hours tick away from the start of the arc as the sun moves round.
- The game's title stays above the dial, its hours, time and date sit centred on the face (in brown ink), and its
  buttons go below.
- SundialWaitMenu.ini has the size, the gaps, the ink and the log level.
- TestBench tools `sundial.status` and `sundial.drive`.
