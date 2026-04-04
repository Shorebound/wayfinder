# User Commands

## Prompt after finishing work
Critically evaluate this pr. Grade it on modularity (if applicable), production-readiness, architecture, extensibility (if applicable), and developer experience/usability. Compare it to other game engines like Bevy, Unreal, Godot, etc. Provide feedback and suggestions for changes, improvements, complete revamps, etc. Explain which are useful to do now and which should be new issues.

Make sure you adhere to the copilot-instructions. Do not be sycophantic, be honest and direct. We want a future-proofed, elegantly designed engine.

## v2
Critically evaluate the code in this branch/pr. Grade it on modularity (if applicable), production-readiness, architecture, extensibility (if applicable), and developer experience/usability. Compare it to other game engines like Bevy, Unreal, Godot, Spartan, RavEngine, carimbo, etc. Provide feedback and suggestions for changes, improvements, complete revamps, etc. Do not worry about legacy code, deprecations or migrations. We want to make sure we get it right now rather than later.

Make sure you adhere to the copilot-instructions. Do not be sycophantic, be honest and direct. 


## Prompt for sub-issues
Some part of this has been implemented. Critically evaluate your scope. Grade it on modularity (if applicable), production-readiness, architecture, extensibility (if applicable), and developer experience/usability. Compare it to other game engines like Bevy, Unreal, Godot, etc. If there are any changes, improvements or revamps that are valuable to do, make a plan and implement them.

Make sure you adhere to pillars and engineering standards set out in the copilot instructions. Do not be sycophantic, be honest and direct. We want a future-proofed, elegantly designed engine.

## Code Review
Here are some code review comments from coderabbit. Verify each finding against the current code and only fix it if needed. If something does not have an obvious solution, take the time to think it through. We do not want band-aid fixes that kick the can down the road.

## GSD Research
Yes, research.

Investigate domain, patterns and dependencies before planning. 

We are trying to make clean, elegant and idiomatic modern C++23 code. So let's make sure we research the idioms and patterns relevant to this phase - we want to use the latest and best features where they genuinely improve the code. As well as the best practices for the specific architectural patterns we're implementing. 

Also remember this is game engine, research how other engines approach this problem. 

Some engine examples:
- Oxylus ([text](https://github.com/oxylusengine/Oxylus)) and RavEngine ([text](https://github.com/ravengine/ravengine)) which are both similar to our stack
- Bevy which we share a lot of architectural ideas with
- Unreal which we also take some architectural ideas from
- O3DE which may be a good example of a mature solution with modern C++ conventions
- Spartan and Wicked which are smaller, but apparently clean with well-developed renders.

We're taking inspiration from their architecture and design decisions, not their code style. Our code should be cleaner than all of them.
