#pragma once

// The TestBench tools (rule 64): sundial.status reads the dial back; sundial.drive rebuilds it, imports the pictures again
// or shows a number of hours on the rim for a test.
namespace tool
{
	bool Register();   // lazily, once TestBench.dll is loaded (idempotent)
}
