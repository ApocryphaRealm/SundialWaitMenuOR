#pragma once

// ============================================================================================================
// The dial, inside the game's own wait / sleep menu (WBP_ModernMenu_SleepWait_C). The menu keeps its title, its texts,
// its hours slider and its buttons, so its input is the game's own; this hides only its flat panel art (the drop shadow,
// the background tile's picture, the slider's bar) and lays the dial into the background tile, under the texts:
//
//   title                      (the game's, above the dial)
//   [ the dial: the face, the rim's hours from now on, the four marks, the sun at the current hour ]
//     hours / time / date      (the game's texts, centred on the face, in brown ink)
//   Continue   Cancel          (the game's buttons, below the dial)
//
// The menu's view model (VSleepWaitMenuViewModel) and widgets were read in game on 2026-10-01 (the primary session's
// captures): TimeToWait is the hours chosen once the wait is confirmed and then counts down one per real second to 0;
// before that the hours are the slider's. Game thread only, from the frame tick.
// ============================================================================================================

namespace dial
{
	void Tick();
	json State();

	enum class Action
	{
		kReapply,       // build and lay out again on the next tick
		kReloadArt,     // import the PNGs again (after tools/gen-art.py)
	};
	void Queue(Action a_action);
	void PreviewHours(int a_hours);   // -1: the menu's own hours; 0-24: show that many on the rim (testing)
}
