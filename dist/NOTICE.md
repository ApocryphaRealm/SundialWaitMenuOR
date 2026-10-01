# Sundial Wait Menu - copyright and licence

Copyright (C) 2026 ApocryphaRealm

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program (`LICENSE`). If not, see
<https://www.gnu.org/licenses/>.

SPDX-License-Identifier: GPL-3.0-or-later

## Why GPL-3.0-or-later

The plugin statically links CommonLibOB64 (https://github.com/libxse/commonlibob64, commit
3e5ee4fcf96593125339d0035e92be9280eb3acd) and its commonlib-shared library (https://github.com/libxse/commonlib-shared,
commit 9fbb74d628134ab4ea3a3cf5c0ed3f7eeabbd01d), both GPL-3.0. The modding exception that comes with the plugin
template is kept in the source repository as `EXCEPTIONS`.

The dial's pictures (Face.png, Hour.png, Marks.png, Sun.png) are original. They are drawn by the repository's
`tools/gen-art.py` and are under the same licence. The menu itself, its texts and its controls are the game's own, used
at run time; no game file is replaced.

Components under other licences, with their notices: `THIRD_PARTY_NOTICES.md`.
